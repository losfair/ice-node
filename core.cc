#include <node.h>
#include <node_buffer.h>
#include <uv.h>
#include <vector>
#include <map>
#include <iostream>
#include <queue>
#include <mutex>
#include <string>
#include "ice.h"

#define RESOURCE_TYPE_UNKNOWN 0
#define RESOURCE_TYPE_SERVER 1
#define RESOURCE_TYPE_REQUEST 2
#define RESOURCE_TYPE_RESPONSE 3

using namespace v8;

struct AsyncCallbackInfo {
    Persistent<Function> *fn;
    Resource call_info;

    AsyncCallbackInfo() {
        fn = NULL;
        call_info = NULL;
    }

    AsyncCallbackInfo(Persistent<Function> *_fn, Resource _call_info) {
        fn = _fn;
        call_info = _call_info;
    }
};

struct EndpointHandlerInfo {
    Persistent<Function> *fn;

    EndpointHandlerInfo(Persistent<Function> *_fn) {
        fn = _fn;
    }

    AsyncCallbackInfo to_async_cb_info(Resource call_info) {
        return AsyncCallbackInfo(fn, call_info);
    }
};

//static std::map<int, EndpointHandlerInfo *> endpoint_handlers;
static uv_async_t uv_async;

static std::deque<AsyncCallbackInfo> pending_cbs;
static std::mutex pending_cbs_mutex;

static Local<Object> build_resource(Isolate *isolate, unsigned long type, Resource handle) {
    Local<FunctionTemplate> ft = FunctionTemplate::New(isolate);
    ft -> InstanceTemplate() -> SetInternalFieldCount(2);

    Local<Object> ret = ft -> GetFunction() -> NewInstance();
    ret -> SetAlignedPointerInInternalField(0, (void *) type);
    ret -> SetAlignedPointerInInternalField(1, handle);

    return ret;
}


static Resource get_resource(unsigned long target_type, Local<Object> obj) {
    if(obj -> InternalFieldCount() != 2) {
        return NULL;
    }

    unsigned long type = (unsigned long) obj -> GetAlignedPointerFromInternalField(0);
    if(type != target_type) return NULL;

    Resource ret = obj -> GetAlignedPointerFromInternalField(1);
    return ret;
}

template<class T> T * get_resource(unsigned long target_type, Local<Object> obj) {
    return (T *) ((Resource) get_resource(target_type, obj));
}

static bool set_resource(unsigned long target_type, Local<Object> obj, Resource handle) {
    if(obj -> InternalFieldCount() != 2) {
        return false;
    }

    unsigned long type = (unsigned long) obj -> GetAlignedPointerFromInternalField(0);
    if(type != target_type) return false;

    obj -> SetAlignedPointerInInternalField(1, handle);

    return true;
}

static void create_server(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = args.GetIsolate();
    ice::Server *server = new ice::Server(NULL);

    args.GetReturnValue().Set(build_resource(isolate, RESOURCE_TYPE_SERVER, server));
}

static void set_endpoint_timeout_ms(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = args.GetIsolate();

    if(args.Length() < 2 || !args[0] -> IsObject() || !args[1] -> IsNumber()) {
        isolate -> ThrowException(String::NewFromUtf8(isolate, "Invalid parameters"));
        return;
    }

    ice::Server *server = get_resource<ice::Server>(RESOURCE_TYPE_SERVER, Local<Object>::Cast(args[0]));
    if(!server) {
        isolate -> ThrowException(String::NewFromUtf8(isolate, "Unable to get server"));
        return;
    }

    unsigned int timeout = args[1] -> NumberValue();
    ice_server_set_endpoint_timeout_ms(server -> handle, timeout);
}

static void set_session_timeout_ms(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = args.GetIsolate();

    if(args.Length() < 2 || !args[0] -> IsObject() || !args[1] -> IsNumber()) {
        isolate -> ThrowException(String::NewFromUtf8(isolate, "Invalid parameters"));
        return;
    }

    ice::Server *server = get_resource<ice::Server>(RESOURCE_TYPE_SERVER, Local<Object>::Cast(args[0]));
    if(!server) {
        isolate -> ThrowException(String::NewFromUtf8(isolate, "Unable to get server"));
        return;
    }

    unsigned int timeout = args[1] -> NumberValue();
    ice_server_set_session_timeout_ms(server -> handle, timeout);
}

