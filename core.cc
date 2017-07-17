#include <node.h>
#include <uv.h>
#include "imports.h"
#include <vector>
#include <map>
#include <iostream>
#include <queue>
#include <mutex>

using namespace v8;

struct AsyncCallbackInfo {
    Persistent<Function, CopyablePersistentTraits<Function>> *fn;
    Resource call_info;

    AsyncCallbackInfo() {
        fn = NULL;
        call_info = NULL;
    }

    AsyncCallbackInfo(Persistent<Function, CopyablePersistentTraits<Function>> *_fn, Resource _call_info) {
        fn = _fn;
        call_info = _call_info;
    }
};

struct EndpointHandlerInfo {
    Persistent<Function, CopyablePersistentTraits<Function>> *fn;

    EndpointHandlerInfo(Persistent<Function, CopyablePersistentTraits<Function>> *_fn) {
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

    unsigned int id = args[0] -> NumberValue();
    String::Utf8Value _p(args[1] -> ToString());
    const char *p = *_p;

    if(id >= servers.size()) {
        isolate -> ThrowException(String::NewFromUtf8(isolate, "Invalid server id"));
        return;
    }

    Resource server = servers[id];

    Resource ep = ice_server_router_add_endpoint(server, p);
    int ep_id = ice_core_endpoint_get_id(ep);

    Local<Function> _cb = Local<Function>::Cast(args[2]);
    auto cb = new Persistent<Function, CopyablePersistentTraits<Function>>(isolate, _cb);

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

static void init(Local<Object> exports) {
    uv_async_init(uv_default_loop(), &uv_async, node_endpoint_handler);
    ice_glue_register_async_endpoint_handler(async_endpoint_handler);

    NODE_SET_METHOD(exports, "create_server", create_server);
    NODE_SET_METHOD(exports, "listen", listen);
    NODE_SET_METHOD(exports, "add_endpoint", add_endpoint);
    NODE_SET_METHOD(exports, "fire_callback", fire_callback);
    NODE_SET_METHOD(exports, "create_response", create_response);
    NODE_SET_METHOD(exports, "set_response_status", set_response_status);
}

NODE_MODULE(ice_node_core, init)
