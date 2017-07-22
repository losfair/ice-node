const core = require("./build/Release/ice_node_core");
module.exports.static = require("./static.js");

module.exports.Ice = Ice;
function Ice(cfg) {
    if (!cfg) cfg = {};

    this.server = null;
    this.routes = [];
    this.middlewares = [];
    this.templates = {};
    this.config = {
        session_timeout_ms: cfg.session_timeout_ms || 600000,
        session_cookie: cfg.session_cookie || "ICE_SESSION_ID",
        max_request_body_size: cfg.max_request_body_size || null
    };
}

Ice.prototype.route = function (methods, p, handler) {
    if (typeof (methods) == "string") methods = [methods];
    if (!(methods instanceof Array)) throw new Error("The first parameter to `route` must be a string or array of strings.");
    if (typeof (p) != "string") throw new Error("Path must be a string");
    if (typeof (handler) != "function") throw new Error("Handler must be a function");

    methods = methods.map(v => v.toUpperCase());

    let mm = {};
    for (const v of methods) {
        mm[v] = true;
    }

    let param_mappings = p.split("/").filter(v => v).map(v => v.startsWith(":") ? v.substr(1) : null);

    let self = this;

    this.routes.push({
        path: p,
        methods: methods,
        param_mappings: param_mappings,
        handler: function (req) {
            if (!mm[req.method]) {
                return new Response({
                    status: 405,
                    body: "Method not allowed"
                }).send(self, req.call_info);
            }

            let ok = r => {
                if (r instanceof Response) {
                    r.send(self, req.call_info);
                } else {
                    try {
                        new Response({
                            body: r
                        }).send(self, req.call_info);
                    } catch (e) {
                        err(e);
                    }
                }
            };
            let err = e => {
                console.log(e);
                new Response({
                    status: 500,
                    body: "Internal error"
                }).send(self, req.call_info);
            };
 
            try {
                let r = handler(req);
                if (r && r.then) {
                    r.then(ok, err);
                } else {
                    ok(r);
                }
            } catch (e) {
                err(e);
            }
        }
    });
}

Ice.prototype.get = function (p, handler) {
    return this.route(["HEAD", "GET"], p, handler);
}

Ice.prototype.post = function (p, handler) {
    return this.route("POST", p, handler);
}

Ice.prototype.put = function (p, handler) {
    return this.route("PUT", p, handler);
}

Ice.prototype.delete = function (p, handler) {
    return this.route("DELETE", p, handler);
}

Ice.prototype.use = function (p, handler) {
    if (typeof (p) != "string") throw new Error("Prefix must be a string");
    if (typeof (handler) != "function" && !(handler instanceof Flag)) throw new Error("Handler must be a function or flag");

    this.middlewares.push({
        prefix: p,
        handler: handler
    });
}

Ice.prototype.add_template = function (name, content) {
    if (typeof (name) != "string") throw new Error("Name must be a string");
    if (typeof (content) != "string") throw new Error("Content must be a string");

    this.templates[name] = content;
}

Ice.prototype.listen = function (addr) {
    if (typeof (addr) != "string") throw new Error("Address must be a string");
    if (this.server !== null) {
        throw new Error("Already listening");
    }

    this.server = core.create_server();
    core.set_session_timeout_ms(this.server, this.config.session_timeout_ms);
    core.set_session_cookie_name(this.server, this.config.session_cookie);
    if(this.config.max_request_body_size) {
        core.set_max_request_body_size(this.server, this.config.max_request_body_size);
    }

    for (const k in this.templates) {
        try {
            core.add_template(this.server, k, this.templates[k]);
        } catch (e) {
            console.log("Warning: Unable to add template: " + k);
            delete this.templates[k];
        }
    }

    let self = this;

    for (const rt of this.routes) {
        let flags = [];
        if (rt.methods.indexOf("POST") != -1 || rt.methods.indexOf("PUT") != -1) {
            flags.push("read_body");
        }

        let mws = this.middlewares.filter(v => rt.path.startsWith(v.prefix));
        let flag_mws = mws.filter(v => v.handler instanceof Flag);
        mws = mws.filter(v => typeof (v.handler) == "function");

        for (const f of flag_mws) {
            flags.push(f.handler.name);
        }

        core.add_endpoint(this.server, rt.path, call_info => {
            // Why setImmediate ?
            setImmediate(async () => {
                let req = new Request(self, rt, call_info);
                for (const mw of mws) {
                    try {
                        await mw.handler(req, mw);
                    } catch (e) {
                        if (e instanceof Response) {
                            e.send(self, call_info);
                        } else {
                            console.log(e);
                            new Response({
                                status: 500,
                                body: "Internal error"
                            }).send(self, call_info);
                        }
                        return;
                    }
                }
                rt.handler(req);
            });
        }, flags);
    }
    core.add_endpoint(this.server, "", call_info => setImmediate(() => this.not_found_handler(call_info)));

    core.listen(this.server, addr);
};