static void add_template(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = args.GetIsolate();

    if(args.Length() < 3 || !args[0] -> IsObject() || !args[1] -> IsString() || !args[2] -> IsString()) {
        isolate -> ThrowException(String::NewFromUtf8(isolate, "Invalid parameters"));
        return;
    }

    ice::Server *server = get_resource<ice::Server>(RESOURCE_TYPE_SERVER, Local<Object>::Cast(args[0]));
    if(!server) {
        isolate -> ThrowException(String::NewFromUtf8(isolate, "Unable to get server"));
        return;
    }

    String::Utf8Value _name(args[1] -> ToString());
    String::Utf8Value _content(args[2] -> ToString());

    bool ret = ice_server_add_template(server -> handle, *_name, *_content);
    if(!ret) {
        isolate -> ThrowException(String::NewFromUtf8(isolate, "Unable to add template"));
        return;
    }
}

static void set_session_cookie_name(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = args.GetIsolate();

    if(args.Length() < 2 || !args[0] -> IsObject() || !args[1] -> IsString()) {
        isolate -> ThrowException(String::NewFromUtf8(isolate, "Invalid parameters"));
        return;
    }

    ice::Server *server = get_resource<ice::Server>(RESOURCE_TYPE_SERVER, Local<Object>::Cast(args[0]));
    if(!server) {
        isolate -> ThrowException(String::NewFromUtf8(isolate, "Unable to get server"));
        return;
    }

    String::Utf8Value _name(args[1] -> ToString());
    ice_server_set_session_cookie_name(server -> handle, *_name);
}

static void set_max_request_body_size(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = args.GetIsolate();

    if(args.Length() < 2 || !args[0] -> IsObject() || !args[1] -> IsNumber()) {
        isolate -> ThrowException(String::NewFromUtf8(isolate, "Invalid parameters"));
        return;
    }

    ice::Server *server = get_resource<ice::Server>(RESOURCE_TYPE_SERVER, Local<Object>::Cast(args[0]));
    if(!server) {
        isolate -> ThrowException(String::NewFromUtf8(isolate, "Unable to get server"));
        return;
    }

    u32 size = args[1] -> NumberValue();
    ice_server_set_max_request_body_size(server -> handle, size);
}

static void disable_request_logging(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = args.GetIsolate();

    if(args.Length() < 1 || !args[0] -> IsObject()) {
        isolate -> ThrowException(String::NewFromUtf8(isolate, "Invalid parameters"));
        return;
    }

    ice::Server *server = get_resource<ice::Server>(RESOURCE_TYPE_SERVER, Local<Object>::Cast(args[0]));
    if(!server) {
        isolate -> ThrowException(String::NewFromUtf8(isolate, "Unable to get server"));
        return;
    }

    server -> disable_request_logging();
}

static void listen(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = args.GetIsolate();

    if(args.Length() < 2 || !args[0] -> IsObject() || !args[1] -> IsString()) {
        isolate -> ThrowException(String::NewFromUtf8(isolate, "Invalid parameters"));
        return;
    }

    ice::Server *server = get_resource<ice::Server>(RESOURCE_TYPE_SERVER, Local<Object>::Cast(args[0]));
    if(!server) {
        isolate -> ThrowException(String::NewFromUtf8(isolate, "Unable to get server"));
        return;
    }

    String::Utf8Value _addr(args[1] -> ToString());
    const char *addr = *_addr;

    server -> listen(addr);
}

