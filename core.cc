#include <iostream>
#include <vector>
#include <string>
#include <node.h>
#include <node_buffer.h>
#include <node_object_wrap.h>
#include <unordered_map>
#include <string>
#include <assert.h>
#include <mutex>
#include <uv.h>
#include <functional>
#include <queue>

#include "ice-api-v4/init.h"
#include "ice-api-v4/http.h"

using namespace v8;

namespace ice_node {

enum NativeResourceType {
    NR_Invalid,
    NR_HttpServerConfig,
    NR_HttpServer,
    NR_HttpRouteInfo,
    NR_HttpEndpointContext,
    NR_HttpRequest,
    NR_HttpResponse
};

struct AsyncCallbackInfo {
    std::function<void()> executor;
    AsyncCallbackInfo(std::function<void()> _executor) {
        executor = _executor;
    }
};

std::mutex async_callback_queue_lock;
std::queue<AsyncCallbackInfo> async_callback_queue;
uv_async_t global_async_handle;

Persistent<FunctionTemplate> *nr_object_template = NULL;
class NativeResource {
    NativeResourceType type;
    void *data;

public:

    NativeResource(NativeResourceType _type, void *_data) {
        type = _type;
        data = _data;
    }

    Local<Object> build_object(Isolate *isolate) {
        if(nr_object_template == NULL) {
            Local<FunctionTemplate> t = FunctionTemplate::New(isolate);
            t -> InstanceTemplate() -> SetInternalFieldCount(2);

            nr_object_template = new Persistent<FunctionTemplate>(isolate, t);
        }

        Local<FunctionTemplate> ft = Local<FunctionTemplate>::New(isolate, *nr_object_template);

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

    static void reset_object(Local<Object> obj) {
        assert(obj -> InternalFieldCount() == 2);
        obj -> SetAlignedPointerInInternalField(0, NULL);
        obj -> SetAlignedPointerInInternalField(1, NULL);
    }
};

static void handle_async_callback(uv_async_t *async_info) {
    while(true) {
        async_callback_queue_lock.lock();

        if(!async_callback_queue.size()) {
            async_callback_queue_lock.unlock();
            break;
        }

        auto cb_info = async_callback_queue.front();
        async_callback_queue.pop();
        async_callback_queue_lock.unlock();

        cb_info.executor();
    }
}

static void enqueue_executor(std::function<void()> executor) {
    async_callback_queue_lock.lock();
    async_callback_queue.push(AsyncCallbackInfo(executor));
    async_callback_queue_lock.unlock();
    uv_async_send(&global_async_handle);
}

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

static void http_server_route_create(const FunctionCallbackInfo<Value>& args) {
    Isolate *isolate = args.GetIsolate();

    String::Utf8Value path(args[0] -> ToString());
    Local<Function> _cb = Local<Function>::Cast(args[1]);
    auto cb = new Persistent<Function>(isolate, _cb);

    IceHttpRouteInfo rt = ice_http_server_route_create(
        *path,
        [](IceHttpEndpointContext ctx, IceHttpRequest req, void *call_with) {
            auto cb = (Persistent<Function> *) call_with;
            enqueue_executor([cb, ctx, req]() {
                Isolate *isolate = Isolate::GetCurrent();
                HandleScope scope(isolate);

                Local<Function> local_cb = Local<Function>::New(isolate, *cb);
                
                Local<Value> argv[] = {
                    NativeResource(NR_HttpEndpointContext, (void *) ctx).build_object(isolate),
                    NativeResource(NR_HttpRequest, (void *) req).build_object(isolate)
                };

                node::MakeCallback(
                    isolate,
                    Object::New(isolate),
                    local_cb,
                    2,
                    argv
                );
            });
        },
        (void *) cb
    );

    NativeResource res(NR_HttpRouteInfo, (void *) rt);
    args.GetReturnValue().Set(res.build_object(isolate));
}

static void http_response_create(const FunctionCallbackInfo<Value>& args) {
    Isolate *isolate = args.GetIsolate();

    NativeResource res(
        NR_HttpResponse,
        (void *) ice_http_response_create()
    );

    args.GetReturnValue().Set(res.build_object(isolate));
}

static void http_response_destroy(const FunctionCallbackInfo<Value>& args) {
    Local<Object> target = args[0] -> ToObject();
    NativeResource res = NativeResource::from_object(
        target
    );
    assert(res.get_type() == NR_HttpResponse);

    ice_http_response_destroy(
        (IceHttpResponse) res.get_data()
    );
    NativeResource::reset_object(target);
}

static void http_server_endpoint_context_end_with_response(
    const FunctionCallbackInfo<Value>& args
) {
    NativeResource ctxRes = NativeResource::from_object(
        args[0] -> ToObject()
    );
    assert(ctxRes.get_type() == NR_HttpEndpointContext);

    NativeResource respRes = NativeResource::from_object(
        args[1] -> ToObject()
    );
    assert(respRes.get_type() == NR_HttpResponse);

    ice_http_server_endpoint_context_end_with_response(
        (IceHttpEndpointContext) ctxRes.get_data(),
        (IceHttpResponse) respRes.get_data()
    );
}

static void http_server_route_destroy(const FunctionCallbackInfo<Value>& args) {
    NativeResource res = NativeResource::from_object(args[0] -> ToObject());
    assert(res.get_type() == NR_HttpRouteInfo);

    IceHttpRouteInfo rt = (IceHttpRouteInfo) res.get_data();
    ice_http_server_route_destroy(rt);
}

static void http_server_add_route(const FunctionCallbackInfo<Value>& args) {
    NativeResource serverRes = NativeResource::from_object(args[0] -> ToObject());
    assert(serverRes.get_type() == NR_HttpServer);

    NativeResource rtRes = NativeResource::from_object(args[1] -> ToObject());
    assert(rtRes.get_type() == NR_HttpRouteInfo);

    IceHttpServer server = (IceHttpServer) serverRes.get_data();
    IceHttpRouteInfo rt = (IceHttpRouteInfo) rtRes.get_data();

    ice_http_server_add_route(server, rt);
}

static void http_server_set_default_route(const FunctionCallbackInfo<Value>& args) {
    NativeResource serverRes = NativeResource::from_object(args[0] -> ToObject());
    assert(serverRes.get_type() == NR_HttpServer);

    NativeResource rtRes = NativeResource::from_object(args[1] -> ToObject());
    assert(rtRes.get_type() == NR_HttpRouteInfo);

    IceHttpServer server = (IceHttpServer) serverRes.get_data();
    IceHttpRouteInfo rt = (IceHttpRouteInfo) rtRes.get_data();

    ice_http_server_set_default_route(server, rt);
}

static void init_module(Local<Object> exports) {
    uv_async_init(uv_default_loop(), &global_async_handle, handle_async_callback);
    NODE_SET_METHOD(exports, "http_server_config_create", http_server_config_create);
    NODE_SET_METHOD(exports, "http_server_config_destroy", http_server_config_destroy);
    NODE_SET_METHOD(exports, "http_server_config_set_listen_addr", http_server_config_set_listen_addr);
    NODE_SET_METHOD(exports, "http_server_config_set_num_executors", http_server_config_set_num_executors);
    NODE_SET_METHOD(exports, "http_server_create", http_server_create);
    NODE_SET_METHOD(exports, "http_server_start", http_server_start);
    NODE_SET_METHOD(exports, "http_server_route_create", http_server_route_create);
    NODE_SET_METHOD(exports, "http_server_route_destroy", http_server_route_destroy);
    NODE_SET_METHOD(exports, "http_server_add_route", http_server_add_route);
    NODE_SET_METHOD(exports, "http_server_set_default_route", http_server_set_default_route);
    NODE_SET_METHOD(exports, "http_response_create", http_response_create);
    NODE_SET_METHOD(exports, "http_response_destroy", http_response_destroy);
    NODE_SET_METHOD(exports, "http_server_endpoint_context_end_with_response", http_server_endpoint_context_end_with_response);
}

NODE_MODULE(addon, init_module)

}
