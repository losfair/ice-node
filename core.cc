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
#include <utility>
#include <string.h>

#include "ice-api-v4/metadata.h"
#include "ice-api-v4/glue.h"
#include "ice-api-v4/http.h"
#include "ice-api-v4/rpc.h"

using namespace v8;

namespace ice_node {

enum NativeResourceType {
    NR_Invalid,
    NR_HttpServerConfig,
    NR_HttpServer,
    NR_HttpRouteInfo,
    NR_HttpEndpointContext,
    NR_HttpRequest,
    NR_HttpResponse,
    NR_RpcServerConfig,
    NR_RpcServer,
    NR_RpcCallContext,
    NR_RpcParam,
    NR_RpcClient,
    NR_RpcClientConnection
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

static Local<Value> build_string_from_ice_owned_string(Isolate *isolate, ice_owned_string_t os) {
    if(os) {
        auto s = String::NewFromUtf8(isolate, os);
        ice_glue_destroy_cstring(os);
        return s;
    } else {
        return Null(isolate);
    }
}

static void http_server_config_create(const FunctionCallbackInfo<Value>& args) {
    Isolate *isolate = args.GetIsolate();
    IceHttpServerConfig cfg = ice_http_server_config_create();
    NativeResource res(NR_HttpServerConfig, (void *) cfg);
    args.GetReturnValue().Set(res.build_object(isolate));
}

static void http_server_config_destroy(const FunctionCallbackInfo<Value>& args) {
    auto arg0 = args[0] -> ToObject();

    NativeResource res = NativeResource::from_object(arg0);
    assert(res.get_type() == NR_HttpServerConfig);

    IceHttpServerConfig cfg = (IceHttpServerConfig) res.get_data();
    ice_http_server_config_destroy(cfg);

    NativeResource::reset_object(arg0);
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
    auto arg0 = args[0] -> ToObject();

    NativeResource res = NativeResource::from_object(arg0);
    assert(res.get_type() == NR_HttpServerConfig);

    IceHttpServerConfig cfg = (IceHttpServerConfig) res.get_data();

    IceHttpServer server = ice_http_server_create(cfg);
    NativeResource serverRes(NR_HttpServer, (void *) server);

    NativeResource::reset_object(arg0);
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

static void http_server_endpoint_context_end_with_response(
    const FunctionCallbackInfo<Value>& args
) {
    auto arg0 = args[0] -> ToObject();
    auto arg1 = args[1] -> ToObject();

    NativeResource ctxRes = NativeResource::from_object(
        arg0
    );
    assert(ctxRes.get_type() == NR_HttpEndpointContext);

    NativeResource respRes = NativeResource::from_object(
        arg1
    );
    assert(respRes.get_type() == NR_HttpResponse);

    ice_http_server_endpoint_context_end_with_response(
        (IceHttpEndpointContext) ctxRes.get_data(),
        (IceHttpResponse) respRes.get_data()
    );

    NativeResource::reset_object(arg0);
    NativeResource::reset_object(arg1);
}

static void http_server_endpoint_context_take_request(
    const FunctionCallbackInfo<Value>& args
) {
    Isolate *isolate = args.GetIsolate();

    auto arg0 = args[0] -> ToObject();

    NativeResource ctxRes = NativeResource::from_object(
        arg0
    );
    assert(ctxRes.get_type() == NR_HttpEndpointContext);

    IceHttpEndpointContext ctx = (IceHttpEndpointContext) ctxRes.get_data();
    IceHttpRequest req = ice_http_server_endpoint_context_take_request(ctx);
    assert(req != NULL);

    NativeResource reqRes(NR_HttpRequest, (void *) req);
    args.GetReturnValue().Set(reqRes.build_object(isolate));
}

static void http_server_route_destroy(const FunctionCallbackInfo<Value>& args) {
    auto arg0 = args[0] -> ToObject();

    NativeResource res = NativeResource::from_object(arg0);
    assert(res.get_type() == NR_HttpRouteInfo);

    IceHttpRouteInfo rt = (IceHttpRouteInfo) res.get_data();
    ice_http_server_route_destroy(rt);

    NativeResource::reset_object(arg0);
}

static void http_server_add_route(const FunctionCallbackInfo<Value>& args) {
    auto arg0 = args[0] -> ToObject();
    auto arg1 = args[1] -> ToObject();

    NativeResource serverRes = NativeResource::from_object(arg0);
    assert(serverRes.get_type() == NR_HttpServer);

    NativeResource rtRes = NativeResource::from_object(arg1);
    assert(rtRes.get_type() == NR_HttpRouteInfo);

    IceHttpServer server = (IceHttpServer) serverRes.get_data();
    IceHttpRouteInfo rt = (IceHttpRouteInfo) rtRes.get_data();

    ice_http_server_add_route(server, rt);

    NativeResource::reset_object(arg1);
}

static void http_server_set_default_route(const FunctionCallbackInfo<Value>& args) {
    auto arg0 = args[0] -> ToObject();
    auto arg1 = args[1] -> ToObject();

    NativeResource serverRes = NativeResource::from_object(arg0);
    assert(serverRes.get_type() == NR_HttpServer);

    NativeResource rtRes = NativeResource::from_object(arg1);
    assert(rtRes.get_type() == NR_HttpRouteInfo);

    IceHttpServer server = (IceHttpServer) serverRes.get_data();
    IceHttpRouteInfo rt = (IceHttpRouteInfo) rtRes.get_data();

    ice_http_server_set_default_route(server, rt);

    NativeResource::reset_object(arg1);
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

static void http_response_set_body(const FunctionCallbackInfo<Value>& args) {
    Local<Object> target = args[0] -> ToObject();
    NativeResource res = NativeResource::from_object(
        target
    );
    assert(res.get_type() == NR_HttpResponse);

    IceHttpResponse resp = (IceHttpResponse) res.get_data();
    Local<Object> buf_obj = Local<Object>::Cast(args[1]);

    const ice_uint8_t *data = (ice_uint8_t *) node::Buffer::Data(buf_obj);
    assert(data != NULL);

    ice_uint32_t data_len = node::Buffer::Length(buf_obj);
    ice_http_response_set_body(resp, data, data_len);
}

static void http_response_set_status(const FunctionCallbackInfo<Value>& args) {
    Local<Object> target = args[0] -> ToObject();
    NativeResource res = NativeResource::from_object(
        target
    );
    assert(res.get_type() == NR_HttpResponse);

    IceHttpResponse resp = (IceHttpResponse) res.get_data();

    ice_uint16_t status = args[1] -> NumberValue();
    ice_http_response_set_status(resp, status);
}

static void http_response_set_header(const FunctionCallbackInfo<Value>& args) {
    Local<Object> target = args[0] -> ToObject();
    NativeResource res = NativeResource::from_object(
        target
    );
    assert(res.get_type() == NR_HttpResponse);

    IceHttpResponse resp = (IceHttpResponse) res.get_data();

    String::Utf8Value key(args[1] -> ToString());
    String::Utf8Value value(args[2] -> ToString());

    ice_http_response_set_header(resp, *key, *value);
}

static void http_response_append_header(const FunctionCallbackInfo<Value>& args) {
    Local<Object> target = args[0] -> ToObject();
    NativeResource res = NativeResource::from_object(
        target
    );
    assert(res.get_type() == NR_HttpResponse);

    IceHttpResponse resp = (IceHttpResponse) res.get_data();

    String::Utf8Value key(args[1] -> ToString());
    String::Utf8Value value(args[2] -> ToString());

    ice_http_response_append_header(resp, *key, *value);
}

static void http_request_destroy(const FunctionCallbackInfo<Value>& args) {
    Local<Object> target = args[0] -> ToObject();
    NativeResource res = NativeResource::from_object(
        target
    );
    assert(res.get_type() == NR_HttpRequest);
    IceHttpRequest req = (IceHttpRequest) res.get_data();

    ice_http_request_destroy(req);
    NativeResource::reset_object(target);
}

static void http_request_get_uri(const FunctionCallbackInfo<Value>& args) {
    Isolate *isolate = args.GetIsolate();

    Local<Object> target = args[0] -> ToObject();
    NativeResource res = NativeResource::from_object(
        target
    );
    assert(res.get_type() == NR_HttpRequest);
    IceHttpRequest req = (IceHttpRequest) res.get_data();

    args.GetReturnValue().Set(
        build_string_from_ice_owned_string(
            isolate,
            ice_http_request_get_uri_to_owned(req)
        )
    );
}

static void http_request_get_method(const FunctionCallbackInfo<Value>& args) {
    Isolate *isolate = args.GetIsolate();
    
    Local<Object> target = args[0] -> ToObject();
    NativeResource res = NativeResource::from_object(
        target
    );
    assert(res.get_type() == NR_HttpRequest);
    IceHttpRequest req = (IceHttpRequest) res.get_data();

    args.GetReturnValue().Set(
        build_string_from_ice_owned_string(
            isolate,
            ice_http_request_get_method_to_owned(req)
        )
    );
}

static void http_request_get_remote_addr(const FunctionCallbackInfo<Value>& args) {
    Isolate *isolate = args.GetIsolate();
    
    Local<Object> target = args[0] -> ToObject();
    NativeResource res = NativeResource::from_object(
        target
    );
    assert(res.get_type() == NR_HttpRequest);
    IceHttpRequest req = (IceHttpRequest) res.get_data();

    args.GetReturnValue().Set(
        build_string_from_ice_owned_string(
            isolate,
            ice_http_request_get_remote_addr_to_owned(req)
        )
    );
}

static void http_request_get_header(const FunctionCallbackInfo<Value>& args) {
    Isolate *isolate = args.GetIsolate();
    
    Local<Object> target = args[0] -> ToObject();
    NativeResource res = NativeResource::from_object(
        target
    );
    assert(res.get_type() == NR_HttpRequest);
    IceHttpRequest req = (IceHttpRequest) res.get_data();

    String::Utf8Value key(args[1] -> ToString());
    args.GetReturnValue().Set(
        build_string_from_ice_owned_string(
            isolate,
            ice_http_request_get_header_to_owned(req, *key)
        )
    );
}

struct RequestBodyReadContext {
    std::unique_ptr<Persistent<Function>> onData;
    std::unique_ptr<Persistent<Function>> onEnd;
    bool shouldTerminate;

    RequestBodyReadContext(Persistent<Function> *_onData, Persistent<Function> *_onEnd)
        : onData(_onData), onEnd(_onEnd) {
            shouldTerminate = false;
    }

    ~RequestBodyReadContext() {
        onData -> Reset();
        onEnd -> Reset();
    }
};

static void http_request_take_and_read_body(const FunctionCallbackInfo<Value>& args) {
    Isolate *isolate = args.GetIsolate();
    
    Local<Object> target = args[0] -> ToObject();
    NativeResource res = NativeResource::from_object(
        target
    );
    assert(res.get_type() == NR_HttpRequest);
    IceHttpRequest req = (IceHttpRequest) res.get_data();

    NativeResource::reset_object(target);

    Local<Function> onData = Local<Function>::Cast(args[1]);
    Local<Function> onEnd = Local<Function>::Cast(args[2]);

    auto callbackCtx = new RequestBodyReadContext(
        new Persistent<Function>(isolate, onData),
        new Persistent<Function>(isolate, onEnd)
    );

    ice_http_request_take_and_read_body(
        req,
        [](const ice_uint8_t *data, ice_uint32_t len, void *call_with) -> ice_uint8_t {
            auto callbackCtx = (RequestBodyReadContext *) call_with;
            if(callbackCtx -> shouldTerminate) {
                return 0;
            }

            char *raw_buf = new char [len];
            memcpy(raw_buf, data, len);

            enqueue_executor([=]() {
                Isolate *isolate = Isolate::GetCurrent();
                HandleScope scope(isolate);

                Local<Function> local_cb = Local<Function>::New(isolate, *callbackCtx -> onData);
    
                auto data_buf = node::Buffer::New(
                    isolate,
                    raw_buf,
                    len,
                    [](char *data, void *hint) {
                        delete[] data;
                    },
                    NULL
                );
                assert(!data_buf.IsEmpty());
    
                Local<Value> argv[] = {
                    data_buf.ToLocalChecked()
                };
    
                Local<Value> ret = node::MakeCallback(
                    isolate,
                    Object::New(isolate),
                    local_cb,
                    1,
                    argv
                );
                if(ret -> BooleanValue() == false) {
                    callbackCtx -> shouldTerminate = true;
                }
            });

            return 1;
        },
        [](ice_uint8_t ok, void *call_with) {
            enqueue_executor([=]() {
                auto callbackCtx = (RequestBodyReadContext *) call_with;

                Isolate *isolate = Isolate::GetCurrent();
                HandleScope scope(isolate);
    
                Local<Function> local_cb = Local<Function>::New(isolate, *callbackCtx -> onEnd);
                Local<Value> argv[] = {
                    Boolean::New(isolate, (bool) ok)
                };
                node::MakeCallback(
                    isolate,
                    Object::New(isolate),
                    local_cb,
                    1,
                    argv
                );
    
                delete callbackCtx;
            });
        },
        (void *) callbackCtx
    );
}

static void storage_file_http_response_begin_send(const FunctionCallbackInfo<Value>& args) {
    Isolate *isolate = args.GetIsolate();
    
    Local<Object> arg0 = args[0] -> ToObject();
    Local<Object> arg1 = args[1] -> ToObject();

    NativeResource reqRes = NativeResource::from_object(
        arg0
    );
    assert(reqRes.get_type() == NR_HttpRequest);
    IceHttpRequest req = (IceHttpRequest) reqRes.get_data();

    NativeResource respRes = NativeResource::from_object(
        arg1
    );
    assert(respRes.get_type() == NR_HttpResponse);
    IceHttpResponse resp = (IceHttpResponse) respRes.get_data();

    String::Utf8Value path(args[2] -> ToString());
    ice_uint8_t ret = ice_storage_file_http_response_begin_send(req, resp, *path);
    args.GetReturnValue().Set(Boolean::New(isolate, (bool) ret));
}

static void rpc_server_config_create(const FunctionCallbackInfo<Value>& args) {
    NativeResource res(
        NR_RpcServerConfig,
        (void *) ice_rpc_server_config_create()
    );
    args.GetReturnValue().Set(res.build_object(args.GetIsolate()));
}

static void rpc_server_config_destroy(const FunctionCallbackInfo<Value>& args) {
    Local<Object> arg0 = args[0] -> ToObject();
    NativeResource res = NativeResource::from_object(
        arg0
    );
    assert(res.get_type() == NR_RpcServerConfig);

    ice_rpc_server_config_destroy((IceRpcServerConfig) res.get_data());
    NativeResource::reset_object(arg0);
}

static void rpc_server_config_add_method(const FunctionCallbackInfo<Value>& args) {
    Isolate *isolate = args.GetIsolate();

    Local<Object> arg0 = args[0] -> ToObject();
    NativeResource res = NativeResource::from_object(
        arg0
    );
    assert(res.get_type() == NR_RpcServerConfig);

    auto config = (IceRpcServerConfig) res.get_data();

    String::Utf8Value name(args[1] -> ToString());

    Local<Function> cb = Local<Function>::Cast(args[2]);
    auto persistent_cb = new Persistent<Function>(isolate, cb);

    ice_rpc_server_config_add_method(
        config,
        *name,
        [](IceRpcCallContext ctx, void *call_with) {
            enqueue_executor([=]() {
                Isolate *isolate = Isolate::GetCurrent();
                HandleScope scope(isolate);
    
                auto pf = (Persistent<Function> *) call_with;
                auto cb = Local<Function>::New(isolate, *pf);

                Local<Value> argv[] = {
                    NativeResource(NR_RpcCallContext, (void *) ctx).build_object(isolate)
                };
                node::MakeCallback(
                    isolate,
                    Object::New(isolate),
                    cb,
                    1,
                    argv
                );
            });
        },
        (void *) persistent_cb
    );
}

static void rpc_server_create(const FunctionCallbackInfo<Value>& args) {
    Isolate *isolate = args.GetIsolate();
    Local<Object> arg0 = args[0] -> ToObject();
    NativeResource res = NativeResource::from_object(
        arg0
    );
    assert(res.get_type() == NR_RpcServerConfig);

    auto config = (IceRpcServerConfig) res.get_data();
    NativeResource::reset_object(arg0);

    IceRpcServer server = ice_rpc_server_create(config);
    args.GetReturnValue().Set(
        NativeResource(
            NR_RpcServer,
            (void *) server
        ).build_object(isolate)
    );
}

static void rpc_server_start(const FunctionCallbackInfo<Value>& args) {
    Local<Object> arg0 = args[0] -> ToObject();
    NativeResource res = NativeResource::from_object(
        arg0
    );
    assert(res.get_type() == NR_RpcServer);
    auto server = (IceRpcServer) res.get_data();

    String::Utf8Value addr(args[1] -> ToString());
    ice_rpc_server_start(server, *addr);
}

static void rpc_call_context_get_num_params(const FunctionCallbackInfo<Value>& args) {
    Isolate *isolate = args.GetIsolate();

    Local<Object> arg0 = args[0] -> ToObject();
    NativeResource res = NativeResource::from_object(
        arg0
    );
    assert(res.get_type() == NR_RpcCallContext);

    auto ctx = (IceRpcCallContext) res.get_data();
    int n = ice_rpc_call_context_get_num_params(ctx);

    args.GetReturnValue().Set(
        Number::New(isolate, n)
    );
}

static void rpc_call_context_get_param(const FunctionCallbackInfo<Value>& args) {
    Isolate *isolate = args.GetIsolate();

    Local<Object> arg0 = args[0] -> ToObject();
    NativeResource res = NativeResource::from_object(
        arg0
    );
    assert(res.get_type() == NR_RpcCallContext);

    auto ctx = (IceRpcCallContext) res.get_data();
    int pos = args[1] -> NumberValue();

    IceRpcParam p = ice_rpc_call_context_get_param(ctx, pos);
    if(p == NULL) {
        args.GetReturnValue().Set(Null(isolate));
    } else {
        args.GetReturnValue().Set(
            NativeResource(NR_RpcParam, (void *) p).build_object(isolate)
        );
    }
}

static void rpc_call_context_end(const FunctionCallbackInfo<Value>& args) {
    Local<Object> arg0 = args[0] -> ToObject();
    NativeResource ctxRes = NativeResource::from_object(
        arg0
    );
    assert(ctxRes.get_type() == NR_RpcCallContext);

    Local<Object> arg1 = args[1] -> ToObject();
    NativeResource retRes = NativeResource::from_object(
        arg1
    );
    assert(retRes.get_type() == NR_RpcParam);

    auto ctx = (IceRpcCallContext) ctxRes.get_data();
    auto ret = (IceRpcParam) retRes.get_data();
    NativeResource::reset_object(arg0);
    NativeResource::reset_object(arg1);

    ice_rpc_call_context_end(ctx, ret);
}

static void rpc_param_build_i32(const FunctionCallbackInfo<Value>& args) {
    Isolate *isolate = args.GetIsolate();
    int v = args[0] -> NumberValue();

    IceRpcParam p = ice_rpc_param_build_i32(v);
    NativeResource res(NR_RpcParam, (void *) p);

    args.GetReturnValue().Set(res.build_object(isolate));
}

static void rpc_param_build_f64(const FunctionCallbackInfo<Value>& args) {
    Isolate *isolate = args.GetIsolate();
    double v = args[0] -> NumberValue();

    IceRpcParam p = ice_rpc_param_build_f64(v);
    NativeResource res(NR_RpcParam, (void *) p);

    args.GetReturnValue().Set(res.build_object(isolate));
}

static void rpc_param_build_string(const FunctionCallbackInfo<Value>& args) {
    Isolate *isolate = args.GetIsolate();
    String::Utf8Value v(args[0] -> ToString());

    IceRpcParam p = ice_rpc_param_build_string(*v);
    NativeResource res(NR_RpcParam, (void *) p);

    args.GetReturnValue().Set(res.build_object(isolate));
}

static void rpc_param_build_error(const FunctionCallbackInfo<Value>& args) {
    Isolate *isolate = args.GetIsolate();

    Local<Object> arg0 = args[0] -> ToObject();
    NativeResource fromRes = NativeResource::from_object(arg0);

    assert(fromRes.get_type() == NR_RpcParam);
    auto from = (IceRpcParam) fromRes.get_data();

    NativeResource::reset_object(arg0);

    IceRpcParam p = ice_rpc_param_build_error(from);
    NativeResource res(NR_RpcParam, (void *) p);

    args.GetReturnValue().Set(res.build_object(isolate));
}

static void rpc_param_build_bool(const FunctionCallbackInfo<Value>& args) {
    Isolate *isolate = args.GetIsolate();
    bool v = args[0] -> BooleanValue();

    IceRpcParam p = ice_rpc_param_build_bool(v);
    NativeResource res(NR_RpcParam, (void *) p);

    args.GetReturnValue().Set(res.build_object(isolate));
}

static void rpc_param_build_null(const FunctionCallbackInfo<Value>& args) {
    Isolate *isolate = args.GetIsolate();

    IceRpcParam p = ice_rpc_param_build_null();
    NativeResource res(NR_RpcParam, (void *) p);

    args.GetReturnValue().Set(res.build_object(isolate));
}

static void rpc_param_get_i32(const FunctionCallbackInfo<Value>& args) {
    Isolate *isolate = args.GetIsolate();

    NativeResource res = NativeResource::from_object(args[0] -> ToObject());
    assert(res.get_type() == NR_RpcParam);
    IceRpcParam p = (IceRpcParam) res.get_data();

    int v = ice_rpc_param_get_i32(p);
    args.GetReturnValue().Set(Number::New(isolate, v));
}

static void rpc_param_get_f64(const FunctionCallbackInfo<Value>& args) {
    Isolate *isolate = args.GetIsolate();

    NativeResource res = NativeResource::from_object(args[0] -> ToObject());
    assert(res.get_type() == NR_RpcParam);
    IceRpcParam p = (IceRpcParam) res.get_data();

    double v = ice_rpc_param_get_f64(p);
    args.GetReturnValue().Set(Number::New(isolate, v));
}

static void rpc_param_get_string(const FunctionCallbackInfo<Value>& args) {
    Isolate *isolate = args.GetIsolate();

    NativeResource res = NativeResource::from_object(args[0] -> ToObject());
    assert(res.get_type() == NR_RpcParam);
    IceRpcParam p = (IceRpcParam) res.get_data();

    ice_owned_string_t v = ice_rpc_param_get_string_to_owned(p);
    if(v == NULL) {
        args.GetReturnValue().Set(Null(isolate));
    } else {
        Local<Value> ret = String::NewFromUtf8(isolate, v);
        ice_glue_destroy_cstring(v);
        args.GetReturnValue().Set(ret);
    }
}

static void rpc_param_get_bool(const FunctionCallbackInfo<Value>& args) {
    Isolate *isolate = args.GetIsolate();

    NativeResource res = NativeResource::from_object(args[0] -> ToObject());
    assert(res.get_type() == NR_RpcParam);
    IceRpcParam p = (IceRpcParam) res.get_data();

    bool v = ice_rpc_param_get_bool(p);
    args.GetReturnValue().Set(Boolean::New(isolate, v));
}

static void rpc_param_get_error(const FunctionCallbackInfo<Value>& args) {
    Isolate *isolate = args.GetIsolate();

    NativeResource res = NativeResource::from_object(args[0] -> ToObject());
    assert(res.get_type() == NR_RpcParam);
    IceRpcParam p = (IceRpcParam) res.get_data();

    IceRpcParam v = ice_rpc_param_get_error(p);
    if(v == NULL) {
        args.GetReturnValue().Set(Null(isolate));
    } else {
        NativeResource ret(NR_RpcParam, (void *) v);
        args.GetReturnValue().Set(ret.build_object(isolate));
    }
}

static void rpc_param_is_null(const FunctionCallbackInfo<Value>& args) {
    Isolate *isolate = args.GetIsolate();

    NativeResource res = NativeResource::from_object(args[0] -> ToObject());
    assert(res.get_type() == NR_RpcParam);
    IceRpcParam p = (IceRpcParam) res.get_data();

    bool ret = ice_rpc_param_is_null(p);
    args.GetReturnValue().Set(Boolean::New(isolate, ret));
}

static void rpc_param_destroy(const FunctionCallbackInfo<Value>& args) {
    Local<Object> arg0 = args[0] -> ToObject();

    NativeResource res = NativeResource::from_object(arg0);
    assert(res.get_type() == NR_RpcParam);
    IceRpcParam p = (IceRpcParam) res.get_data();

    NativeResource::reset_object(arg0);
    ice_rpc_param_destroy(p);
}

static void rpc_param_clone(const FunctionCallbackInfo<Value>& args) {
    Isolate *isolate = args.GetIsolate();

    NativeResource res = NativeResource::from_object(args[0] -> ToObject());
    assert(res.get_type() == NR_RpcParam);
    IceRpcParam p = (IceRpcParam) res.get_data();

    IceRpcParam ret = ice_rpc_param_clone(p);

    args.GetReturnValue().Set(
        NativeResource(NR_RpcParam, (void *) ret).build_object(isolate)
    );
}

void check_version() {
    const char *version = ice_metadata_get_version();
    const char *target_version = "0.4.0-alpha.";

    assert(strncmp(target_version, version, strlen(target_version)) == 0);
}

static void init_module(Local<Object> exports) {
    uv_async_init(uv_default_loop(), &global_async_handle, handle_async_callback);

    check_version();

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
    NODE_SET_METHOD(exports, "http_response_set_body", http_response_set_body);
    NODE_SET_METHOD(exports, "http_response_set_status", http_response_set_status);
    NODE_SET_METHOD(exports, "http_response_set_header", http_response_set_header);
    NODE_SET_METHOD(exports, "http_response_append_header", http_response_append_header);
    NODE_SET_METHOD(exports, "http_server_endpoint_context_end_with_response", http_server_endpoint_context_end_with_response);
    NODE_SET_METHOD(exports, "http_request_get_uri", http_request_get_uri);
    NODE_SET_METHOD(exports, "http_request_get_method", http_request_get_method);
    NODE_SET_METHOD(exports, "http_request_get_remote_addr", http_request_get_remote_addr);
    NODE_SET_METHOD(exports, "http_request_get_header", http_request_get_header);
    NODE_SET_METHOD(exports, "storage_file_http_response_begin_send", storage_file_http_response_begin_send);
    NODE_SET_METHOD(exports, "http_server_endpoint_context_take_request", http_server_endpoint_context_take_request);
    NODE_SET_METHOD(exports, "http_request_destroy", http_request_destroy);
    NODE_SET_METHOD(exports, "http_request_take_and_read_body", http_request_take_and_read_body);
    NODE_SET_METHOD(exports, "rpc_server_config_create", rpc_server_config_create);
    NODE_SET_METHOD(exports, "rpc_server_config_destroy", rpc_server_config_destroy);
    NODE_SET_METHOD(exports, "rpc_server_config_add_method", rpc_server_config_add_method);
    NODE_SET_METHOD(exports, "rpc_server_create", rpc_server_create);
    NODE_SET_METHOD(exports, "rpc_server_start", rpc_server_start);
    NODE_SET_METHOD(exports, "rpc_call_context_get_num_params", rpc_call_context_get_num_params);
    NODE_SET_METHOD(exports, "rpc_call_context_get_param", rpc_call_context_get_param);
    NODE_SET_METHOD(exports, "rpc_call_context_end", rpc_call_context_end);
    NODE_SET_METHOD(exports, "rpc_param_build_i32", rpc_param_build_i32);
    NODE_SET_METHOD(exports, "rpc_param_build_f64", rpc_param_build_f64);
    NODE_SET_METHOD(exports, "rpc_param_build_string", rpc_param_build_string);
    NODE_SET_METHOD(exports, "rpc_param_build_error", rpc_param_build_error);
    NODE_SET_METHOD(exports, "rpc_param_build_bool", rpc_param_build_bool);
    NODE_SET_METHOD(exports, "rpc_param_build_null", rpc_param_build_null);
    NODE_SET_METHOD(exports, "rpc_param_get_i32", rpc_param_get_i32);
    NODE_SET_METHOD(exports, "rpc_param_get_f64", rpc_param_get_f64);
    NODE_SET_METHOD(exports, "rpc_param_get_string", rpc_param_get_string);
    NODE_SET_METHOD(exports, "rpc_param_get_bool", rpc_param_get_bool);
    NODE_SET_METHOD(exports, "rpc_param_get_error", rpc_param_get_error);
    NODE_SET_METHOD(exports, "rpc_param_is_null", rpc_param_is_null);
    NODE_SET_METHOD(exports, "rpc_param_destroy", rpc_param_destroy);
    NODE_SET_METHOD(exports, "rpc_param_clone", rpc_param_clone);
    //NODE_SET_METHOD(exports, , );
}

NODE_MODULE(addon, init_module)

}