static void add_endpoint(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = args.GetIsolate();

    if(args.Length() < 3 || !args[0] -> IsObject() || !args[1] -> IsString() || !args[2] -> IsFunction()) {
        isolate -> ThrowException(String::NewFromUtf8(isolate, "Invalid parameters"));
        return;
    }

    std::vector<std::string> flags;

    if(args.Length() >= 4) {
        if(!args[3] -> IsArray()) {
            isolate -> ThrowException(String::NewFromUtf8(isolate, "Expecting an array"));
            return;
        }
        Local<Array> local_flags = Local<Array>::Cast(args[3]);
        unsigned int local_flags_len = local_flags -> Length();

        for(unsigned int i = 0; i < local_flags_len; i++) {
            Local<Value> elem = local_flags -> Get(i);
            if(!elem -> IsString()) {
                isolate -> ThrowException(String::NewFromUtf8(isolate, "Flags must be strings"));
                return;
            }
            String::Utf8Value _param_name(elem -> ToString());
            flags.push_back(std::string(*_param_name));
        }
    }

    ice::Server *server = get_resource<ice::Server>(RESOURCE_TYPE_SERVER, Local<Object>::Cast(args[0]));
    if(!server) {
        isolate -> ThrowException(String::NewFromUtf8(isolate, "Unable to get server"));
        return;
    }

    String::Utf8Value _p(args[1] -> ToString());
    const char *p = *_p;

    int ep_id = -1;

    Local<Function> _cb = Local<Function>::Cast(args[2]);
    auto cb = new Persistent<Function>(isolate, _cb);

    if(!p || p[0] == 0) {
        p = ""; // Default endpoint
    }
    server -> route_async(p, [cb](ice::Request _req) {
        Isolate *isolate = Isolate::GetCurrent();
        Local<Function> local_cb = Local<Function>::New(isolate, *cb);

        ice::Request *req = new ice::Request(_req);

        Local<Value> argv[] = {
            build_resource(isolate, RESOURCE_TYPE_REQUEST, (void *) req)
        };

        node::MakeCallback(isolate, Object::New(isolate), local_cb, 1, argv);
    });
}

static void fire_callback(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = args.GetIsolate();

    if(args.Length() < 2 || !args[0] -> IsObject() || !args[1] -> IsObject()) {
        isolate -> ThrowException(String::NewFromUtf8(isolate, "Invalid parameters"));
        return;
    }

    ice::Request *req = get_resource<ice::Request>(RESOURCE_TYPE_REQUEST, Local<Object>::Cast(args[0]));
    if(!req) {
        isolate -> ThrowException(String::NewFromUtf8(isolate, "Invalid request"));
        return;
    }

    ice::Response *resp = get_resource<ice::Response>(RESOURCE_TYPE_RESPONSE, Local<Object>::Cast(args[1]));
    if(!resp) {
        isolate -> ThrowException(String::NewFromUtf8(isolate, "Invalid response"));
        return;
    }

    resp -> send();

    delete req;
    delete resp;
    set_resource(RESOURCE_TYPE_REQUEST, Local<Object>::Cast(args[0]), NULL);
    set_resource(RESOURCE_TYPE_RESPONSE, Local<Object>::Cast(args[1]), NULL);
}

static void create_response(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = args.GetIsolate();

    if(args.Length() < 1 || !args[0] -> IsObject()) {
        isolate -> ThrowException(String::NewFromUtf8(isolate, "Invalid parameters"));
        return;
    }

    ice::Request *req = get_resource<ice::Request>(RESOURCE_TYPE_REQUEST, Local<Object>::Cast(args[0]));
    if(!req) {
        isolate -> ThrowException(String::NewFromUtf8(isolate, "Invalid request"));
        return;
    }

    ice::Response *resp = new ice::Response(req -> create_response());
    args.GetReturnValue().Set(build_resource(isolate, RESOURCE_TYPE_RESPONSE, (void *) resp));
}

