const lib = require("./lib.js");

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
    resp.detach();

    resp.send = function(data) {
        if(data) resp.body(data);
        _send();
    };

    resp.end = function(data) {
        resp.body(data);
        _send();
    };
}
