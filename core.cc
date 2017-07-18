#include <node.h>
#include <node_buffer.h>
#include <uv.h>
#include "imports.h"
#include <vector>
#include <map>
#include <iostream>
#include <queue>
#include <mutex>
#include <string>

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

static std::vector<Resource> servers;
static std::map<int, EndpointHandlerInfo *> endpoint_handlers;
static uv_async_t uv_async;

static std::deque<AsyncCallbackInfo> pending_cbs;
static std::mutex pending_cbs_mutex;

static std::vector<Resource> pending_call_info;
static std::deque<int> released_pending_call_info_ids; // Not thread safe.

static std::vector<Resource> pending_responses;
static std::deque<int> released_pending_response_ids; // Not thread safe.

static void create_server(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = args.GetIsolate();
    Resource inst = ice_create_server();
    servers.push_back(inst);
    int id = servers.size() - 1;

    args.GetReturnValue().Set(Number::New(isolate, id));
}

static void set_session_timeout_ms(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = args.GetIsolate();

    if(args.Length() < 2 || !args[0] -> IsNumber() || !args[1] -> IsNumber()) {
        isolate -> ThrowException(String::NewFromUtf8(isolate, "Invalid parameters"));
        return;
    }

    unsigned int id = args[0] -> NumberValue();

    if(id >= servers.size()) {
        isolate -> ThrowException(String::NewFromUtf8(isolate, "Invalid server id"));
        return;
    }

    Resource server = servers[id];

    unsigned int timeout = args[1] -> NumberValue();
    ice_server_set_session_timeout_ms(server, timeout);
}   

static void listen(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = args.GetIsolate();

    if(args.Length() < 2 || !args[0] -> IsNumber() || !args[1] -> IsString()) {
        isolate -> ThrowException(String::NewFromUtf8(isolate, "Invalid parameters"));
        return;
    }

    unsigned int id = args[0] -> NumberValue();
    String::Utf8Value _addr(args[1] -> ToString());
    const char *addr = *_addr;

    if(id >= servers.size()) {
        isolate -> ThrowException(String::NewFromUtf8(isolate, "Invalid server id"));
        return;
    }

    ice_server_listen(servers[id], addr);
}

