#ifndef _ICE_NODE_RESPONSE_H_
#define _ICE_NODE_RESPONSE_H_

#include <node.h>
#include <node_buffer.h>
#include <node_object_wrap.h>
#include "ice-cpp/ice.h"

#include "request.h"
#include "response_stream.h"

namespace ice_node {

using namespace v8;

class Response : public node::ObjectWrap {
    private:
        ice::Response _inst;
        ice::Context _ctx;
        Persistent<Object> reqObj;
        bool sent;
        bool streamCreated;

        explicit Response(ice::Response& from, ice::Context& ctx) : _inst(from), _ctx(ctx) {
            sent = false;
            streamCreated = false;
        };

        ~Response() {
            if(!sent) {
                std::cerr << "Warning: Response dropped without sending" << std::endl;
                _inst.set_status(500).send();
            }
            reqObj.Reset();
        }

        static void New(const v8::FunctionCallbackInfo<v8::Value>& args);
        static void File(const v8::FunctionCallbackInfo<v8::Value>& args);
        static void Status(const v8::FunctionCallbackInfo<v8::Value>& args);
        static void Header(const v8::FunctionCallbackInfo<v8::Value>& args);
        static void Cookie(const v8::FunctionCallbackInfo<v8::Value>& args);
        static void Stream(const v8::FunctionCallbackInfo<v8::Value>& args);
        static void Body(const v8::FunctionCallbackInfo<v8::Value>& args);
        static void Send(const v8::FunctionCallbackInfo<v8::Value>& args);
        static void RenderTemplate(const v8::FunctionCallbackInfo<v8::Value>& args);
    
    public:
        static Local<Object> Create(Isolate *isolate, ice::Response& from, Local<Object> _reqObj, ice::Context& ctx);
        static void Init(Isolate *isolate);
};

}

#endif