static void get_request_info(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = args.GetIsolate();

    if(args.Length() < 1 || !args[0] -> IsObject()) {
        isolate -> ThrowException(String::NewFromUtf8(isolate, "Invalid parameters"));
        return;
    }

    ice::Request *req = get_resource<ice::Request>(RESOURCE_TYPE_REQUEST, Local<Object>::Cast(args[0]));
    if(!req) {
        isolate -> ThrowException(String::NewFromUtf8(isolate, "Invalid request"));
        return;
    }

    Local<Object> info = Object::New(isolate);
    info -> Set(String::NewFromUtf8(isolate, "uri"), String::NewFromUtf8(isolate, req -> get_uri()));
    info -> Set(String::NewFromUtf8(isolate, "method"), String::NewFromUtf8(isolate, req -> get_method()));
    info -> Set(String::NewFromUtf8(isolate, "remote_addr"), String::NewFromUtf8(isolate, req -> get_remote_addr()));

    Local<Object> headers = Object::New(isolate);
    auto itr_p = ice_glue_request_create_header_iterator(req -> handle);

    while(true) {
        const char *k = ice_glue_request_header_iterator_next(req -> handle, itr_p);
        if(!k) break;
        const char *v = ice_glue_request_get_header(req -> handle, k);
        if(v) {
            headers -> Set(String::NewFromUtf8(isolate, k), String::NewFromUtf8(isolate, v));
        }
    }

    ice_glue_destroy_header_iterator(itr_p);

    info -> Set(String::NewFromUtf8(isolate, "headers"), headers);
    args.GetReturnValue().Set(info);
}

static void get_request_body(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = args.GetIsolate();

    if(args.Length() < 1 || !args[0] -> IsObject()) {
        isolate -> ThrowException(String::NewFromUtf8(isolate, "Invalid parameters"));
        return;
    }

    ice::Request *req = get_resource<ice::Request>(RESOURCE_TYPE_REQUEST, Local<Object>::Cast(args[0]));
    if(!req) {
        isolate -> ThrowException(String::NewFromUtf8(isolate, "Invalid request"));
        return;
    }

    u32 body_len = 0;
    const u8 *body = ice_glue_request_get_body(req -> handle, &body_len);

    if(!body || !body_len) {
        args.GetReturnValue().Set(Null(isolate));
        return;
    }

    auto maybe_buf = node::Buffer::Copy(isolate, (const char *) body, body_len);
    if(maybe_buf.IsEmpty()) {
        args.GetReturnValue().Set(Null(isolate));
        return;
    }

    args.GetReturnValue().Set(maybe_buf.ToLocalChecked());
}

static void get_request_session_item(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = args.GetIsolate();

    if(args.Length() < 2 || !args[0] -> IsObject() || !args[1] -> IsString()) {
        isolate -> ThrowException(String::NewFromUtf8(isolate, "Invalid parameters"));
        return;
    }

    ice::Request *req = get_resource<ice::Request>(RESOURCE_TYPE_REQUEST, Local<Object>::Cast(args[0]));
    if(!req) {
        isolate -> ThrowException(String::NewFromUtf8(isolate, "Invalid request"));
        return;
    }

    String::Utf8Value _key(args[1] -> ToString());
    const char *value = ice_glue_request_get_session_item(req -> handle, *_key);
    if(!value) {
        args.GetReturnValue().Set(Null(isolate));
        return;
    }

    args.GetReturnValue().Set(String::NewFromUtf8(isolate, value));
}

static void set_request_session_item(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = args.GetIsolate();

    if(args.Length() < 3 || !args[0] -> IsObject() || !args[1] -> IsString()) {
        isolate -> ThrowException(String::NewFromUtf8(isolate, "Invalid parameters"));
        return;
    }

    ice::Request *req = get_resource<ice::Request>(RESOURCE_TYPE_REQUEST, Local<Object>::Cast(args[0]));
    if(!req) {
        isolate -> ThrowException(String::NewFromUtf8(isolate, "Invalid request"));
        return;
    }

    String::Utf8Value _key(args[1] -> ToString());

    if(args[2] -> IsString()) {
        String::Utf8Value _value(args[2] -> ToString());
        ice_glue_request_set_session_item(req -> handle, *_key, *_value);
    } else if(args[2] -> IsNull()) {
        ice_glue_request_set_session_item(req -> handle, *_key, NULL);
    } else {
        isolate -> ThrowException(String::NewFromUtf8(isolate, "Invalid value"));
    }
}

