const lib = require("./lib.js");
const assert = require("assert");

class Router {
    constructor() {
        this.endpoints = {};
        this.middlewares = [];
    }

    build(server) {
        assert(server instanceof lib.HttpServer);

        for(const k in this.endpoints) {
            let ep = this.endpoints[k];
            let mws = this.middlewares.filter(v => ep.path.startsWith(v.path));
            let target = build_target(ep, mws);

            server.route(ep.path, target);
        }

        server.routeDefault(async (req) => {
            let reqUri = req.getUri();

            if(!(await call_middlewares(
                this.middlewares.filter(v => reqUri.startsWith(v.path)),
                req
            ))) {
                return;
            }

            req.createResponse().setStatus(404).setBody("Not found\n").send();
        });
    }

    route(method, path, target) {
        if(!this.endpoints[path]) {
            this.endpoints[path] = new Endpoint(path);
        }
        let ep = this.endpoints[path];
        ep.addMethod(method, target);
    }

    use(path, target) {
        this.middlewares.push(new Middleware(path, target));
    }
}

class Endpoint {
    constructor(path) {
        this.path = path;
        this.methodTargets = {};
    }

    addMethod(name, target) {
        assert(typeof(target) == "function");
        this.methodTargets[name.toUpperCase()] = target;
    }

    call(req) {
        let target = this.methodTargets[req.getMethod()];
        if(!target) {
            throw new MethodNotAllowedException();
        }
        return target(req);
    }
}

class Middleware {
    constructor(path, target) {
        this.path = path;
        this.target = target;
    }

    call() {
        return this.target(...arguments);
    }
}

class MethodNotAllowedException extends Error {
    constructor(msg) {
        super(msg);
    }
}

class Detached {
    constructor() {}
}

function build_target(ep, mws) {
    return async function(req) {
        if(!(await call_middlewares(mws, req))) {
            return;
        }

        let ret = null;

        try {
            ret = await ep.call(req);
        } catch(e) {
            if(e instanceof MethodNotAllowedException) {
                req.createResponse().setStatus(405).send();
            } else {
                console.log(e);
                req.createResponse().setStatus(500).send();
            }
            return;
        }

        if(ret instanceof lib.HttpResponse) {
            ret.send();
        } else if(ret instanceof Detached) {
            return;
        } else {
            try {
                ret = Buffer.from(ret);
                req.createResponse().setBody(ret).send();
            } catch(e) {
                console.log("Warning: Invalid return value from route target");
                req.createResponse().setStatus(500).send();
            }
        }
    };
}

async function call_middlewares(mws, req) {
    for(const mw of mws) {
        try {
            await mw.call(req);
        } catch(e) {
            if(e instanceof lib.HttpResponse) {
                e.send();
            } else {
                console.log(e);
                req.createResponse().setStatus(500).send();
            }
            return false;
        }
    }
    return true;
};

module.exports.Router = Router;
module.exports.Detached = Detached;
