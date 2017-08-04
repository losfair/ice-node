#include <node.h>
#include <node_buffer.h>
#include <node_object_wrap.h>

#include "response_stream.h"

using namespace v8;

namespace ice_node {

static ice::ResponseStream *_pending_owned_response_stream = NULL;
static Persistent<Function> _response_stream_constructor;

void ResponseStream::New(const v8::FunctionCallbackInfo<v8::Value>& args) {
    Isolate* isolate = args.GetIsolate();

    if (args.IsConstructCall()) {
        if(!_pending_owned_response_stream) {
            isolate -> ThrowException(String::NewFromUtf8(isolate, "Illegal call to ResponseStream::New"));
            return;
        }

        ResponseStream *req = new ResponseStream(_pending_owned_response_stream);
        _pending_owned_response_stream = NULL;

        req -> Wrap(args.This());

        args.GetReturnValue().Set(args.This());
    } else {
        isolate -> ThrowException(String::NewFromUtf8(isolate, "Illegal call to ResponseStream::New"));
        return;
    }
}

void ResponseStream::Write(const v8::FunctionCallbackInfo<v8::Value>& args) {
    Isolate *isolate = args.GetIsolate();
    ResponseStream *resp = node::ObjectWrap::Unwrap<ResponseStream>(args.Holder());

    if(!resp -> _inst) {
        isolate -> ThrowException(String::NewFromUtf8(isolate, "ResponseStream already closed"));
        return;
    }

    if(!args[0] -> IsObject()) {
        isolate -> ThrowException(String::NewFromUtf8(isolate, "ResponseStream::Write: Buffer required"));
        return;
    }

    Local<Object> bufObj = Local<Object>::Cast(args[0]);

    u8 *data = (u8 *) node::Buffer::Data(bufObj);
    u32 dataLen = node::Buffer::Length(bufObj);

    if(!data) {
        isolate -> ThrowException(String::NewFromUtf8(isolate, "ResponseStream::Write: Invalid buffer"));
        return;
    }

    resp -> _inst -> write(data, dataLen);
}

void ResponseStream::Close(const v8::FunctionCallbackInfo<v8::Value>& args) {
    Isolate *isolate = args.GetIsolate();
    ResponseStream *resp = node::ObjectWrap::Unwrap<ResponseStream>(args.Holder());

    if(!resp -> _inst) {
        isolate -> ThrowException(String::NewFromUtf8(isolate, "ResponseStream already closed"));
        return;
    }

    delete resp -> _inst;
    resp -> _inst = NULL;
}

Local<Object> ResponseStream::Create(Isolate *isolate, ice::ResponseStream *inst) {
    const int argc = 0;
    Local<Value> argv[argc] = { };

    _pending_owned_response_stream = inst;

    Local<Function> cons = Local<Function>::New(isolate, _response_stream_constructor);
    Local<Context> context = isolate -> GetCurrentContext();
    Local<Object> instance = cons -> NewInstance(context, argc, argv).ToLocalChecked();

    return instance;
}

void ResponseStream::Init(Isolate *isolate) {
    Local<FunctionTemplate> tpl = FunctionTemplate::New(isolate, New);
    tpl -> SetClassName(String::NewFromUtf8(isolate, "ResponseStream"));
    tpl -> InstanceTemplate() -> SetInternalFieldCount(1);

    NODE_SET_PROTOTYPE_METHOD(tpl, "write", Write);
    NODE_SET_PROTOTYPE_METHOD(tpl, "close", Close);

    _response_stream_constructor.Reset(isolate, tpl -> GetFunction());
}

}
