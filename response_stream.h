#ifndef _ICE_NODE_RESPONSE_STREAM_H_
#define _ICE_NODE_RESPONSE_STREAM_H_

#include <node.h>
#include <node_buffer.h>
#include <node_object_wrap.h>
#include "ice-cpp/ice.h"

namespace ice_node {

using namespace v8;

class ResponseStream : public node::ObjectWrap {
    private:
        ice::ResponseStream *_inst;

        explicit ResponseStream(ice::ResponseStream *inst) {
            _inst = inst;
        }

        ~ResponseStream() {
            if(_inst) delete _inst;
        }

        static void New(const v8::FunctionCallbackInfo<v8::Value>& args);
        static void Write(const v8::FunctionCallbackInfo<v8::Value>& args);
        static void Close(const v8::FunctionCallbackInfo<v8::Value>& args);
    
    public:
        static Local<Object> Create(Isolate *isolate, ice::ResponseStream *inst);
        static void Init(Isolate *isolate);
};

}

#endif
