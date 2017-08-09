const lib = require("./lib.js");
const mime = require("mime");

module.exports.createApplication = createApplication;

function createApplication(cfg) {
    let app = new lib.Application(cfg);
    patchApplication(app);
    return app;
}

module.exports.patchApplication = patchApplication;

function patchApplication(app) {
    if(!(app instanceof lib.Application)) {
        throw new Error("Application required");
    }

    const _app_use = app.use.bind(app);
    const _app_route = app.route.bind(app);
    const _app_prepare = app.prepare.bind(app);
    const _app_listen = app.listen.bind(app);

    app.use = function(p, fn) {
        if(typeof(fn) == "function") {
            _app_use(p, function(req, resp, mw) {
                patchResponse(resp);
                return new Promise((cb, reject) => {
                    fn(req, resp, function(e) {
                        if(e) {
                            reject(e);
                        } else {
                            cb();
                        }
                    });
                });
            });
        } else {
            _app_use(p, fn);
        }
    };
    
    app.route = function(methods, p, fn) {
        _app_route(methods, p, function(req, resp) {
            patchResponse(resp);
            return fn(req, resp);
        });
    };

    app.listen = function(addr) {
        if(typeof(addr) == "number") {
            addr = "0.0.0.0:" + addr;
        }
        _app_prepare();
        _app_listen(addr);
    };

    app.setDefaultHandler((req, resp) => {
        resp.detach();
        resp.status(404).body("Not found").send();
    });
}

function patchResponse(resp) {
    if(!(resp instanceof lib.Response)) {
        throw new Error("Response required");
    }

    if(resp.patched) return;
    resp.patched = true;

    const _send = resp.send.bind(resp);
    const _status = resp.status.bind(resp);
    const _header = resp.header.bind(resp);
    const _file = resp.file.bind(resp);
    const _body = resp.body.bind(resp);
    const _renderTemplate = resp.renderTemplate.bind(resp);

    resp.detach();

    resp.send = function(data) {
        if(data) _body(data);
        _send();
    };

    resp.end = function(data) {
        if(data) _body(data);
        _send();
    };

    resp.json = function(data) {
        _header("Content-Type", "application/json");
        _body(JSON.stringify(data));
        _send();
    };

    resp.sendFile = function(p) {
        _file(p);
        _send();
    };

    resp.sendStatus = function(code) {
        _status(code);
        _send();
    };

    resp.redirect = function() {
        let path = arguments.pop();
        let code = arguments.pop() || 302;
        _status(code);
        _header("Location", path);
        _send();
    };

    resp.type = function(t) {
        _header("Content-Type", mime.lookup(t));
        return resp;
    };

    resp.header = function(k, v) {
        if(typeof(k) == "object") {
            let obj = k;
            for(const k of obj) {
                _header(k, obj[k]);
            }
        } else {
            _header(k, v);;
        }
    };

    resp.set = resp.header;

    resp.render = function(name, data) {
        _renderTemplate(name, data);
        _send();
    };
}

module.exports.bodyParser = {
    json: parseJsonBody,
    urlencoded: parseUrlencodedBody
}

function parseJsonBody() {
    return function(req, resp, next) {
        try {
            req.body = req.json();
            next();
        } catch(e) {
            next(e);
        }
    }
}

function parseUrlencodedBody() {
    return function(req, resp, next) {
        try {
            req.body = req.form();
            next();
        } catch(e) {
            next(e);
        }
    }
}
