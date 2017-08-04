#include <iostream>
#include <vector>
#include <string>
#include <node.h>
#include <node_buffer.h>
#include <node_object_wrap.h>
#include <unordered_map>
#include <string>

#include "ice-cpp/ice.h"

using namespace v8;

namespace ice_node {

ice::Response *_pending_response = NULL;
Local<Object> _pending_req_obj;
Persistent<Function> _response_constructor;

class Response : public node::ObjectWrap {
    private:
        ice::Response _inst;
        Persistent<Object> reqObj;
        bool sent;

        explicit Response(ice::Response& from) : _inst(from) {
            sent = false;
        };

        ~Response() {
            if(!sent) {
                _inst.set_status(500).set_body("Response dropped without sending").send();
            }
            reqObj.Reset();
        }

        static void New(const v8::FunctionCallbackInfo<v8::Value>& args) {
            Isolate* isolate = args.GetIsolate();

            if (args.IsConstructCall()) {
                if(!_pending_response) {
                    isolate -> ThrowException(String::NewFromUtf8(isolate, "Illegal call to Response::New"));
                    return;
                }

                Response *req = new Response(*_pending_response);
                req -> reqObj.Reset(isolate, _pending_req_obj);
                _pending_response = NULL;

                req -> Wrap(args.This());

                args.GetReturnValue().Set(args.This());
            } else {
                isolate -> ThrowException(String::NewFromUtf8(isolate, "Illegal call to Response::New"));
                return;
            }
        }

        static void Send(const v8::FunctionCallbackInfo<v8::Value>& args) {
            Isolate *isolate = args.GetIsolate();
            Response *resp = node::ObjectWrap::Unwrap<Response>(args.Holder());

            if(resp -> sent) {
                isolate -> ThrowException(String::NewFromUtf8(isolate, "Response already sent"));
                return;
            }

            resp -> sent = true;

            resp -> _inst.send();
        }
    
    public:
        static Local<Object> Create(Isolate *isolate, ice::Response& from, Local<Object> _reqObj) {
            const int argc = 0;
            Local<Value> argv[argc] = { };

            _pending_response = &from;
            _pending_req_obj = _reqObj;

            Local<Function> cons = Local<Function>::New(isolate, _response_constructor);
            Local<Context> context = isolate -> GetCurrentContext();
            Local<Object> instance = cons -> NewInstance(context, argc, argv).ToLocalChecked();

            return instance;
        }

        static void Init(Isolate *isolate) {
            Local<FunctionTemplate> tpl = FunctionTemplate::New(isolate, New);
            tpl -> SetClassName(String::NewFromUtf8(isolate, "Response"));
            tpl -> InstanceTemplate() -> SetInternalFieldCount(1);

            NODE_SET_PROTOTYPE_METHOD(tpl, "send", Send);

            _response_constructor.Reset(isolate, tpl -> GetFunction());
        }
};

// Should be changed.
ice::Request *_pending_request = NULL;
Persistent<Function> _request_constructor;

class Request : public node::ObjectWrap {
    private:
        ice::Request _inst;
        bool responseCreated;

        explicit Request(ice::Request& from) : _inst(from) {
            responseCreated = false;
        }

        ~Request() {
            if(!responseCreated) {
                _inst.create_response().set_status(500).set_body("Request dropped without creating a Response").send();
            }
        }

