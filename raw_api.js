const core = require("./build/Release/ice_node_core");
const util = require("util");

const EventEmitter = require('events').EventEmitter;

function Server() {
    this.listening = false;
    this.inner = null;
    this.maxHeadersCount = 2000;
    this.timeout = 120000;
    this.keepAliveTimeout = 5000;
}

util.inherits(Server, EventEmitter);

Server.prototype.init = function(cfg) {
    this.inner = new core.Server(cfg);
    this.inner.route("", this.defaultEndpoint.bind(this));
}

Server.prototype.defaultEndpoint = function(req) {
    f
}

Server.prototype.listen = function(port, hostname, backlog, cb) {
    let addr;
    if(hostname) {
        addr = hostname + ":" + port;
    } else {
        addr = "0.0.0.0:" + port;
    }

    this.inner.listen(addr);
    this.listening = true;
    if(cb) {
        process.nextTick(() => cb({}));
    }
}

Server.prototype.setTimeout = function() {}
Server.prototype.close = function() {}

function ServerResponse(inst) {
    this.inst = inst;
    this.connection = {};
    this.sentHeaders = {};
    this.finished = false;
    this.headersSent = false;
    this.sendDate = true;
    this.statusCode = 200;
}

ServerResponse.prototype.addTrailers = function() {}

ServerResponse.prototype.end = function(data, encoding) {
    if(data) {
        if(!(data instanceof Buffer)) {
            data = Buffer.from(data);
        }

        this.inst.body(data);
    }

    this.inst.status(this.statusCode);
    this.inst.send();
    this.finished = true;
    this.headersSent = true;
}

ServerResponse.prototype.getHeader = function(name) {
    return this.sentHeaders[name];
}

ServerResponse.prototype.getHeaderNames = function() {
    return Object.keys(this.sentHeaders);
}

ServerResponse.prototype.getHeaders = function() {
    let c = {};
    for(const k in this.sentHeaders) {
        c[k] = this.sentHeaders[k];
    }
    return c;
}

ServerResponse.prototype.hasHeader = function(name) {
    return !!this.sentHeaders[name];
}

ServerResponse.prototype.removeHeader = function(name) {
    console.log("removeHeader is not supported currently.");
    //delete this.sentHeaders[name];
}

ServerResponse.prototype.setHeader = function(name, value) {
    name = name.toLowerCase();
    this.sentHeaders[name] = value;

    if(name == "set-cookie") {
        if(typeof(value) == "string") value = [ value ];
        for(const item of value) {
            let p = item.indexOf("=");
            let k = item.substr(0, p);
            let v = item.substr(p + 1);
            this.inst.cookie(k, v);
        }
    } else {
        this.inst.header(name, value);
    }
}

ServerResponse.prototype.setTimeout = function() {}
