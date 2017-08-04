const core = require("./build/Release/ice_node_core");
/*const core = new Proxy({}, {
    get: (t, k) => {
        console.log(k);
        return _core[k].bind(_core);
    }
});*/

const stat = require("./stat.js");
module.exports.static = require("./static.js");

module.exports.Ice = Ice;

/**
 * Creates an Ice-node Application.
 * @constructor
 * @param {object} cfg - The configuration to use
 */
function Ice(cfg) {
    if(!(this instanceof Ice)) {
        return new Ice(...arguments);
    }

    if (!cfg) cfg = {};

    this.routes = [];
    this.middlewares = [];
    this.templates = {};
    this.config = {
        disable_request_logging: cfg.disable_request_logging || false,
        session_timeout_ms: cfg.session_timeout_ms || 600000,
        session_cookie: cfg.session_cookie || "ICE_SESSION_ID",
        max_request_body_size: cfg.max_request_body_size || null,
        endpoint_timeout_ms: cfg.endpoint_timeout_ms === undefined ? null : cfg.endpoint_timeout_ms
    };
    this.inst = new core.Server(this.config);
}

/**
 * Adds a route to the application.
 * @param {string[]} methods - Allowed methods for the route
 * @param {string} p - The target path for the route. 
 * Parameters should start with `:`.
 * Examples:
 * - /user/login
 * - /user/profile/:uid
 * @param {function} handler - Called when a request hits the route, with a parameter of type Request.
 */
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

/**
 * An alias of `route`, with `methods` set to ["GET"].
 * @param {string} p - The target path for the route.
 * @param {function} handler - Called when a request hits the route, with a parameter of type Request.
 */
Ice.prototype.get = function (p, handler) {
    return this.route(["HEAD", "GET"], p, handler);
}

/**
 * An alias of `route`, with `methods` set to ["POST"].
 * @param {string} p - The target path for the route.
 * @param {function} handler - Called when a request hits the route, with a parameter of type Request.
 */
Ice.prototype.post = function (p, handler) {
    return this.route("POST", p, handler);
}

/**
 * An alias of `route`, with `methods` set to ["PUT"].
 * @param {string} p - The target path for the route.
 * @param {function} handler - Called when a request hits the route, with a parameter of type Request.
 */
Ice.prototype.put = function (p, handler) {
    return this.route("PUT", p, handler);
}

/**
 * An alias of `route`, with `methods` set to ["Delete"].
 * @param {string} p - The target path for the route.
 * @param {function} handler - Called when a request hits the route, with a parameter of type Request.
 */
Ice.prototype.delete = function (p, handler) {
    return this.route("DELETE", p, handler);
}

/**
 * Adds a middleware to the application.
 * @param {string} p - Path (prefix) for the middleware
 * @param {function} handler - Called when a request hits an endpoint with the specified prefix on its path.
 */
Ice.prototype.use = function (p, handler) {
    if (typeof (p) != "string") throw new Error("Prefix must be a string");
    if (typeof (handler) != "function" && !(handler instanceof Flag)) throw new Error("Handler must be a function or flag");

    this.middlewares.push({
        prefix: p,
        handler: handler
    });
}

/**
 * Adds a template to the application.
 * @param {string} name - Name of the template. Should end in the corresponding format.
 * Examples:
 * - base.html
 * @param {string} content - Content of the template.
 * The template engine is based on Tera (https://github.com/Keats/tera), with a Jinja2-like syntax.
 */
Ice.prototype.add_template = function (name, content) {
    if (typeof (name) != "string") throw new Error("Name must be a string");
    if (typeof (content) != "string") throw new Error("Content must be a string");

    this.templates[name] = content;
}

/**
 * Starts the server and listen on the specified address.
 * @param {string} addr - The address to listen on.
 * Examples:
 * - 127.0.0.1:9812
 * - 0.0.0.0:8080
 */
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
    if(this.config.disable_request_logging) {
        core.disable_request_logging(this.server);
    }
    if(this.config.endpoint_timeout_ms !== null) {
        core.set_endpoint_timeout_ms(this.server, this.config.endpoint_timeout_ms);
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

        core.add_endpoint(this.server, rt.path, async call_info => {
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
        }, flags);
    }
    core.add_endpoint(this.server, "", call_info => this.not_found_handler(call_info));

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


/**
 * Creates a Request object. Only for internal use.
 * @constructor
 * @private
 */