static void add_endpoint(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = args.GetIsolate();

    if(args.Length() < 3 || !args[0] -> IsNumber() || !args[1] -> IsString() || !args[2] -> IsFunction()) {
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

    unsigned int id = args[0] -> NumberValue();
    String::Utf8Value _p(args[1] -> ToString());
    const char *p = *_p;

    if(id >= servers.size()) {
        isolate -> ThrowException(String::NewFromUtf8(isolate, "Invalid server id"));
        return;
    }

    Resource server = servers[id];

    int ep_id = -1;

    if(!p || p[0] == 0) {
        ep_id = -1; // Default endpoint
    } else {
        Resource ep = ice_server_router_add_endpoint(server, p);
        for(auto& f : flags) {
            ice_core_endpoint_set_flag(ep, f.c_str(), true);
        }

        ep_id = ice_core_endpoint_get_id(ep);
    }

    Local<Function> _cb = Local<Function>::Cast(args[2]);
    auto cb = new Persistent<Function>(isolate, _cb);

    endpoint_handlers[ep_id] = new EndpointHandlerInfo(cb);
}

static void async_endpoint_handler(int id, Resource call_info) {
    auto target = endpoint_handlers[id];
    if(!target) {
        std::cerr << "Warning: Received async callback to unknown endpoint " << id << "." << std::endl;
        return;
    }

    pending_cbs_mutex.lock();
    pending_cbs.push_back(target -> to_async_cb_info(call_info));
    pending_cbs_mutex.unlock();

    uv_async_send(&uv_async);
}

static void node_endpoint_handler(uv_async_t *ev) {
    Isolate *isolate = Isolate::GetCurrent();
    HandleScope currentScope(isolate);

    while(true) {
        pending_cbs_mutex.lock();
        if(pending_cbs.size() == 0) {
            pending_cbs_mutex.unlock();
            break;
        }
        AsyncCallbackInfo info = pending_cbs.front();
        pending_cbs.pop_front();
        pending_cbs_mutex.unlock();

        int pci_id = -1;

        if(released_pending_call_info_ids.size()) {
            pci_id = released_pending_call_info_ids.front();
            released_pending_call_info_ids.pop_front();
            pending_call_info[pci_id] = info.call_info;
        } else {
            pending_call_info.push_back(info.call_info);
            pci_id = pending_call_info.size() - 1;
        }

        Local<Value> argv[] = {
            Number::New(isolate, pci_id)
        };

        Local<Function> cb = Local<Function>::New(isolate, *info.fn);
        cb -> Call(Null(isolate), 1, argv);
    }
}

static void fire_callback(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = args.GetIsolate();

    if(args.Length() < 2 || !args[0] -> IsNumber() || !args[1] -> IsNumber()) {
        isolate -> ThrowException(String::NewFromUtf8(isolate, "Invalid parameters"));
        return;
    }

    unsigned int call_info_id = args[0] -> NumberValue();
    unsigned int response_id = args[1] -> NumberValue();

    if(call_info_id >= pending_call_info.size() || response_id >= pending_responses.size()) {
        isolate -> ThrowException(String::NewFromUtf8(isolate, "Invalid call_info_id and / or response_id"));
        return;
    }

    Resource call_info = pending_call_info[call_info_id];
    Resource resp = pending_responses[response_id];

    if(!call_info || !resp) {
        isolate -> ThrowException(String::NewFromUtf8(isolate, "Invalid call_info_id and / or response_id"));
        return;
    }

    pending_call_info[call_info_id] = NULL;
    pending_responses[response_id] = NULL;

    released_pending_call_info_ids.push_back(call_info_id);
    released_pending_response_ids.push_back(response_id);

    //std::cerr << pending_call_info.size() << " " << pending_responses.size() << std::endl;

    ice_core_fire_callback(call_info, resp);
}

static void create_response(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = args.GetIsolate();

    auto resp = ice_glue_create_response();
    int pr_id = -1;

    if(released_pending_response_ids.size()) {
        pr_id = released_pending_response_ids.front();
        released_pending_response_ids.pop_front();
        pending_responses[pr_id] = resp;
    } else {
        pending_responses.push_back(resp);
        pr_id = pending_responses.size() - 1;
    }

    args.GetReturnValue().Set(Number::New(isolate, pr_id));
}

static void get_request_info(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = args.GetIsolate();

    if(args.Length() < 1 || !args[0] -> IsNumber()) {
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

    Local<Object> info = Object::New(isolate);
    info -> Set(String::NewFromUtf8(isolate, "uri"), String::NewFromUtf8(isolate, ice_glue_request_get_uri(req)));
    info -> Set(String::NewFromUtf8(isolate, "method"), String::NewFromUtf8(isolate, ice_glue_request_get_method(req)));
    info -> Set(String::NewFromUtf8(isolate, "remote_addr"), String::NewFromUtf8(isolate, ice_glue_request_get_remote_addr(req)));

    Local<Object> headers = Object::New(isolate);
    auto itr_p = ice_glue_request_create_header_iterator(req);

    while(true) {
        const char *k = ice_glue_request_header_iterator_next(req, itr_p);
        if(!k) break;
        const char *v = ice_glue_request_get_header(req, k);
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

    if(args.Length() < 1 || !args[0] -> IsNumber()) {
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

    u32 body_len = 0;
    const u8 *body = ice_glue_request_get_body(req, &body_len);

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

static void init_request_session(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = args.GetIsolate();

    if(args.Length() < 1 || !args[0] -> IsNumber()) {
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

    bool load_ok = false;

    if(args.Length() >= 2) {
        if(!args[1] -> IsString()) {
            isolate -> ThrowException(String::NewFromUtf8(isolate, "Session id must be a string"));
            return;
        }
        String::Utf8Value _session_id(args[1] -> ToString());
        load_ok = ice_glue_request_load_session(req, *_session_id);
    }

    if(!load_ok) {
        ice_glue_request_create_session(req);
    }
}

static void get_request_session_id(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = args.GetIsolate();

    if(args.Length() < 1 || !args[0] -> IsNumber()) {
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

    const char *id = ice_glue_request_get_session_id(req);
    if(!id) {
        args.GetReturnValue().Set(Null(isolate));
        return;
    }

    args.GetReturnValue().Set(String::NewFromUtf8(isolate, id));
}

static void get_request_session_item(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = args.GetIsolate();

    if(args.Length() < 2 || !args[0] -> IsNumber() || !args[1] -> IsString()) {
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

    String::Utf8Value _key(args[1] -> ToString());
    const char *value = ice_glue_request_get_session_item(req, *_key);
    if(!value) {
        args.GetReturnValue().Set(Null(isolate));
        return;
    }

    args.GetReturnValue().Set(String::NewFromUtf8(isolate, value));
}

static void set_request_session_item(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = args.GetIsolate();

    if(args.Length() < 3 || !args[0] -> IsNumber() || !args[1] -> IsString()) {
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

    String::Utf8Value _key(args[1] -> ToString());

    if(args[2] -> IsString()) {
        String::Utf8Value _value(args[2] -> ToString());
        ice_glue_request_set_session_item(req, *_key, *_value);
    } else if(args[2] -> IsNull()) {
        ice_glue_request_remove_session_item(req, *_key);
    } else {
        isolate -> ThrowException(String::NewFromUtf8(isolate, "Invalid value"));
    }
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

static void init(Local<Object> exports) {
    uv_async_init(uv_default_loop(), &uv_async, node_endpoint_handler);
    ice_glue_register_async_endpoint_handler(async_endpoint_handler);

    NODE_SET_METHOD(exports, "create_server", create_server);
    NODE_SET_METHOD(exports, "set_session_timeout_ms", set_session_timeout_ms);
    NODE_SET_METHOD(exports, "listen", listen);
    NODE_SET_METHOD(exports, "add_endpoint", add_endpoint);
    NODE_SET_METHOD(exports, "fire_callback", fire_callback);
    NODE_SET_METHOD(exports, "get_request_info", get_request_info);
    NODE_SET_METHOD(exports, "get_request_body", get_request_body);
    NODE_SET_METHOD(exports, "init_request_session", init_request_session);
    NODE_SET_METHOD(exports, "get_request_session_id", get_request_session_id);
    NODE_SET_METHOD(exports, "get_request_session_item", get_request_session_item);
    NODE_SET_METHOD(exports, "set_request_session_item", set_request_session_item);
    NODE_SET_METHOD(exports, "create_response", create_response);
    NODE_SET_METHOD(exports, "set_response_status", set_response_status);
    NODE_SET_METHOD(exports, "set_response_header", set_response_header);
    NODE_SET_METHOD(exports, "set_response_cookie", set_response_cookie);
    NODE_SET_METHOD(exports, "set_response_body", set_response_body);
}

NODE_MODULE(ice_node_core, init)
