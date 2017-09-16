#include <iostream>
#include <vector>
#include <string>
#include <node.h>
#include <node_buffer.h>
#include <node_object_wrap.h>
#include <unordered_map>
#include <string>
#include <assert.h>

#include "ice-api-v4/init.h"
#include "ice-api-v4/http.h"

using namespace v8;

namespace ice_node {

enum NativeResourceType {
    NR_HttpServerConfig,
    NR_HttpServer,
    NR_HttpRouteInfo
};

class NativeResource {
    NativeResourceType type;
    void *data;

public:

    NativeResource(NativeResourceType _type, void *_data) {
        type = _type;
        data = _data;
    }

    Local<Object> build_object(Isolate *isolate) {
        Local<FunctionTemplate> ft = FunctionTemplate::New(isolate);
        ft -> InstanceTemplate() -> SetInternalFieldCount(2);

        Local<Object> ret = ft -> GetFunction() -> NewInstance(isolate -> GetCurrentContext()).ToLocalChecked();
        ret -> SetAlignedPointerInInternalField(0, (void *) (type * sizeof(long)));
        ret -> SetAlignedPointerInInternalField(1, data);
        return ret;
    }

    NativeResourceType get_type() {
        return type;
    }

    void * get_data() {
        return data;
    }

    static NativeResource from_object(Local<Object> obj) {
        assert(obj -> InternalFieldCount() == 2);
        NativeResourceType _type = (NativeResourceType) (((unsigned long) obj -> GetAlignedPointerFromInternalField(0)) / sizeof(long));
        void *_data = obj -> GetAlignedPointerFromInternalField(1);
        return NativeResource(_type, _data);
    }
};

static void http_server_config_create(const FunctionCallbackInfo<Value>& args) {
    Isolate *isolate = args.GetIsolate();
    IceHttpServerConfig cfg = ice_http_server_config_create();
    NativeResource res(NR_HttpServerConfig, (void *) cfg);
    args.GetReturnValue().Set(res.build_object(isolate));
}

static void http_server_config_destroy(const FunctionCallbackInfo<Value>& args) {
    NativeResource res = NativeResource::from_object(args[0] -> ToObject());
    assert(res.get_type() == NR_HttpServerConfig);

    IceHttpServerConfig cfg = (IceHttpServerConfig) res.get_data();
    ice_http_server_config_destroy(cfg);
}

static void http_server_config_set_listen_addr(const FunctionCallbackInfo<Value>& args) {
    NativeResource res = NativeResource::from_object(args[0] -> ToObject());
    assert(res.get_type() == NR_HttpServerConfig);

    IceHttpServerConfig cfg = (IceHttpServerConfig) res.get_data();

    String::Utf8Value addr(args[1] -> ToString());
    ice_http_server_config_set_listen_addr(cfg, *addr);
}

static void http_server_config_set_num_executors(const FunctionCallbackInfo<Value>& args) {
    NativeResource res = NativeResource::from_object(args[0] -> ToObject());
    assert(res.get_type() == NR_HttpServerConfig);

    IceHttpServerConfig cfg = (IceHttpServerConfig) res.get_data();

    ice_http_server_config_set_num_executors(cfg, (unsigned int) args[1] -> NumberValue());
}

static void http_server_create(const FunctionCallbackInfo<Value>& args) {
    NativeResource res = NativeResource::from_object(args[0] -> ToObject());
    assert(res.get_type() == NR_HttpServerConfig);

    IceHttpServerConfig cfg = (IceHttpServerConfig) res.get_data();

    IceHttpServer server = ice_http_server_create(cfg);
    NativeResource serverRes(NR_HttpServer, (void *) server);

    args.GetReturnValue().Set(serverRes.build_object(args.GetIsolate()));
}

static void http_server_start(const FunctionCallbackInfo<Value>& args) {
    NativeResource res = NativeResource::from_object(args[0] -> ToObject());
    assert(res.get_type() == NR_HttpServer);

    IceHttpServer server = (IceHttpServer) res.get_data();
    ice_http_server_start(server);
}

static void init_module(Local<Object> exports) {
    NODE_SET_METHOD(exports, "http_server_config_create", http_server_config_create);
    NODE_SET_METHOD(exports, "http_server_config_destroy", http_server_config_destroy);
    NODE_SET_METHOD(exports, "http_server_config_set_listen_addr", http_server_config_set_listen_addr);
    NODE_SET_METHOD(exports, "http_server_config_set_num_executors", http_server_config_set_num_executors);
    NODE_SET_METHOD(exports, "http_server_create", http_server_create);
    NODE_SET_METHOD(exports, "http_server_start", http_server_start);
}

NODE_MODULE(addon, init_module)

}
