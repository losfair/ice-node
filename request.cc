#include <string>
#include <node.h>
#include <node_buffer.h>
#include <node_object_wrap.h>

#include "request.h"

using namespace v8;

namespace ice_node {

static ice::Request *_pending_request = NULL;
static Persistent<Function> _request_constructor;

void Request::New(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = args.GetIsolate();

    if (args.IsConstructCall()) {
        if(!_pending_request) {
            isolate -> ThrowException(String::NewFromUtf8(isolate, "Illegal call to Request::New"));
            return;
        }

        Request *req = new Request(*_pending_request);
        _pending_request = NULL;

        req -> Wrap(args.This());

        args.GetReturnValue().Set(args.This());
    } else {
        isolate -> ThrowException(String::NewFromUtf8(isolate, "Illegal call to Request::New"));
        return;
    }
}

void Request::RemoteAddr(const v8::FunctionCallbackInfo<v8::Value>& args) {
    Isolate *isolate = args.GetIsolate();
    Request *req = node::ObjectWrap::Unwrap<Request>(args.Holder());

    if(req -> responseSent) {
        isolate -> ThrowException(String::NewFromUtf8(isolate, "Request is no longer valid once a Response is sent"));
        return;
    }

    args.GetReturnValue().Set(String::NewFromUtf8(isolate, req -> _inst.get_remote_addr()));
}

void Request::Method(const v8::FunctionCallbackInfo<v8::Value>& args) {
    Isolate *isolate = args.GetIsolate();
    Request *req = node::ObjectWrap::Unwrap<Request>(args.Holder());

    if(req -> responseSent) {
        isolate -> ThrowException(String::NewFromUtf8(isolate, "Request is no longer valid once a Response is sent"));
        return;
    }

    args.GetReturnValue().Set(String::NewFromUtf8(isolate, req -> _inst.get_method()));
}

void Request::Uri(const v8::FunctionCallbackInfo<v8::Value>& args) {
    Isolate *isolate = args.GetIsolate();
    Request *req = node::ObjectWrap::Unwrap<Request>(args.Holder());

    if(req -> responseSent) {
        isolate -> ThrowException(String::NewFromUtf8(isolate, "Request is no longer valid once a Response is sent"));
        return;
    }

    args.GetReturnValue().Set(String::NewFromUtf8(isolate, req -> _inst.get_uri()));
}

void Request::SessionItem(const v8::FunctionCallbackInfo<v8::Value>& args) {
    Isolate *isolate = args.GetIsolate();
    Request *req = node::ObjectWrap::Unwrap<Request>(args.Holder());

    if(req -> responseSent) {
        isolate -> ThrowException(String::NewFromUtf8(isolate, "Request is no longer valid once a Response is sent"));
        return;
    }

    String::Utf8Value key(args[0] -> ToString());

    if(args.Length() >= 2) {
        String::Utf8Value value(args[1] -> ToString());
        req -> _inst.set_session_item(*key, *value);
    } else {
        const char *v = req -> _inst.get_session_item(*key);
        if(v) {
            args.GetReturnValue().Set(String::NewFromUtf8(isolate, v));
        } else {
            args.GetReturnValue().Set(Null(isolate));
        }
    }
}

void Request::SessionItems(const v8::FunctionCallbackInfo<v8::Value>& args) {
    Isolate *isolate = args.GetIsolate();
    Request *req = node::ObjectWrap::Unwrap<Request>(args.Holder());

    if(req -> responseSent) {
        isolate -> ThrowException(String::NewFromUtf8(isolate, "Request is no longer valid once a Response is sent"));
        return;
    }

    Local<Object> items = Object::New(isolate);
    auto _items = req -> _inst.get_session_items();

    for(auto& p : _items) {
        items -> Set(String::NewFromUtf8(isolate, p.first.c_str()), String::NewFromUtf8(isolate, p.second.c_str()));
    }

    args.GetReturnValue().Set(items);
}

void Request::Header(const v8::FunctionCallbackInfo<v8::Value>& args) {
    Isolate *isolate = args.GetIsolate();
    Request *req = node::ObjectWrap::Unwrap<Request>(args.Holder());

    if(req -> responseSent) {
        isolate -> ThrowException(String::NewFromUtf8(isolate, "Request is no longer valid once a Response is sent"));
        return;
    }

    String::Utf8Value key(args[0] -> ToString());

    args.GetReturnValue().Set(String::NewFromUtf8(isolate, req -> _inst.get_header(*key)));
}

void Request::Headers(const v8::FunctionCallbackInfo<v8::Value>& args) {
    Isolate *isolate = args.GetIsolate();
    Request *req = node::ObjectWrap::Unwrap<Request>(args.Holder());

    if(req -> responseSent) {
        isolate -> ThrowException(String::NewFromUtf8(isolate, "Request is no longer valid once a Response is sent"));
        return;
    }

    Local<Object> items = Object::New(isolate);
    auto _items = req -> _inst.get_headers();

    for(auto& p : _items) {
        if(p.second) items -> Set(String::NewFromUtf8(isolate, p.first.c_str()), String::NewFromUtf8(isolate, p.second));
    }

    args.GetReturnValue().Set(items);
}

void Request::Cookie(const v8::FunctionCallbackInfo<v8::Value>& args) {
    Isolate *isolate = args.GetIsolate();
    Request *req = node::ObjectWrap::Unwrap<Request>(args.Holder());

    if(req -> responseSent) {
        isolate -> ThrowException(String::NewFromUtf8(isolate, "Request is no longer valid once a Response is sent"));
        return;
    }

    String::Utf8Value key(args[0] -> ToString());

    args.GetReturnValue().Set(String::NewFromUtf8(isolate, req -> _inst.get_cookie(*key)));
}

void Request::Cookies(const v8::FunctionCallbackInfo<v8::Value>& args) {
    Isolate *isolate = args.GetIsolate();
    Request *req = node::ObjectWrap::Unwrap<Request>(args.Holder());

    if(req -> responseSent) {
        isolate -> ThrowException(String::NewFromUtf8(isolate, "Request is no longer valid once a Response is sent"));
        return;
    }

    Local<Object> items = Object::New(isolate);
    auto _items = req -> _inst.get_cookies();

    for(auto& p : _items) {
        if(p.second) items -> Set(String::NewFromUtf8(isolate, p.first.c_str()), String::NewFromUtf8(isolate, p.second));
    }

    args.GetReturnValue().Set(items);
}

void Request::Body(const v8::FunctionCallbackInfo<v8::Value>& args) {
    Isolate *isolate = args.GetIsolate();
    Request *req = node::ObjectWrap::Unwrap<Request>(args.Holder());

    if(req -> responseSent) {
        isolate -> ThrowException(String::NewFromUtf8(isolate, "Request is no longer valid once a Response is sent"));
        return;
    }

    u32 bodyLen = 0;
    const u8 *body = req -> _inst.get_body(&bodyLen);

    if(!body) {
        args.GetReturnValue().Set(Null(isolate));
        return;
    }

    auto maybeBuf = node::Buffer::Copy(isolate, (const char *) body, bodyLen);
    if(maybeBuf.IsEmpty()) {
        args.GetReturnValue().Set(Null(isolate));
        return;
    }

    args.GetReturnValue().Set(maybeBuf.ToLocalChecked());
}

void Request::CustomProperty(const v8::FunctionCallbackInfo<v8::Value>& args) {
    Isolate *isolate = args.GetIsolate();
    Request *req = node::ObjectWrap::Unwrap<Request>(args.Holder());

    if(req -> responseSent) {
        isolate -> ThrowException(String::NewFromUtf8(isolate, "Request is no longer valid once a Response is sent"));
        return;
    }

    String::Utf8Value key(args[0] -> ToString());
    auto cp = req -> _inst.borrow_custom_properties();

    if(args.Length() >= 2) {
        String::Utf8Value v(args[1] -> ToString());
        cp.set(*key, *v);
    } else {
        std::string v = cp.get(*key);
        args.GetReturnValue().Set(String::NewFromUtf8(isolate, v.c_str()));
    }
}

void Request::CreateResponse(const v8::FunctionCallbackInfo<v8::Value>& args) {
    Isolate *isolate = args.GetIsolate();
    Request *req = node::ObjectWrap::Unwrap<Request>(args.Holder());

    if(req -> responseCreated) {
        isolate -> ThrowException(String::NewFromUtf8(isolate, "Cannot create more than one response from one request"));
        return;
    }

    auto _resp = req -> _inst.create_response();
    req -> responseCreated = true;

    auto ctx = req -> _inst.get_context();

    args.GetReturnValue().Set(Response::Create(isolate, _resp, args.This(), ctx));
}

Local<Object> Request::Create(Isolate *isolate, ice::Request& from) {
    const int argc = 0;
    Local<Value> argv[argc] = { };
    _pending_request = &from;

    Local<Function> cons = Local<Function>::New(isolate, _request_constructor);
    Local<Context> context = isolate -> GetCurrentContext();
    Local<Object> instance = cons -> NewInstance(context, argc, argv).ToLocalChecked();

    return instance;
}

void Request::Init(Isolate *isolate) {
    Local<FunctionTemplate> tpl = FunctionTemplate::New(isolate, New);
    tpl -> SetClassName(String::NewFromUtf8(isolate, "Request"));
    tpl -> InstanceTemplate() -> SetInternalFieldCount(1);

    NODE_SET_PROTOTYPE_METHOD(tpl, "remoteAddr", RemoteAddr);
    NODE_SET_PROTOTYPE_METHOD(tpl, "method", Method);
    NODE_SET_PROTOTYPE_METHOD(tpl, "uri", Uri);
    NODE_SET_PROTOTYPE_METHOD(tpl, "sessionItem", SessionItem);
    NODE_SET_PROTOTYPE_METHOD(tpl, "sessionItems", SessionItems);
    NODE_SET_PROTOTYPE_METHOD(tpl, "header", Header);
    NODE_SET_PROTOTYPE_METHOD(tpl, "headers", Headers);
    NODE_SET_PROTOTYPE_METHOD(tpl, "cookie", Cookie);
    NODE_SET_PROTOTYPE_METHOD(tpl, "cookies", Cookies);
    NODE_SET_PROTOTYPE_METHOD(tpl, "body", Body);
    NODE_SET_PROTOTYPE_METHOD(tpl, "customProperty", CustomProperty);
    NODE_SET_PROTOTYPE_METHOD(tpl, "createResponse", CreateResponse);

    _request_constructor.Reset(isolate, tpl -> GetFunction());
}

}
