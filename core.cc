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

ice::ResponseStream *_pending_owned_response_stream = NULL;
Persistent<Function> _response_stream_constructor;

class ResponseStream : public node::ObjectWrap {
    private:
        ice::ResponseStream *_inst;

        explicit ResponseStream(ice::ResponseStream *inst) {
            _inst = inst;
        }

        ~ResponseStream() {
            if(_inst) delete _inst;
        }

        static void New(const v8::FunctionCallbackInfo<v8::Value>& args) {
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

        static void Write(const v8::FunctionCallbackInfo<v8::Value>& args) {
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

        static void Close(const v8::FunctionCallbackInfo<v8::Value>& args) {
            Isolate *isolate = args.GetIsolate();
            ResponseStream *resp = node::ObjectWrap::Unwrap<ResponseStream>(args.Holder());

            if(!resp -> _inst) {
                isolate -> ThrowException(String::NewFromUtf8(isolate, "ResponseStream already closed"));
                return;
            }

            delete resp -> _inst;
            resp -> _inst = NULL;
        }
    
    public:
        static Local<Object> Create(Isolate *isolate, ice::ResponseStream *inst) {
            const int argc = 0;
            Local<Value> argv[argc] = { };

            _pending_owned_response_stream = inst;

            Local<Function> cons = Local<Function>::New(isolate, _response_stream_constructor);
            Local<Context> context = isolate -> GetCurrentContext();
            Local<Object> instance = cons -> NewInstance(context, argc, argv).ToLocalChecked();

            return instance;
        }

        static void Init(Isolate *isolate) {
            Local<FunctionTemplate> tpl = FunctionTemplate::New(isolate, New);
            tpl -> SetClassName(String::NewFromUtf8(isolate, "ResponseStream"));
            tpl -> InstanceTemplate() -> SetInternalFieldCount(1);

            NODE_SET_PROTOTYPE_METHOD(tpl, "write", Write);
            NODE_SET_PROTOTYPE_METHOD(tpl, "close", Close);

            _response_stream_constructor.Reset(isolate, tpl -> GetFunction());
        }
};

ice::Response *_pending_response = NULL;
ice::Context *_pending_response_context = NULL;
Local<Object> _pending_req_obj;
Persistent<Function> _response_constructor;

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
                _inst.set_status(500).set_body("Response dropped without sending").send();
            }
            reqObj.Reset();
        }

        static void New(const v8::FunctionCallbackInfo<v8::Value>& args) {
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

        static void File(const v8::FunctionCallbackInfo<v8::Value>& args) {
            Isolate *isolate = args.GetIsolate();
            Response *resp = node::ObjectWrap::Unwrap<Response>(args.Holder());

            if(resp -> sent) {
                isolate -> ThrowException(String::NewFromUtf8(isolate, "Response already sent"));
                return;
            }

            String::Utf8Value path(args[0] -> ToString());
            resp -> _inst.set_file(*path);
        }

        static void Status(const v8::FunctionCallbackInfo<v8::Value>& args) {
            Isolate *isolate = args.GetIsolate();
            Response *resp = node::ObjectWrap::Unwrap<Response>(args.Holder());

            if(resp -> sent) {
                isolate -> ThrowException(String::NewFromUtf8(isolate, "Response already sent"));
                return;
            }

            resp -> _inst.set_status(args[0] -> NumberValue());
        }

        static void Header(const v8::FunctionCallbackInfo<v8::Value>& args) {
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

        static void Cookie(const v8::FunctionCallbackInfo<v8::Value>& args) {
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

        static void Stream(const v8::FunctionCallbackInfo<v8::Value>& args) {
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

        static void Body(const v8::FunctionCallbackInfo<v8::Value>& args) {
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
        static Local<Object> Create(Isolate *isolate, ice::Response& from, Local<Object> _reqObj, ice::Context& ctx) {
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

        static void Init(Isolate *isolate) {
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

            if(req -> responseCreated) {
                isolate -> ThrowException(String::NewFromUtf8(isolate, "Request is no longer valid once a Response is created"));
                return;
            }

            args.GetReturnValue().Set(String::NewFromUtf8(isolate, req -> _inst.get_remote_addr()));
        }

        static void Method(const v8::FunctionCallbackInfo<v8::Value>& args) {
            Isolate *isolate = args.GetIsolate();
            Request *req = node::ObjectWrap::Unwrap<Request>(args.Holder());

            if(req -> responseCreated) {
                isolate -> ThrowException(String::NewFromUtf8(isolate, "Request is no longer valid once a Response is created"));
                return;
            }

            args.GetReturnValue().Set(String::NewFromUtf8(isolate, req -> _inst.get_method()));
        }

        static void Uri(const v8::FunctionCallbackInfo<v8::Value>& args) {
            Isolate *isolate = args.GetIsolate();
            Request *req = node::ObjectWrap::Unwrap<Request>(args.Holder());

            if(req -> responseCreated) {
                isolate -> ThrowException(String::NewFromUtf8(isolate, "Request is no longer valid once a Response is created"));
                return;
            }

            args.GetReturnValue().Set(String::NewFromUtf8(isolate, req -> _inst.get_uri()));
        }

        static void SessionItem(const v8::FunctionCallbackInfo<v8::Value>& args) {
            Isolate *isolate = args.GetIsolate();
            Request *req = node::ObjectWrap::Unwrap<Request>(args.Holder());

            if(req -> responseCreated) {
                isolate -> ThrowException(String::NewFromUtf8(isolate, "Request is no longer valid once a Response is created"));
                return;
            }

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

            if(req -> responseCreated) {
                isolate -> ThrowException(String::NewFromUtf8(isolate, "Request is no longer valid once a Response is created"));
                return;
            }

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

            if(req -> responseCreated) {
                isolate -> ThrowException(String::NewFromUtf8(isolate, "Request is no longer valid once a Response is created"));
                return;
            }

            String::Utf8Value key(args[0] -> ToString());

            args.GetReturnValue().Set(String::NewFromUtf8(isolate, req -> _inst.get_header(*key)));
        }

        static void Headers(const v8::FunctionCallbackInfo<v8::Value>& args) {
            Isolate *isolate = args.GetIsolate();
            Request *req = node::ObjectWrap::Unwrap<Request>(args.Holder());

            if(req -> responseCreated) {
                isolate -> ThrowException(String::NewFromUtf8(isolate, "Request is no longer valid once a Response is created"));
                return;
            }

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

            if(req -> responseCreated) {
                isolate -> ThrowException(String::NewFromUtf8(isolate, "Request is no longer valid once a Response is created"));
                return;
            }

            String::Utf8Value key(args[0] -> ToString());

            args.GetReturnValue().Set(String::NewFromUtf8(isolate, req -> _inst.get_cookie(*key)));
        }

        static void Cookies(const v8::FunctionCallbackInfo<v8::Value>& args) {
            Isolate *isolate = args.GetIsolate();
            Request *req = node::ObjectWrap::Unwrap<Request>(args.Holder());

            if(req -> responseCreated) {
                isolate -> ThrowException(String::NewFromUtf8(isolate, "Request is no longer valid once a Response is created"));
                return;
            }

            Local<Object> items = Object::New(isolate);
            auto _items = req -> _inst.get_cookies();

            for(auto& p : _items) {
                if(p.second) items -> Set(String::NewFromUtf8(isolate, p.first.c_str()), String::NewFromUtf8(isolate, p.second));
            }

            args.GetReturnValue().Set(items);
        }

        static void Body(const v8::FunctionCallbackInfo<v8::Value>& args) {
            Isolate *isolate = args.GetIsolate();
            Request *req = node::ObjectWrap::Unwrap<Request>(args.Holder());

            if(req -> responseCreated) {
                isolate -> ThrowException(String::NewFromUtf8(isolate, "Request is no longer valid once a Response is created"));
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

        static void CreateResponse(const v8::FunctionCallbackInfo<v8::Value>& args) {
            Isolate *isolate = args.GetIsolate();
            Request *req = node::ObjectWrap::Unwrap<Request>(args.Holder());

            if(req -> responseCreated) {
                isolate -> ThrowException(String::NewFromUtf8(isolate, "Request is no longer valid once a Response is created"));
                return;
            }

            auto _resp = req -> _inst.create_response();
            req -> responseCreated = true;

            auto ctx = req -> _inst.get_context();

            args.GetReturnValue().Set(Response::Create(isolate, _resp, args.This(), ctx));
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
            NODE_SET_PROTOTYPE_METHOD(tpl, "method", Method);
            NODE_SET_PROTOTYPE_METHOD(tpl, "uri", Uri);
            NODE_SET_PROTOTYPE_METHOD(tpl, "sessionItem", SessionItem);
            NODE_SET_PROTOTYPE_METHOD(tpl, "sessionItems", SessionItems);
            NODE_SET_PROTOTYPE_METHOD(tpl, "header", Header);
            NODE_SET_PROTOTYPE_METHOD(tpl, "headers", Headers);
            NODE_SET_PROTOTYPE_METHOD(tpl, "cookie", Cookie);
            NODE_SET_PROTOTYPE_METHOD(tpl, "cookies", Cookies);
            NODE_SET_PROTOTYPE_METHOD(tpl, "body", Body);
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
    ResponseStream::Init(isolate);
}

NODE_MODULE(addon, InitAll)

}
