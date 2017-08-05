const core = require("./build/Release/ice_node_core");

module.exports.Application = Application;

function Application(cfg) {
    if(!(this instanceof Application)) {
        return new Application(...arguments);
    }
    if(!cfg) cfg = {};

    this.server = new core.Server(cfg);
    this.routes = {};
    this.flags = [];
    this.middlewares = [];
    this.prepared = false;
}

Application.prototype.route = function(methods, p, fn) {
    if(typeof(methods) == "string") methods = [ methods ];
    let param_mappings = p.split("/").filter(v => v).map(v => v.startsWith(":") ? v.substr(1) : null);

    let target;
    if(param_mappings.filter(v => v).length) {
        target = (req, resp) => {
            try {
                let params = {};
                req.url.split("/").filter(v => v).map((v, index) => [param_mappings[index], v]).forEach(p => {
                    if (p[0]) {
                        params[p[0]] = p[1];
                    }
                });
                req.params = params;
            } catch (e) {
                req.params = {};
            }
            return fn(req, resp);
        };
    } else {
        target = fn;
    }

    methods.map(v => v.toUpperCase()).forEach(m => {
        this.routes[m + " " + p] = target;
    });
    return this;
}

Application.prototype.get = function(p, fn) {
    return this.route("GET", p, fn);
}

Application.prototype.post = function(p, fn) {
    this.use(p, new Flag("read_body"));
    return this.route("POST", p, fn);
}

Application.prototype.put = function(p, fn) {
    this.use(p, new Flag("read_body"));
    return this.route("PUT", p, fn);
}

Application.prototype.delete = function(p, fn) {
    return this.route("DELETE", p, fn);
}

Application.prototype.use = function(p, fn) {
    if(fn instanceof Flag) {
        this.flags.push({
            prefix: p,
            name: fn.name
        });
    } else {
        this.middlewares.push({
            prefix: p,
            handler: fn
        });
    }
    return this;
}

Application.prototype.addTemplate = function(name, content) {
    this.server.addTemplate(name, content);
    return this;
}

Application.prototype.prepare = function() {
    if(this.prepared) {
        throw new Error("Application.prepare: Already prepared");
    }

    let routes = {};

    for(const k in this.routes) {
        const p = k.split(" ")[1];
        let mws = this.middlewares.filter(v => p.startsWith(v.prefix));

        if(!routes[p]) routes[p] = {};
        routes[p][k.split(" ")[0]] = generateEndpointHandler(mws, this.routes[k]);
    }

    for(const p in routes) {
        let methodRoutes = routes[p];
        let flags = this.flags.filter(v => p.startsWith(v.prefix)).map(v => v.name);

        this.server.route(p, function(req) {
            let rt = methodRoutes[req.method()];
            if(rt) {
                rt(req);
            } else {
                let resp = req.createResponse();
                resp.status(405);
                resp.send();
            }
        }, flags);
    }

    this.server.route("", generateEndpointHandler(this.middlewares, (req, resp) => resp.status(404)));

    this.prepared = true;
    this.route = null;
    this.use = null;
}

Application.prototype.listen = function(addr) {
    if(!this.prepared) {
        throw new Error("Not prepared");
    }

    this.server.listen(addr);
}

//module.exports.Request = Request;

function Request(inst) {
    if(!(this instanceof Request)) {
        return new Request(...arguments);
    }

    this.inst = inst;
    this.cache = {
        uri: null,
        url: null,
        headers: null,
        cookies: null
    };
    this.params = {};

    this.session = new Proxy({}, {
        get: (t, k) => this.inst.sessionItem(k),
        set: (t, k, v) => this.inst.sessionItem(k, v)
    });
}

Request.prototype.createResponse = function () {
    return new Response(this);
}

Request.prototype.body = function() {
    return this.inst.body();
}

Request.prototype.json = function() {
    let body = this.body();
    if(body) {
        return JSON.parse(body);
    } else {
        return null;
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

Object.defineProperty(Request.prototype, "uri", {
    get: function() {
        return this.cache.uri || (this.cache.uri = this.inst.uri())
    }
});

Object.defineProperty(Request.prototype, "url", {
    get: function() {
        return this.cache.url || (this.cache.url = this.uri.split("?")[0])
    }
});

Object.defineProperty(Request.prototype, "headers", {
    get: function() {
        return this.cache.headers || (this.cache.headers = this.inst.headers())
    }
});

Object.defineProperty(Request.prototype, "cookies", {
    get: function() {
        return this.cache.cookies || (this.cache.cookies = this.inst.cookies())
    }
});

//module.exports.Response = Response;

function Response(req) {
    if(!(this instanceof Response)) {
        return new Response(...arguments);
    }

    if(!(req instanceof Request)) {
        throw new Error("Request required");
    }

    this.inst = req.inst.createResponse();
    this.detached = false;
}

Response.from = function(req, data) {
    if(typeof(data) == "string" || data instanceof Buffer) {
        return new Response(req).body(data);
    }

    throw new Error("Unable to convert data into Response");
}

Response.prototype.body = function(data) {
    if(!(data instanceof Buffer)) {
        data = Buffer.from(data);
    }

    this.inst.body(data);
    return this;
}

Response.prototype.json = function(data) {
    return this.body(JSON.stringify(data));
}

Response.prototype.file = function(p) {
    this.inst.file(p);
    return this;
}

Response.prototype.status = function(code) {
    this.inst.status(code);
    return this;
}

Response.prototype.header = function(k, v) {
    this.inst.header(k, v);
    return this;
}

Response.prototype.cookie = function(k, v) {
    this.inst.cookie(k, v);
    return this;
}

Response.prototype.renderTemplate = function(name, data) {
    this.inst.renderTemplate(name, JSON.stringify(data));
    return this;
}

Response.prototype.stream = function(cb) {
    const stream = this.inst.stream();
    setImmediate(() => cb(stream));
    return this;
}

Response.prototype.send = function() {
    this.inst.send();
}

Response.prototype.detach = function() {
    this.detached = true;
}

function generateEndpointHandler(mws, fn) {
    return async function(req) {
        req = new Request(req);
        let resp = req.createResponse();

        for(const mw of mws) {
            // For dynamic dispatch
            if(!req.uri.startsWith(mw.prefix)) {
                continue;
            }
            try {
                // An exception from a middleware leads to a normal termination of the flow.
                await mw.handler(req, resp, mw);
            } catch(e) {
                resp.send();
                return;
            }
        }

        try {
            // An exception from an endpoint handler leads to an abnormal termination.
            await fn(req, resp);
        } catch(e) {
            console.log(e);
            resp.status(500).send();
            return;
        }

        if(!resp.detached) resp.send();
    }
}


module.exports.Flag = Flag;

function Flag(name) {
    if(!(this instanceof Flag)) {
        return new Flag(...arguments);
    }

    this.name = name;
}

module.exports.static = require("./static.js");
