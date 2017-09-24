const core = require("./build/Release/ice_node_v4_core");
const assert = require("assert");
const router = require("./router.js");
const rpc = require("./rpc.js");

module.exports.router = router;
module.exports.rpc = rpc;

class HttpServer {
    constructor(cfg) {
        assert((cfg instanceof HttpServerConfig) && cfg.inst);
        this.inst = core.http_server_create(cfg.inst);
        cfg.inst = null;

        this.started = false;
    }

    start() {
        assert(!this.started);
        core.http_server_start(this.inst);
        this.started = true;
    }

    route(path, target) {
        let rt = core.http_server_route_create(path, function (ctx, rawReq) {
            let req = new HttpRequest(ctx, rawReq);
            target(req);
        });
        core.http_server_add_route(this.inst, rt);
    }

    routeDefault(target) {
        let rt = core.http_server_route_create("", function (ctx, rawReq) {
            let req = new HttpRequest(ctx, rawReq);
            target(req);
        });
        core.http_server_set_default_route(this.inst, rt);
    }
}

class HttpServerConfig {
    constructor() {
        this.inst = core.http_server_config_create();
    }

    destroy() {
        assert(this.inst);
        core.http_server_config_destroy(this.inst);
        this.inst = null;
    }

    setNumExecutors(n) {
        assert(this.inst);
        assert(typeof(n) == "number" && n > 0);
        core.http_server_config_set_num_executors(this.inst, n);
        return this;
    }

    setListenAddr(addr) {
        assert(this.inst);
        assert(typeof(addr) == "string");
        core.http_server_config_set_listen_addr(this.inst, addr);
        return this;
    }
}

class HttpRequest {
    constructor(ctx, req) {
        this.ctx = ctx;
        this.inst = req;
        this._cache = {
            uri: null,
            method: null,
            remoteAddr: null
        };
    }

    createResponse() {
        return new HttpResponse(this.ctx, this);
    }

    intoBody(onData, onEnd) {
        assert(this.inst);
        let ownedInst = core.http_server_endpoint_context_take_request(this.ctx);
        this.inst = null;

        core.http_request_take_and_read_body(ownedInst, onData, onEnd);
    }

    getMethod() {
        assert(this.inst);
        return core.http_request_get_method(this.inst);
    }

    getUri() {
        assert(this.inst);
        return core.http_request_get_uri(this.inst);
    }

    getRemoteAddr() {
        assert(this.inst);
        return core.http_request_get_remote_addr(this.inst);
    }

    getHeader(k) {
        assert(this.inst);
        assert(typeof(k) == "string");
        return core.http_request_get_header(this.inst, k);
    }

    get uri() {
        return (this._cache.uri || (this._cache.uri = this.getUri()));
    }

    get method() {
        return (this._cache.method || (this._cache.method = this.getMethod()));
    }

    get remoteAddr() {
        return (this._cache.remoteAddr || (this._cache.remoteAddr = this.getRemoteAddr()));
    }
}

class HttpResponse {
    constructor(ctx, req) {
        this.ctx = ctx;
        this.req = req;
        this.inst = core.http_response_create();
        core.http_response_set_header(this.inst, "X-Powered-By", "Ice-node");
    }

    destroy() {
        assert(this.inst);
        core.http_response_destroy(this.inst);
        this.inst = null;
    }

    send() {
        assert(this.inst);
        core.http_server_endpoint_context_end_with_response(this.ctx, this.inst);
        this.inst = null;
    }

    setBody(data) {
        assert(this.inst);

        if(!(data instanceof Buffer)) {
            data = Buffer.from(data);
        }
        assert(data);

        core.http_response_set_body(this.inst, data);

        return this;
    }

    setStatus(status) {
        assert(this.inst);
        assert(typeof(status) == "number");

        core.http_response_set_status(this.inst, status);

        return this;
    }

    setHeader(k, v) {
        assert(this.inst);
        assert(typeof(k) == "string" && typeof(v) == "string");

        core.http_response_set_header(this.inst, k, v);

        return this;
    }

    appendHeader(k, v) {
        assert(this.inst);
        assert(typeof(k) == "string" && typeof(v) == "string");

        core.http_response_append_header(this.inst, k, v);

        return this;
    }

    sendFile(path) {
        assert(this.inst && this.req.inst);
        assert(typeof(path) == "string");

        let ret = core.storage_file_http_response_begin_send(this.req.inst, this.inst, path);
        if(!ret) {
            throw new Error("Unable to send file: " + path);
        }

        return this;
    }
}

module.exports.HttpServer = HttpServer;
module.exports.HttpServerConfig = HttpServerConfig;
module.exports.HttpRequest = HttpRequest;
module.exports.HttpResponse = HttpResponse;
