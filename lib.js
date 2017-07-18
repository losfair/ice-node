const core = require("./build/Release/ice_node_core");

module.exports.Ice = Ice;
function Ice() {
    this.server = null;
    this.routes = [];
    this.middlewares = [];
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

    this.routes.push({
        path: p,
        methods: methods,
        handler: function (req) {
            if (!mm[req.method]) {
                return new Response({
                    status: 405,
                    body: "Method not allowed"
                }).send(req.call_info);
            }

            let ok = r => {
                if (r instanceof Response) {
                    r.send(req.call_info);
                } else {
                    try {
                        new Response({
                            body: r
                        }).send(req.call_info);
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
                }).send(req.call_info);
            };

            try {
                let p = handler(req);
                if (p && p.then) {
                    p.then(ok).catch(err);
                } else {
                    ok(p);
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

Ice.prototype.use = function(p, handler) {
    if(typeof(p) != "string") throw new Error("Prefix must be a string");
    if(typeof(handler) != "function") throw new Error("Handler must be a function");

    this.middlewares.push({
        prefix: p,
        handler: handler
    });
}

Ice.prototype.listen = function (addr) {
    if (typeof (addr) != "string") throw new Error("Address must be a string");
    if (this.server !== null) {
        throw new Error("Already listening");
    }

    this.server = core.create_server();
    for (const rt of this.routes) {
        let flags = [];
        if (rt.methods.indexOf("POST") != -1 || rt.methods.indexOf("PUT") != -1) {
            flags.push("read_body");
        }

        let mws = this.middlewares.filter(v => rt.path.startsWith(v.prefix));

        core.add_endpoint(this.server, rt.path, call_info => {
            let req = new Request(call_info);
            for(const mw of mws) {
                try {
                    mw.handler(req);
                } catch(e) {
                    if(e instanceof Response) {
                        e.send(call_info);
                    } else {
                        console.log(e);
                        new Response({
                            status: 500,
                            body: "Internal error"
                        }).send(call_info);
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

Ice.prototype.not_found_handler = function (call_info) {
    return new Response({
        status: 404,
        body: "Not found"
    }).send(call_info);
}

module.exports.Request = Request;
function Request(call_info) {
    this.call_info = call_info;

    let req_info = core.get_request_info(call_info);
    this.headers = req_info.headers;
    this.uri = req_info.uri;
    this.url = req_info.uri.split("?")[0];
    this.remote_addr = req_info.remote_addr;
    this.method = req_info.method;

    this.host = req_info.headers.host;
}

Request.prototype.body = function () {
    return core.get_request_body(this.call_info);
}

Request.prototype.json = function() {
    let body = this.body();
    if(!body) return null;
    try {
        return JSON.parse(body.toString());
    } catch(e) {
        throw new Error("Request body is not valid JSON");
    }
}

Request.prototype.form = function() {
    let body = this.body();
    if(!body) return null;

    let form = {};
    try {
        body.toString().split("&").filter(v => v).map(v => v.split("=")).forEach(p => form[p[0]] = p[1]);
        return form;
    } catch(e) {
        throw new Error("Request body is not valid urlencoded form");
    }
}

module.exports.Response = Response;
function Response({ status = 200, headers = {}, body = "" }) {
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

    if (body instanceof Buffer) {
        this.body = body;
    } else {
        this.body = Buffer.from(body);
    }
    if (!this.body) throw new Error("Invalid body");
}

Response.prototype.send = function (call_info) {
    let resp = core.create_response();
    core.set_response_status(resp, this.status);
    for (const k in this.headers) {
        core.set_response_header(resp, k, this.headers[k]);
    }
    core.set_response_body(resp, this.body);
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
