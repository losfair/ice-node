#include <node.h>
#include <node_buffer.h>
#include <node_object_wrap.h>

#include "response.h"

using namespace v8;

namespace ice_node {

static ice::Response *_pending_response = NULL;
static ice::Context *_pending_response_context = NULL;
static Local<Object> _pending_req_obj;
static Persistent<Function> _response_constructor;

void Response::New(const v8::FunctionCallbackInfo<v8::Value>& args) {
    Isolate* isolate = args.GetIsolate();

    if (args.IsConstructCall()) {
        if(!_pending_response || !_pending_response_context) {
            isolate -> ThrowException(String::NewFromUtf8(isolate, "Illegal call to Response::New"));
            return;
        }

        Response *req = new Response(*_pending_response, *_pending_response_context);
        req -> reqObj.Reset(isolate, _pending_req_obj);
        _pending_response = NULL;

        req -> Wrap(args.This());

        args.GetReturnValue().Set(args.This());
    } else {
        isolate -> ThrowException(String::NewFromUtf8(isolate, "Illegal call to Response::New"));
        return;
    }
}

void Response::File(const v8::FunctionCallbackInfo<v8::Value>& args) {
    Isolate *isolate = args.GetIsolate();
    Response *resp = node::ObjectWrap::Unwrap<Response>(args.Holder());

    if(resp -> sent) {
        isolate -> ThrowException(String::NewFromUtf8(isolate, "Response already sent"));
        return;
    }

    String::Utf8Value path(args[0] -> ToString());
    resp -> _inst.set_file(*path);
}

void Response::Status(const v8::FunctionCallbackInfo<v8::Value>& args) {
    Isolate *isolate = args.GetIsolate();
    Response *resp = node::ObjectWrap::Unwrap<Response>(args.Holder());

    if(resp -> sent) {
        isolate -> ThrowException(String::NewFromUtf8(isolate, "Response already sent"));
        return;
    }

    resp -> _inst.set_status(args[0] -> NumberValue());
}

void Response::Header(const v8::FunctionCallbackInfo<v8::Value>& args) {
    Isolate *isolate = args.GetIsolate();
    Response *resp = node::ObjectWrap::Unwrap<Response>(args.Holder());

    if(resp -> sent) {
        isolate -> ThrowException(String::NewFromUtf8(isolate, "Response already sent"));
        return;
    }

    String::Utf8Value k(args[0] -> ToString());
    String::Utf8Value v(args[1] -> ToString());
    resp -> _inst.set_header(*k, *v);
}

void Response::Cookie(const v8::FunctionCallbackInfo<v8::Value>& args) {
    Isolate *isolate = args.GetIsolate();
    Response *resp = node::ObjectWrap::Unwrap<Response>(args.Holder());

    if(resp -> sent) {
        isolate -> ThrowException(String::NewFromUtf8(isolate, "Response already sent"));
        return;
    }

    String::Utf8Value k(args[0] -> ToString());
    String::Utf8Value v(args[1] -> ToString());
    resp -> _inst.set_cookie(*k, *v);
}

void Response::Stream(const v8::FunctionCallbackInfo<v8::Value>& args) {
    Isolate *isolate = args.GetIsolate();
    Response *resp = node::ObjectWrap::Unwrap<Response>(args.Holder());

    if(resp -> sent) {
        isolate -> ThrowException(String::NewFromUtf8(isolate, "Response already sent"));
        return;
    }

    if(resp -> streamCreated) {
        isolate -> ThrowException(String::NewFromUtf8(isolate, "Stream already created"));
        return;
    }

    resp -> streamCreated = true;
    Local<Object> stream = ResponseStream::Create(isolate, new ice::ResponseStream(resp -> _inst.stream(resp -> _ctx)));
    args.GetReturnValue().Set(stream);
}

void Response::Body(const v8::FunctionCallbackInfo<v8::Value>& args) {
    Isolate *isolate = args.GetIsolate();
    Response *resp = node::ObjectWrap::Unwrap<Response>(args.Holder());

    if(resp -> sent) {
        isolate -> ThrowException(String::NewFromUtf8(isolate, "Response already sent"));
        return;
    }

    if(!args[0] -> IsObject()) {
        isolate -> ThrowException(String::NewFromUtf8(isolate, "Response::Body: Buffer required"));
        return;
    }

    Local<Object> bufObj = Local<Object>::Cast(args[0]);

    u8 *data = (u8 *) node::Buffer::Data(bufObj);
    u32 dataLen = node::Buffer::Length(bufObj);

    if(!data) {
        isolate -> ThrowException(String::NewFromUtf8(isolate, "Response::Body: Invalid buffer"));
        return;
    }

    resp -> _inst.set_body(data, dataLen);
}

void Response::RenderTemplate(const v8::FunctionCallbackInfo<v8::Value>& args) {
    Isolate *isolate = args.GetIsolate();
    Response *resp = node::ObjectWrap::Unwrap<Response>(args.Holder());

    Local<Object> localReqObj = Local<Object>::New(isolate, resp -> reqObj);
    Request *req = node::ObjectWrap::Unwrap<Request>(localReqObj);

    if(!req) {
        isolate -> ThrowException(String::NewFromUtf8(isolate, "Response::RenderTemplate: Internal error: Unable to get Request object"));
        return;
    }

    String::Utf8Value name(args[0] -> ToString());
    String::Utf8Value data(args[1] -> ToString());

    bool ret = req -> _inst.render_template(resp -> _inst, *name, *data);
    if(!ret) {
        isolate -> ThrowException(String::NewFromUtf8(isolate, "Response::RenderTemplate: Failed"));
        return;
    }
}

void Response::Send(const v8::FunctionCallbackInfo<v8::Value>& args) {
    Isolate *isolate = args.GetIsolate();
    Response *resp = node::ObjectWrap::Unwrap<Response>(args.Holder());

    if(resp -> sent) {
        isolate -> ThrowException(String::NewFromUtf8(isolate, "Response already sent"));
        return;
    }

    resp -> sent = true;

    Local<Object> localReqObj = Local<Object>::New(isolate, resp -> reqObj);
    Request *req = node::ObjectWrap::Unwrap<Request>(localReqObj);

    if(!req) {
        isolate -> ThrowException(String::NewFromUtf8(isolate, "Response::Send: Internal error: Unable to get Request object"));
        return;
    }

    req -> responseSent = true;

    resp -> _inst.send();
}

Local<Object> Response::Create(Isolate *isolate, ice::Response& from, Local<Object> _reqObj, ice::Context& ctx) {
    const int argc = 0;
    Local<Value> argv[argc] = { };

    _pending_response = &from;
    _pending_req_obj = _reqObj;
    _pending_response_context = &ctx;

    Local<Function> cons = Local<Function>::New(isolate, _response_constructor);
    Local<Context> context = isolate -> GetCurrentContext();
    Local<Object> instance = cons -> NewInstance(context, argc, argv).ToLocalChecked();

    return instance;
}

void Response::Init(Isolate *isolate) {
    Local<FunctionTemplate> tpl = FunctionTemplate::New(isolate, New);
    tpl -> SetClassName(String::NewFromUtf8(isolate, "Response"));
    tpl -> InstanceTemplate() -> SetInternalFieldCount(1);

    NODE_SET_PROTOTYPE_METHOD(tpl, "body", Body);
    NODE_SET_PROTOTYPE_METHOD(tpl, "file", File);
    NODE_SET_PROTOTYPE_METHOD(tpl, "status", Status);
    NODE_SET_PROTOTYPE_METHOD(tpl, "header", Header);
    NODE_SET_PROTOTYPE_METHOD(tpl, "cookie", Cookie);
    NODE_SET_PROTOTYPE_METHOD(tpl, "stream", Stream);
    NODE_SET_PROTOTYPE_METHOD(tpl, "send", Send);
    NODE_SET_PROTOTYPE_METHOD(tpl, "renderTemplate", RenderTemplate);

    _response_constructor.Reset(isolate, tpl -> GetFunction());
}

}