static void get_request_cookie(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = args.GetIsolate();

    if(args.Length() < 2 || !args[0] -> IsObject() || !args[1] -> IsString()) {
        isolate -> ThrowException(String::NewFromUtf8(isolate, "Invalid parameters"));
        return;
    }

    ice::Request *req = get_resource<ice::Request>(RESOURCE_TYPE_REQUEST, Local<Object>::Cast(args[0]));
    if(!req) {
        isolate -> ThrowException(String::NewFromUtf8(isolate, "Invalid request"));
        return;
    }

    String::Utf8Value _key(args[1] -> ToString());
    const char *value = ice_glue_request_get_cookie(req -> handle, *_key);

    args.GetReturnValue().Set(String::NewFromUtf8(isolate, value));
}

static void get_stats_from_request(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = args.GetIsolate();

    if(args.Length() < 1 || !args[0] -> IsObject()) {
        isolate -> ThrowException(String::NewFromUtf8(isolate, "Invalid parameters"));
        return;
    }

    ice::Request *req = get_resource<ice::Request>(RESOURCE_TYPE_REQUEST, Local<Object>::Cast(args[0]));
    if(!req) {
        isolate -> ThrowException(String::NewFromUtf8(isolate, "Invalid request"));
        return;
    }

    args.GetReturnValue().Set(String::NewFromUtf8(isolate, ice_glue_request_get_stats(req -> handle)));
}

static void set_custom_stat(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = args.GetIsolate();

    if(args.Length() < 3 || !args[0] -> IsObject() || !args[1] -> IsString() || !args[2] -> IsString()) {
        isolate -> ThrowException(String::NewFromUtf8(isolate, "Invalid parameters"));
        return;
    }

    ice::Request *req = get_resource<ice::Request>(RESOURCE_TYPE_REQUEST, Local<Object>::Cast(args[0]));
    if(!req) {
        isolate -> ThrowException(String::NewFromUtf8(isolate, "Invalid request"));
        return;
    }

    String::Utf8Value _k(args[1] -> ToString()), _v(args[2] -> ToString());

    ice_glue_request_set_custom_stat(req -> handle, *_k, *_v);
}

static void set_response_status(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = args.GetIsolate();

    if(args.Length() < 2 || !args[0] -> IsNumber() || !args[1] -> IsNumber()) {
        isolate -> ThrowException(String::NewFromUtf8(isolate, "Invalid parameters"));
        return;
    }

    unsigned int resp_id = args[0] -> NumberValue();
    u16 status = args[1] -> NumberValue();

    if(resp_id >= pending_responses.size() || !pending_responses[resp_id]) {
        isolate -> ThrowException(String::NewFromUtf8(isolate, "Invalid response id"));
        return;
    }

    Resource resp = pending_responses[resp_id];
    ice_glue_response_set_status(resp, status);
}

static void set_response_header(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = args.GetIsolate();

    if(args.Length() < 3 || !args[0] -> IsNumber() || !args[1] -> IsString() || !args[2] -> IsString()) {
        isolate -> ThrowException(String::NewFromUtf8(isolate, "Invalid parameters"));
        return;
    }

    unsigned int resp_id = args[0] -> NumberValue();

    if(resp_id >= pending_responses.size() || !pending_responses[resp_id]) {
        isolate -> ThrowException(String::NewFromUtf8(isolate, "Invalid response id"));
        return;
    }

    Resource resp = pending_responses[resp_id];

    String::Utf8Value _k(args[1] -> ToString());
    String::Utf8Value _v(args[2] -> ToString());

    ice_glue_response_add_header(resp, *_k, *_v);
}

static void set_response_cookie(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = args.GetIsolate();

    if(args.Length() < 3 || !args[0] -> IsNumber() || !args[1] -> IsString() || !args[2] -> IsString()) {
        isolate -> ThrowException(String::NewFromUtf8(isolate, "Invalid parameters"));
        return;
    }

    unsigned int resp_id = args[0] -> NumberValue();

    if(resp_id >= pending_responses.size() || !pending_responses[resp_id]) {
        isolate -> ThrowException(String::NewFromUtf8(isolate, "Invalid response id"));
        return;
    }

    Resource resp = pending_responses[resp_id];

    String::Utf8Value _k(args[1] -> ToString());
    String::Utf8Value _v(args[2] -> ToString());

    ice_glue_response_set_cookie(resp, *_k, *_v, NULL);
}

