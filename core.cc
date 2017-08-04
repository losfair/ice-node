#include <iostream>
#include <vector>
#include <string>
#include <node.h>
#include <node_buffer.h>
#include <node_object_wrap.h>
#include <unordered_map>
#include <string>

#include "ice-cpp/ice.h"
#include "request.h"
#include "response.h"
#include "response_stream.h"

using namespace v8;

namespace ice_node {

Persistent<Function> _server_constructor;

class Server : public node::ObjectWrap {
    private:
        ice::Server _inst;
        Persistent<Object> persistent_self;

        explicit Server(Isolate *isolate, Local<Object> options) : _inst(NULL) {
            Local<Value> opt_disable_request_logging = options -> Get(String::NewFromUtf8(isolate, "disable_request_logging"));
            if(opt_disable_request_logging -> IsBoolean()) {
                bool v = opt_disable_request_logging -> BooleanValue();
                if(v) {
                    _inst.disable_request_logging();
                }
            }

            Local<Value> opt_session_timeout_ms = options -> Get(String::NewFromUtf8(isolate, "session_timeout_ms"));
            if(opt_session_timeout_ms -> IsNumber()) {
                u64 t = opt_session_timeout_ms -> NumberValue();
                _inst.set_session_timeout_ms(t);
            }

            Local<Value> opt_session_cookie = options -> Get(String::NewFromUtf8(isolate, "session_cookie"));
            if(opt_session_cookie -> IsString()) {
                String::Utf8Value _name(opt_session_cookie -> ToString());
                const char *name = *_name;
                if(name[0]) {
                    _inst.set_session_cookie_name(name);
                }
            }

            Local<Value> opt_max_request_body_size = options -> Get(String::NewFromUtf8(isolate, "max_request_body_size"));
            if(opt_max_request_body_size -> IsNumber()) {
                u32 size = opt_max_request_body_size -> NumberValue();
                _inst.set_max_request_body_size(size);
            }

            Local<Value> opt_endpoint_timeout_ms = options -> Get(String::NewFromUtf8(isolate, "endpoint_timeout_ms"));
            if(opt_endpoint_timeout_ms -> IsNumber()) {
                u64 t = opt_endpoint_timeout_ms -> NumberValue();
                _inst.set_endpoint_timeout_ms(t);
            }
        }

        static void New(const v8::FunctionCallbackInfo<v8::Value>& args) {
            Isolate* isolate = args.GetIsolate();

            if (args.IsConstructCall()) {
                // Invoked as constructor: `new MyObject(...)`
                if(!args[0] -> IsObject()) {
                    isolate -> ThrowException(String::NewFromUtf8(isolate, "Invalid options"));
                    return;
                }

                Local<Object> options = Local<Object>::Cast(args[0]);

                Server *s = new Server(isolate, options);
                s -> Wrap(args.This());

                s -> persistent_self.Reset(isolate, args.This());

                args.GetReturnValue().Set(args.This());
            } else {
                // Invoked as plain function `MyObject(...)`, turn into construct call.
                const int argc = 1;
                Local<Value> argv[argc] = { args[0] };
                Local<Function> cons = Local<Function>::New(isolate, _server_constructor);
                Local<Context> context = isolate -> GetCurrentContext();
                Local<Object> instance = cons -> NewInstance(context, argc, argv).ToLocalChecked();
                args.GetReturnValue().Set(instance);
            }
        }

        static void Route(const FunctionCallbackInfo<Value>& args) {
            Isolate *isolate = args.GetIsolate();

            Server *s = node::ObjectWrap::Unwrap<Server>(args.Holder());

            if(!args[0] -> IsString() || !args[1] -> IsFunction()) {
                isolate -> ThrowException(String::NewFromUtf8(isolate, "Server::Route: Invalid parameters"));
                return;
            }

            std::vector<std::string> flags;
            if(args[2] -> IsArray()) {
                Local<Array> arr = Local<Array>::Cast(args[2]);
                for(unsigned int i = 0; i < arr -> Length(); i++) {
                    String::Utf8Value name(arr -> Get(i) -> ToString());
                    flags.push_back(*name);
                }
            }
            
            String::Utf8Value path(args[0] -> ToString());
            Local<Function> local_cb = Local<Function>::Cast(args[1]);
            Persistent<Function> *cb = new Persistent<Function>(isolate, local_cb);

            s -> _inst.route_async(*path, [cb](ice::Request req) {
                Isolate *isolate = Isolate::GetCurrent();
                HandleScope scope(isolate);

                Local<Function> local_cb = Local<Function>::New(isolate, *cb);
                Local<Object> req_obj = Request::Create(isolate, req);

                Local<Value> argv[] = { req_obj };

                node::MakeCallback(isolate, Object::New(isolate), local_cb, 1, argv);
            }, flags);
        }

        static void AddTemplate(const FunctionCallbackInfo<Value>& args) {
            Isolate *isolate = args.GetIsolate();
            Server *s = node::ObjectWrap::Unwrap<Server>(args.Holder());

            String::Utf8Value name(args[0] -> ToString());
            String::Utf8Value content(args[1] -> ToString());

            bool ret = s -> _inst.add_template(*name, *content);
            if(!ret) {
                isolate -> ThrowException(String::NewFromUtf8(isolate, "Server::AddTemplate: Failed"));
                return;
            }
        }

        static void Listen(const FunctionCallbackInfo<Value>& args) {
            Isolate *isolate = args.GetIsolate();
            Server *s = node::ObjectWrap::Unwrap<Server>(args.Holder());

            if(!args[0] -> IsString()) {
                isolate -> ThrowException(String::NewFromUtf8(isolate, "Server::Listen: Invalid parameters"));
                return;
            }

            String::Utf8Value addr(args[0] -> ToString());
            s -> _inst.listen(*addr);
        }

    public:
        static Local<Function> Init(Isolate *isolate) {
            Local<FunctionTemplate> tpl = FunctionTemplate::New(isolate, New);
            tpl -> SetClassName(String::NewFromUtf8(isolate, "Server"));
            tpl -> InstanceTemplate() -> SetInternalFieldCount(1);

            NODE_SET_PROTOTYPE_METHOD(tpl, "route", Route);
            NODE_SET_PROTOTYPE_METHOD(tpl, "addTemplate", AddTemplate);
            NODE_SET_PROTOTYPE_METHOD(tpl, "listen", Listen);

            _server_constructor.Reset(isolate, tpl -> GetFunction());
            return tpl -> GetFunction();
        }
};

void InitAll(Local<Object> exports, Local<Object> module) {
    Isolate *isolate = exports -> GetIsolate();
    exports -> Set(String::NewFromUtf8(isolate, "Server"), Server::Init(isolate));
    Request::Init(isolate);
    Response::Init(isolate);
    ResponseStream::Init(isolate);
}

NODE_MODULE(addon, InitAll)

}
