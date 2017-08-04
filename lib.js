const core = require("./build/Release/ice_node_core");

module.exports.Application = Application;

function Application(cfg) {
    if(!(this instanceof Application)) {
        return new Application(...arguments);
    }
    if(!cfg) cfg = {};

    this.server = new core.Server(cfg);
    this.routes = {};
    this.middlewares = {};
    this.prepared = false;
}

// Dynamic dispatch, O(n)
Application.prototype.defaultRouteHandler = async function(req) {
    let uri = req.uri();

    for(const p in this.middlewares) {
        if(uri.startsWith(p)) {
            const mw = this.middlewares[p];
            try {
                await mw(req);
            } catch(e) {
                // TODO...
                return;
            }
        }
    }

    let resp = req.createResponse();
    resp.status(404);
    resp.send();
}

Application.prototype.route = function(methods, p, fn) {
    if(typeof(methods) == "string") methods = [ methods ];

    methods.map(v => v.toUpperCase()).forEach(m => {
        this.routes[m + " " + p] = fn;
    });
    return this;
}

Application.prototype.use = function(p, fn) {
    if(!this.middlewares[p]) this.middlewares[p] = [];
    this.middlewares[p].push(fn);
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
        let mws = [];
        const p = k.split(" ")[1];
        for(const m in this.middlewares) {
            if(p.startsWith(m)) {
                mws.push(this.middlewares[m]);
            }
        }
        if(!routes[p]) routes[p] = {};
        routes[p][k.split(" ")[0]] = generateEndpointHandler(mws, this.routes[k]);
    }

    for(const p in routes) {
        let methodRoutes = routes[p];
        this.server.route(p, function(req) {
            let rt = methodRoutes[req.method()];
            if(rt) {
                rt(req);
            } else {
                let resp = req.createResponse();
                resp.status(405);
                resp.send();
            }
        });
    }

    this.server.route("", this.defaultRouteHandler.bind(this));

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
        headers: null,
        cookies: null
    };
}

Request.prototype.createResponse = function () {
    return new Response(this);
}

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
            try {
                // An exception from a middleware leads to a normal termination of the flow.
                await mw(req, resp);
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
