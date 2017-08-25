#ifndef _ICE_NODE_REQUEST_H_
#define _ICE_NODE_REQUEST_H_

#include <node.h>
#include <node_buffer.h>
#include <node_object_wrap.h>
#include "ice-cpp/ice.h"

#include "response.h"

namespace ice_node {

using namespace v8;

class Request : public node::ObjectWrap {
    public:
        bool responseSent;

    private:
        bool responseCreated;

        explicit Request(ice::Request& from) : _inst(from) {
            responseCreated = false;
            responseSent = false;
        }

        ~Request() {
            if(!responseCreated) {
                std::cerr << "Warning: Request dropped without creating a Response" << std::endl;
                _inst.create_response().set_status(500).send();
            }
        }

        static void New(const v8::FunctionCallbackInfo<v8::Value>& args);
        static void RemoteAddr(const v8::FunctionCallbackInfo<v8::Value>& args);
        static void Method(const v8::FunctionCallbackInfo<v8::Value>& args);
        static void Uri(const v8::FunctionCallbackInfo<v8::Value>& args);
        static void SessionItem(const v8::FunctionCallbackInfo<v8::Value>& args);
        static void SessionItems(const v8::FunctionCallbackInfo<v8::Value>& args);
        static void Header(const v8::FunctionCallbackInfo<v8::Value>& args);
        static void Headers(const v8::FunctionCallbackInfo<v8::Value>& args);
        static void Cookie(const v8::FunctionCallbackInfo<v8::Value>& args);
        static void Cookies(const v8::FunctionCallbackInfo<v8::Value>& args);
        static void Body(const v8::FunctionCallbackInfo<v8::Value>& args);
        static void CustomProperty(const v8::FunctionCallbackInfo<v8::Value>& args);
        static void CreateResponse(const v8::FunctionCallbackInfo<v8::Value>& args);
        static void UrlParams(const v8::FunctionCallbackInfo<v8::Value>& args);
    
    public:
        ice::Request _inst;

        static Local<Object> Create(Isolate *isolate, ice::Request& from);
        static void Init(Isolate *isolate);
};

}

#endif