function Request(server, route, inst) {
    if(!(this instanceof Request)) {
        return new Request(...arguments);
    }
    this.server = server;
    this.route = route;
    this.inst = inst;

    this.session = new Proxy({}, {
        get: (t, k) => this.inst.sessionItem(k),
        set: (t, k, v) => this.inst.sessionItem(k, v)
    });

    this._remote_addr = null;
    this._uri = null;
    this._url = null;
    this._headers = null;
    this._cookies = null;
    this._params = null;

    let self = this;

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

Object.defineProperty(Request.prototype, "headers", {
    get: this._headers || (this._headers = this.inst.headers())
});

Object.defineProperty(Request.prototype, "uri", {
    get: this._uri || (this._uri = this.inst.uri())
});

Object.defineProperty(Request.prototype, "url", {
    get: this._url || (this._url = this.uri.split("?")[0])
});

Object.defineProperty(Request.prototype, "remote_addr", {
    get: this._remote_addr || (this._remote_addr = this.inst.remoteAddr())
});

Object.defineProperty(Request.prototype, "cookies", {
    get: this._cookies || (this._cookies = this.inst.cookies())
});

/**
 * Returns the request body.
 * @returns {string}
 */
Request.prototype.body = function () {
    return this.inst.body();
}

/**
 * Parses the request body as json.
 * @returns {object}
 */
Request.prototype.json = function () {
    let body = this.body();
    if (!body) return null;
    try {
        return JSON.parse(body.toString());
    } catch (e) {
        throw new Error("Request body is not valid JSON");
    }
}

/**
 * Parses the request body as a urlencoded form.
 * @returns {object}
 */
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

/**
 * Returns the statistics info of the current application context.
 * @param {boolean} load_system_stats - Read the system's statistics info. Defaults to false.
 * @returns {object}
 */
Request.prototype.get_stats = function(load_system_stats = false) {
    if(load_system_stats) {
        stat.update_system_stats(this);
    }

    // TODO: Not implemented yet
    return {};
    //return JSON.parse(core.get_stats_from_request(this.call_info));
}

/**
 * Sets a custom stats item.
 * @param {string} k - Key
 * @param {string} v - Value
 */
Request.prototype.set_custom_stat = function(k, v) {
    // TODO: Not implemented yet
    //core.set_custom_stat(this.call_info, k, v);
}

module.exports.Response = Response;

/**
 * Creates a Response object.
 * @constructor
 * @param {number} status - The HTTP status
 * @param {object} headers - Headers to send
 * @param {object} cookies - Cookies to set
 * @param {string|Buffer} body - Response body (optional)
 * @param {string} file - Path to a file to send (optional)
 * @param {string} template_name - Name of the template to render. Must have been loaded with `add_template()`.
 * @param {object} template_params - Parameters to pass to the template
 */
function Response({ status = 200, headers = {}, cookies = {}, body = "", file = null, template_name = null, template_params = {}, streaming_cb = null }) {
    // Do strict checks here because errors in Response.send() may cause memory leak & deadlock.

    if(!(this instanceof Response)) {
        return new Response(...arguments);
    }

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
        this.body = Buffer.from("");
    }

    if(streaming_cb) {
        if(typeof(streaming_cb) != "function") {
            throw new Error("Streaming callback must be a function");
        }
        this.streaming_cb = streaming_cb;
    }

    Object.freeze(this);
}


// This should not throw.
/**
 * @private
 */
Response.prototype.send = function (server, call_info) {
    if (!(server instanceof Ice)) {
        console.log("Error in Request.send(): Expecting a server instance");
        return;
    }

    let resp = core.create_response(call_info);
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

    if(this.streaming_cb) {
        let sp = new ResponseStream(core.enable_response_streaming(resp, call_info));
        setImmediate(async () => {
            await this.streaming_cb(sp);
            sp.close();
        });
    }

    try {
        core.fire_callback(call_info, resp);
    } catch(e) {
        console.log("Error in Request.send(): " + e);
    }
};

/**
 * Stringifies data as JSON and creates a Response.
 * @param {object} data - The data to send
 * @returns {Response}
 */
Response.json = function (data) {
    return new Response({
        headers: {
            "Content-Type": "application/json"
        },
        body: JSON.stringify(data)
    });
}

/**
 * Sends a file.
 * @param {string} path - Path to the file
 * @returns {Response}
 */
Response.file = function(path) {
    return new Response({
        file: path
    });
}

Response.redirect = function(target, code = 302) {
    return new Response({
        status: code,
        headers: {
            Location: target
        }
    });
}

Response.stream = function(cb) {
    return new Response({
        streaming_cb: cb
    });
}

function ResponseStream(sp) {
    if(!(this instanceof ResponseStream)) {
        return new ResponseStream(...arguments);
    }

    this.sp = sp;
}

ResponseStream.prototype.write = function(data) {
    if(!data) {
        throw new Error("Data required");
    }

    if(!(data instanceof Buffer)) {
        data = Buffer.from(data);
    }

    core.write_response_stream(this.sp, data);
}

ResponseStream.prototype.close = function() {
    core.close_response_stream(this.sp);
}

module.exports.Flag = Flag;

/**
 * Creates a flag which can be used as a middleware.
 * @constructor
 * @param {string} name - Name of the flag
 */
function Flag(name) {
    if(!(this instanceof Flag)) {
        return new Flag(...arguments);
    }

    if (typeof (name) != "string") {
        throw new Error("Flag name must be a string");
    }
    this.name = name;
    Object.freeze(this);
}