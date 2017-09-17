const core = require("./build/Release/ice_node_v4_core");
const assert = require("assert");

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

    set_num_executors(n) {
        assert(this.inst);
        assert(typeof(n) == "number" && n > 0);
        core.http_server_config_set_num_executors(this.inst, n);
        return this;
    }

    set_listen_addr(addr) {
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
    }

    createResponse() {
        return new HttpResponse(this.ctx);
    }

    intoBody(onData, onEnd) {
        assert(this.inst);
        let ownedInst = core.http_server_endpoint_context_take_request(this.ctx);
        this.inst = null;

        core.http_request_take_and_read_body(ownedInst, onData, onEnd);
    }
}

class HttpResponse {
    constructor(ctx) {
        this.ctx = ctx;
        this.inst = core.http_response_create();
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
}

module.exports.HttpServer = HttpServer;
module.exports.HttpServerConfig = HttpServerConfig;
