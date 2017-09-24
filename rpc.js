const core = require("./build/Release/ice_node_v4_core");
const assert = require("assert");

class RpcServerConfig {
    constructor() {
        this.inst = core.rpc_server_config_create();
    }

    destroy() {
        assert(this.inst);

        core.rpc_server_config_destroy(this.inst);
        this.inst = null;
    }

    addMethod(name, cb) {
        assert(this.inst);
        assert(typeof(name) == "string" && typeof(cb) == "function");

        core.rpc_server_config_add_method(this.inst, name, function (rawCtx) {
            return cb(new RpcCallContext(rawCtx));
        });
    }
}

class RpcServer {
    constructor(config) {
        assert(config instanceof RpcServerConfig && config.inst);
        this.inst = core.rpc_server_create(config.inst);
        config.inst = null;
    }

    start(addr) {
        assert(typeof(addr) == "string");
        assert(this.inst);

        core.rpc_server_start(this.inst, addr);
    }
}

class RpcCallContext {
    constructor(inst) {
        this.inst = inst;
    }

    getNumParams() {
        assert(this.inst);
        return core.rpc_call_context_get_num_params(this.inst);
    }

    getParam(pos) {
        assert(typeof(pos) == "number");
        assert(this.inst);

        return new RpcParam(core.rpc_call_context_get_param(this.inst, pos));
    }

    end(ret) {
        assert(this.inst);
        assert(ret instanceof RpcParam && ret.inst);
        core.rpc_call_context_end(this.inst, ret.inst);
        ret.inst = null;
    }
}

class RpcParam {
    constructor(inst) {
        assert(inst);
        this.inst = inst;
    }

    destroy() {
        assert(this.inst);
        core.rpc_param_destroy(this.inst);
        this.inst = null;
    }

    getI32() {
        assert(this.inst);
        return core.rpc_param_get_i32(this.inst);
    }

    getF64() {
        assert(this.inst);
        return core.rpc_param_get_f64(this.inst);
    }

    getString() {
        assert(this.inst);
        return core.rpc_param_get_string(this.inst);
    }

    getBool() {
        assert(this.inst);
        return core.rpc_param_get_bool(this.inst);
    }

    getError() {
        assert(this.inst);
        let e = core.rpc_param_get_error(this.inst);
        if(e) {
            return new RpcParam(e);
        } else {
            return null;
        }
    }

    isNull() {
        assert(this.inst);
        return core.rpc_param_is_null(this.inst);
    }

    static buildI32(v) {
        assert(typeof(v) == "number");
        return new RpcParam(core.rpc_param_build_i32(v));
    }

    static buildF64(v) {
        assert(typeof(v) == "number");
        return new RpcParam(core.rpc_param_build_f64(v));
    }

    static buildString(v) {
        assert(typeof(v) == "string");
        return new RpcParam(core.rpc_param_build_string(v));
    }

    static buildError(v) {
        assert(v instanceof RpcParam && v.inst);
        let newParam = new RpcParam(core.rpc_param_build_error(v.inst));
        v.inst = null;
        return newParam;
    }

    static buildBool(v) {
        assert(v === true || v === false);
        return new RpcParam(core.rpc_param_build_bool(v));
    }

    static buildNull() {
        return new RpcParam(core.rpc_param_build_null());
    }
}

module.exports.RpcServerConfig = RpcServerConfig;
module.exports.RpcServer = RpcServer;
module.exports.RpcCallContext = RpcCallContext;
module.exports.RpcParam = RpcParam;