static void set_response_body(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = args.GetIsolate();

    if(args.Length() < 2 || !args[0] -> IsNumber() || !args[1] -> IsObject()) {
        isolate -> ThrowException(String::NewFromUtf8(isolate, "Invalid parameters"));
        return;
    }

    unsigned int resp_id = args[0] -> NumberValue();

    if(resp_id >= pending_responses.size() || !pending_responses[resp_id]) {
        isolate -> ThrowException(String::NewFromUtf8(isolate, "Invalid response id"));
        return;
    }

    Resource resp = pending_responses[resp_id];

    Local<Object> buf_obj = Local<Object>::Cast(args[1]);

    u8 *data = (u8 *) node::Buffer::Data(buf_obj);
    u32 data_len = node::Buffer::Length(buf_obj);

    ice_glue_response_set_body(resp, data, data_len);
}

static void set_response_file(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = args.GetIsolate();

    if(args.Length() < 2 || !args[0] -> IsNumber() || !args[1] -> IsString()) {
        isolate -> ThrowException(String::NewFromUtf8(isolate, "Invalid parameters"));
        return;
    }

    unsigned int resp_id = args[0] -> NumberValue();

    if(resp_id >= pending_responses.size() || !pending_responses[resp_id]) {
        isolate -> ThrowException(String::NewFromUtf8(isolate, "Invalid response id"));
        return;
    }

    Resource resp = pending_responses[resp_id];

    String::Utf8Value _path(args[1] -> ToString());

    ice_glue_response_set_file(resp, *_path);
}

static void enable_response_streaming(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = args.GetIsolate();

    if(args.Length() < 2 || !args[0] -> IsNumber() || !args[1] -> IsNumber()) {
        isolate -> ThrowException(String::NewFromUtf8(isolate, "Invalid parameters"));
        return;
    }

    unsigned int resp_id = args[0] -> NumberValue();

    if(resp_id >= pending_responses.size() || !pending_responses[resp_id]) {
        isolate -> ThrowException(String::NewFromUtf8(isolate, "Invalid response id"));
        return;
    }

    Resource resp = pending_responses[resp_id];

    unsigned int call_info_id = args[1] -> NumberValue();

    if(call_info_id >= pending_call_info.size() || !pending_call_info[call_info_id]) {
        isolate -> ThrowException(String::NewFromUtf8(isolate, "Invalid call_info_id"));
        return;
    }

    Resource call_info = pending_call_info[call_info_id];
    Resource req = ice_core_borrow_request_from_call_info(call_info);

    Local<FunctionTemplate> ft = FunctionTemplate::New(isolate);
    ft -> InstanceTemplate() -> SetInternalFieldCount(1);

    Local<Object> stream_provider = ft -> GetFunction() -> NewInstance();
    Resource internal_sp = ice_glue_response_stream(resp, ice_glue_request_borrow_context(req));
    stream_provider -> SetAlignedPointerInInternalField(0, internal_sp);

    args.GetReturnValue().Set(stream_provider);
}

static void write_response_stream(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = args.GetIsolate();

    if(args.Length() < 2 || !args[0] -> IsObject() || !args[1] -> IsObject()) {
        isolate -> ThrowException(String::NewFromUtf8(isolate, "Invalid parameters"));
        return;
    }

    Local<Object> stream_provider = Local<Object>::Cast(args[0]);
    Local<Object> buf_obj = Local<Object>::Cast(args[1]);

    Resource internal_sp = stream_provider -> GetAlignedPointerFromInternalField(0);
    if(!internal_sp) {
        isolate -> ThrowException(String::NewFromUtf8(isolate, "Invalid stream"));
        return;
    }

    u8 *data = (u8 *) node::Buffer::Data(buf_obj);
    u32 data_len = node::Buffer::Length(buf_obj);

    ice_core_stream_provider_send_chunk(internal_sp, data, data_len);
}

static void close_response_stream(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = args.GetIsolate();

    if(args.Length() < 1 || !args[0] -> IsObject()) {
        isolate -> ThrowException(String::NewFromUtf8(isolate, "Invalid parameters"));
        return;
    }

    Local<Object> stream_provider = Local<Object>::Cast(args[0]);

    Resource internal_sp = stream_provider -> GetAlignedPointerFromInternalField(0);
    if(!internal_sp) {
        isolate -> ThrowException(String::NewFromUtf8(isolate, "Invalid stream"));
        return;
    }

    stream_provider -> SetAlignedPointerInInternalField(0, NULL);
    ice_core_destroy_stream_provider(internal_sp);
}