        static void New(const v8::FunctionCallbackInfo<v8::Value>& args) {
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

        static void RemoteAddr(const v8::FunctionCallbackInfo<v8::Value>& args) {
            Isolate *isolate = args.GetIsolate();
            Request *req = node::ObjectWrap::Unwrap<Request>(args.Holder());

            args.GetReturnValue().Set(String::NewFromUtf8(isolate, req -> _inst.get_remote_addr()));
        }

        static void Method(const v8::FunctionCallbackInfo<v8::Value>& args) {
            Isolate *isolate = args.GetIsolate();
            Request *req = node::ObjectWrap::Unwrap<Request>(args.Holder());

            args.GetReturnValue().Set(String::NewFromUtf8(isolate, req -> _inst.get_method()));
        }

        static void Uri(const v8::FunctionCallbackInfo<v8::Value>& args) {
            Isolate *isolate = args.GetIsolate();
            Request *req = node::ObjectWrap::Unwrap<Request>(args.Holder());

            args.GetReturnValue().Set(String::NewFromUtf8(isolate, req -> _inst.get_uri()));
        }

        static void SessionItem(const v8::FunctionCallbackInfo<v8::Value>& args) {
            Isolate *isolate = args.GetIsolate();
            Request *req = node::ObjectWrap::Unwrap<Request>(args.Holder());

            String::Utf8Value key(args[0] -> ToString());

            if(args.Length() >= 2) {
                String::Utf8Value value(args[1] -> ToString());
                req -> _inst.set_session_item(*key, *value);
            } else {
                args.GetReturnValue().Set(String::NewFromUtf8(isolate, req -> _inst.get_session_item(*key)));
            }
        }

        static void SessionItems(const v8::FunctionCallbackInfo<v8::Value>& args) {
            Isolate *isolate = args.GetIsolate();
            Request *req = node::ObjectWrap::Unwrap<Request>(args.Holder());

            Local<Object> items = Object::New(isolate);
            auto _items = req -> _inst.get_session_items();

            for(auto& p : _items) {
                items -> Set(String::NewFromUtf8(isolate, p.first.c_str()), String::NewFromUtf8(isolate, p.second.c_str()));
            }

            args.GetReturnValue().Set(items);
        }

        static void Header(const v8::FunctionCallbackInfo<v8::Value>& args) {
            Isolate *isolate = args.GetIsolate();
            Request *req = node::ObjectWrap::Unwrap<Request>(args.Holder());

            String::Utf8Value key(args[0] -> ToString());

            args.GetReturnValue().Set(String::NewFromUtf8(isolate, req -> _inst.get_header(*key)));
        }

        static void Headers(const v8::FunctionCallbackInfo<v8::Value>& args) {
            Isolate *isolate = args.GetIsolate();
            Request *req = node::ObjectWrap::Unwrap<Request>(args.Holder());

            Local<Object> items = Object::New(isolate);
            auto _items = req -> _inst.get_headers();

            for(auto& p : _items) {
                if(p.second) items -> Set(String::NewFromUtf8(isolate, p.first.c_str()), String::NewFromUtf8(isolate, p.second));
            }

            args.GetReturnValue().Set(items);
        }

        static void Cookie(const v8::FunctionCallbackInfo<v8::Value>& args) {
            Isolate *isolate = args.GetIsolate();
            Request *req = node::ObjectWrap::Unwrap<Request>(args.Holder());

            String::Utf8Value key(args[0] -> ToString());

            args.GetReturnValue().Set(String::NewFromUtf8(isolate, req -> _inst.get_cookie(*key)));
        }

        static void Cookies(const v8::FunctionCallbackInfo<v8::Value>& args) {
            Isolate *isolate = args.GetIsolate();
            Request *req = node::ObjectWrap::Unwrap<Request>(args.Holder());

            Local<Object> items = Object::New(isolate);
            auto _items = req -> _inst.get_cookies();

            for(auto& p : _items) {
                if(p.second) items -> Set(String::NewFromUtf8(isolate, p.first.c_str()), String::NewFromUtf8(isolate, p.second));
            }

            args.GetReturnValue().Set(items);
        }

        static void CreateResponse(const v8::FunctionCallbackInfo<v8::Value>& args) {
            Isolate *isolate = args.GetIsolate();
            Request *req = node::ObjectWrap::Unwrap<Request>(args.Holder());

            auto _resp = req -> _inst.create_response();
            req -> responseCreated = true;

            args.GetReturnValue().Set(Response::Create(isolate, _resp, args.This()));
        }
    
    public:
        static Local<Object> Create(Isolate *isolate, ice::Request& from) {
            const int argc = 0;
            Local<Value> argv[argc] = { };
            _pending_request = &from;

            Local<Function> cons = Local<Function>::New(isolate, _request_constructor);
            Local<Context> context = isolate -> GetCurrentContext();
            Local<Object> instance = cons -> NewInstance(context, argc, argv).ToLocalChecked();

            return instance;
        }

        static void Init(Isolate *isolate) {
            Local<FunctionTemplate> tpl = FunctionTemplate::New(isolate, New);
            tpl -> SetClassName(String::NewFromUtf8(isolate, "Request"));
            tpl -> InstanceTemplate() -> SetInternalFieldCount(1);

            NODE_SET_PROTOTYPE_METHOD(tpl, "remoteAddr", RemoteAddr);
            NODE_SET_PROTOTYPE_METHOD(tpl, "createResponse", CreateResponse);

            _request_constructor.Reset(isolate, tpl -> GetFunction());
        }
};

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
}

NODE_MODULE(addon, InitAll)

}