Ice.prototype.not_found_handler = async function (call_info) {
    let req = new Request(this, null, call_info);
    for (const mw of this.middlewares.filter(v => typeof(v.handler) == "function")) {
        if(req.url.startsWith(mw.prefix)) {
            try {
                await mw.handler(req, mw);
            } catch (e) {
                if (e instanceof Response) {
                    e.send(this, call_info);
                } else {
                    console.log(e);
                    new Response({
                        status: 500,
                        body: "Internal error"
                    }).send(this, call_info);
                }
                return;
            }
        }
    }

    return new Response({
        status: 404,
        body: "Not found"
    }).send(this, call_info);
}

module.exports.Request = Request;
function Request(server, route, call_info) {
    this.server = server;
    this.route = route;
    this.call_info = call_info;

    let req_info = core.get_request_info(call_info);
    this.headers = req_info.headers;
    this.uri = req_info.uri;
    this.url = req_info.uri.split("?")[0];
    this.remote_addr = req_info.remote_addr;
    this.method = req_info.method;

    this.host = req_info.headers.host;

    this.session = new Proxy({}, {
        get: (t, k) => core.get_request_session_item(this.call_info, k),
        set: (t, k, v) => core.set_request_session_item(this.call_info, k, v)
    });

    this._cookies = {};
    this._params = null;

    let self = this;

    this.cookies = new Proxy({}, {
        get: (t, k) => {
            if (!self._cookies[k]) {
                self._cookies[k] = core.get_request_cookie(self.call_info, k);
            }
            return self._cookies[k];
        }
    });

    this.params = new Proxy({}, {
        get: (t, k) => {
            if (!self._params) {
                try {
                    let params = {};
                    self.url.split("/").filter(v => v).map((v, index) => [self.route.param_mappings[index], v]).forEach(p => {
                        if (p[0]) {
                            params[p[0]] = p[1];
                        }
                    });
                    self._params = params;
                } catch (e) {
                    self._params = {};
                }
            }
            return self._params[k];
        }
    });
}

Request.prototype.body = function () {
    return core.get_request_body(this.call_info);
}

Request.prototype.json = function () {
    let body = this.body();
    if (!body) return null;
    try {
        return JSON.parse(body.toString());
    } catch (e) {
        throw new Error("Request body is not valid JSON");
    }
}

Request.prototype.form = function () {
    let body = this.body();
    if (!body) return null;

    let form = {};
    try {
        body.toString().split("&").filter(v => v).map(v => v.split("=")).forEach(p => form[p[0]] = p[1]);
        return form;
    } catch (e) {
        throw new Error("Request body is not valid urlencoded form");
    }
}

Request.prototype.get_stats = function() {
    return JSON.parse(core.get_stats_from_request(this.call_info));
}

module.exports.Response = Response;
function Response({ status = 200, headers = {}, cookies = {}, body = "", file = null, template_name = null, template_params = {} }) {
    // Do strict checks here because errors in Response.send() may cause memory leak & deadlock.

    if (typeof (status) != "number" || status < 100 || status >= 600) {
        throw new Error("Invalid status");
    }
    this.status = Math.floor(status);

    if (!headers || typeof (headers) != "object") {
        throw new Error("Invalid headers");
    }
    let transformed_headers = {};
    for (const k in headers) {
        transformed_headers[k] = "" + headers[k];
    }
    this.headers = transformed_headers;

    let transformed_cookies = {};
    for (const k in cookies) {
        transformed_cookies[k] = "" + cookies[k];
    }
    this.cookies = transformed_cookies;

    this.body = null;
    this.template_name = null;
    this.file = null;

    if (body) {
        if (body instanceof Buffer) {
            this.body = body;
        } else {
            this.body = Buffer.from(body);
        }
        if (!this.body) throw new Error("Invalid body");
    } else if (typeof (template_name) == "string") {
        this.template_name = template_name;
        if (!template_params || typeof (template_params) != "object") {
            throw new Error("Invalid template params");
        }
        this.template_params = template_params;
    } else if (typeof (file) == "string") {
        this.file = file;
    } else {
        throw new Error("No valid body or template provided");
    }

    Object.freeze(this);
}

Response.prototype.send = function (server, call_info) {
    if (!(server instanceof Ice)) {
        console.log(server);
        throw new Error("Expecting a server instance. Got it.");
    }

    let resp = core.create_response();
    core.set_response_status(resp, this.status);
    for (const k in this.headers) {
        core.set_response_header(resp, k, this.headers[k]);
    }
    for (const k in this.cookies) {
        core.set_response_cookie(resp, k, this.cookies[k]);
    }
    if (this.body) {
        core.set_response_body(resp, this.body);
    } else if (this.template_name) {
        try {
            core.render_template(call_info, resp, this.template_name, JSON.stringify(this.template_params));
        } catch (e) {
            console.log(e);
        }
    } else if (this.file) {
        core.set_response_file(resp, this.file);
    }
    core.fire_callback(call_info, resp);
};

Response.json = function (data) {
    return new Response({
        headers: {
            "Content-Type": "application/json"
        },
        body: JSON.stringify(data)
    });
}

Response.file = function(path) {
    return new Response({
        file: path
    });
}

module.exports.Flag = Flag;
function Flag(name) {
    if (typeof (name) != "string") {
        throw new Error("Flag name must be a string");
    }
    this.name = name;
    Object.freeze(this);
}