static void render_template(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = args.GetIsolate();

    if(args.Length() < 4 || !args[0] -> IsNumber() || !args[1] -> IsNumber() || !args[2] -> IsString() || !args[3] -> IsString()) {
        isolate -> ThrowException(String::NewFromUtf8(isolate, "Invalid parameters"));
        return;
    }

    unsigned int call_info_id = args[0] -> NumberValue();

    if(call_info_id >= pending_call_info.size() || !pending_call_info[call_info_id]) {
        isolate -> ThrowException(String::NewFromUtf8(isolate, "Invalid call_info_id"));
        return;
    }

    Resource call_info = pending_call_info[call_info_id];
    Resource req = ice_core_borrow_request_from_call_info(call_info);

    unsigned int resp_id = args[1] -> NumberValue();

    if(resp_id >= pending_responses.size() || !pending_responses[resp_id]) {
        isolate -> ThrowException(String::NewFromUtf8(isolate, "Invalid response id"));
        return;
    }

    Resource resp = pending_responses[resp_id];

    String::Utf8Value _name(args[2] -> ToString());
    String::Utf8Value _data(args[3] -> ToString());

    bool ret = ice_glue_response_consume_rendered_template(
        resp,
        ice_glue_request_render_template_to_owned(req, *_name, *_data)
    );
    if(!ret) {
        isolate -> ThrowException(String::NewFromUtf8(isolate, "Unable to render template"));
        return;
    }
}

static void init(Local<Object> exports) {
    uv_async_init(uv_default_loop(), &uv_async, node_endpoint_handler);

    NODE_SET_METHOD(exports, "create_server", create_server);
    NODE_SET_METHOD(exports, "set_endpoint_timeout_ms", set_endpoint_timeout_ms);
    NODE_SET_METHOD(exports, "set_session_timeout_ms", set_session_timeout_ms);
    NODE_SET_METHOD(exports, "add_template", add_template);
    NODE_SET_METHOD(exports, "set_session_cookie_name", set_session_cookie_name);
    NODE_SET_METHOD(exports, "set_max_request_body_size", set_max_request_body_size);
    NODE_SET_METHOD(exports, "disable_request_logging", disable_request_logging);
    NODE_SET_METHOD(exports, "listen", listen);
    NODE_SET_METHOD(exports, "add_endpoint", add_endpoint);
    NODE_SET_METHOD(exports, "fire_callback", fire_callback);
    NODE_SET_METHOD(exports, "get_request_info", get_request_info);
    NODE_SET_METHOD(exports, "get_request_body", get_request_body);
    NODE_SET_METHOD(exports, "get_request_session_item", get_request_session_item);
    NODE_SET_METHOD(exports, "set_request_session_item", set_request_session_item);
    NODE_SET_METHOD(exports, "get_request_cookie", get_request_cookie);
    NODE_SET_METHOD(exports, "get_stats_from_request", get_stats_from_request);
    NODE_SET_METHOD(exports, "set_custom_stat", set_custom_stat);
    NODE_SET_METHOD(exports, "create_response", create_response);
    NODE_SET_METHOD(exports, "set_response_status", set_response_status);
    NODE_SET_METHOD(exports, "set_response_header", set_response_header);
    NODE_SET_METHOD(exports, "set_response_cookie", set_response_cookie);
    NODE_SET_METHOD(exports, "set_response_body", set_response_body);
    NODE_SET_METHOD(exports, "set_response_file", set_response_file);
    NODE_SET_METHOD(exports, "enable_response_streaming", enable_response_streaming);
    NODE_SET_METHOD(exports, "write_response_stream", write_response_stream);
    NODE_SET_METHOD(exports, "close_response_stream", close_response_stream);
    NODE_SET_METHOD(exports, "render_template", render_template);
}

NODE_MODULE(ice_node_core, init)
