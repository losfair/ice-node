const lib = require("./lib.js");

module.exports.ExpressRequest = ExpressRequest;
function ExpressRequest(req) {
    if(!(this instanceof ExpressRequest)) return new ExpressRequest(...arguments);
    if(!(req instanceof lib.Request)) throw new Error("ExpressRequest must be created with a Request");

    this.app = null;
    this.baseUrl = null;
    this.body = {};
    this.cookies = req.cookies;
    this.fresh = true;
    this.hostname = req.host;
    this.ip = req.remote_addr.split(":")[0];
    this.ips = [ this.ip ];
    this.method = req.method;
    this.originalUrl = req.uri;
    this.params = req.params;
    this.path = req.url;
    this.protocol = "http";
    this.query = {};
    this.route = {};
    this.secure = false;
    this.signedCookies = {};
    this.stale = false;
    this.xhr = req.headers["x-requested-with"] == "XMLHttpRequest";

    // TODO: this.accepts
    // TODO: this.acceptsCharsets
    // TODO: this.acceptsEncodings
    // TODO: this.acceptsLanguages

    this.header = k => req.headers[k];
    this.get = this.header;

    // TODO: this.is

    this.param = k => this.params[k] || (this.body ? this.body[k] : null) || this.query[k];

    // TODO: this.range

}

module.exports.ExpressResponse = ExpressResponse;
function ExpressResponse(resp) {
    if(!(this instanceof ExpressResponse)) return new ExpressResponse(...arguments);
    if(!(resp instanceof lib.Response)) throw new Error("ExpressResponse must be created with a Response");

    this.headers = {};
    this.sent = false;

    this.app = null;
    this.headersSent = false;
    this.locals = {};
    
    this.append = (k, v) => this.headers[k] = v; // TODO: Handle Set-Cookie and array values

    // TODO: Many...
}
