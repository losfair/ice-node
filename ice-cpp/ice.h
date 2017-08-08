#ifndef _ICE_H_
#define _ICE_H_

#include <iostream>
#include <fstream>
#include <thread>
#include <vector>
#include <string>
#include <functional>
#include <unordered_map>
#include <mutex>
#include <deque>
#include <sstream>
#include <stdexcept>
#include <string.h>
#include <unistd.h>
#include <uv.h>
#include "imports.h"

namespace ice {

class Request;
class Response;
static void dispatch_task(uv_async_t *task_async);

typedef std::function<void(Request)> DispatchTarget;
typedef std::function<Response(Request)> SyncDispatchTarget;

static std::unordered_map<std::string, const char *> read_map(const u8 *raw) {
    if(!raw) {
        return std::unordered_map<std::string, const char *>();
    }

    const u8 *p = raw;
    char type_buf[64];

    u16 type_len = * (u16 *) p;
    p += 2;

    memcpy(type_buf, p, type_len);
    p += type_len;

    type_buf[type_len] = 0;
    if(strcmp(type_buf, "map") != 0) {
        //throw std::runtime_error("Data is not a map");
        return std::unordered_map<std::string, const char *>();
    }

    u32 item_count = * (u32 *) p;
    p += 4;

    std::unordered_map<std::string, const char *> ret;
    for(u32 i = 0; i < item_count; i++) {
        u32 k_len = * (u32 *) p;
        p += 4;
        const char *k = (const char *) p;
        p += k_len + 1;

        u32 v_len = * (u32 *) p;
        p += 4;
        const char *v = (const char *) p;
        p += v_len + 1;

        ret[std::string(k)] = v;
    }

    return ret;
}

struct Task {
    int ep_id;
    Resource call_info;

    Task(int _ep_id, Resource _call_info) {
        ep_id = _ep_id;
        call_info = _call_info;
    }
};

class ResponseStream {
    private:
        Resource handle;

    public:
        ResponseStream(Resource _handle) {
            handle = _handle;
        }

        ResponseStream(ResponseStream&& from) {
            handle = from._move_handle();
        }

        ~ResponseStream() {
            if(handle) {
                ice_core_destroy_stream_provider(handle);
            }
        }

        ResponseStream& operator = (ResponseStream&& from) {
            if(this != &from) {
                if(handle) {
                    ice_core_destroy_stream_provider(handle);
                }
                handle = from._move_handle();
            }
            return *this;
        }

        Resource _move_handle() {
            Resource _handle = handle;
            handle = NULL;
            return _handle;
        }

        bool write(const u8 *data, u32 len) const {
            if(!this -> handle) return false;
            ice_core_stream_provider_send_chunk(handle, data, len);
            return true;
        }

        bool write(const char *s) const {
            return write((const u8 *) s, strlen(s));
        }
};

class Context {
    private:
        Resource handle;
    
    public:
        Context(Resource _handle) {
            handle = _handle;
        }

        Resource _get_handle() {
            return handle;
        }
};

class CustomProperties {
    private:
        Resource handle;
    
    public:
        CustomProperties(Resource _handle) {
            handle = _handle;
        }

        inline std::string get(const char *k) {
            const char *v = ice_glue_custom_properties_get(handle, k);
            if(!v) v = "";
            return std::string(v);
        }

        inline void set(const char *k, const char *v) {
            ice_glue_custom_properties_set(handle, k, v);
        }
};

class Response {
    private:
        Resource call_info;
        Resource handle;

    public:
        Response(Resource _call_info) {
            call_info = _call_info;
            handle = ice_glue_create_response();
        }

        inline Response& set_body(const u8 *body, u32 len) {
            ice_glue_response_set_body(handle, body, len);
            return *this;
        }
        
        inline Response& set_body(const char *body) {
            return set_body((const u8 *) body, strlen(body));
        }

        inline Response& set_body(const std::string& body) {
            return set_body((const u8 *) body.c_str(), body.size());
        }

        inline Response& set_file(const char *path) {
            ice_glue_response_set_file(handle, path);
            return *this;
        }

        inline Response& set_status(u16 status) {
            ice_glue_response_set_status(handle, status);
            return *this;
        }

        inline Response& add_header(const char *k, const char *v) {
            ice_glue_response_add_header(handle, k, v);
            return *this;
        }

        inline Response& set_header(const char *k, const char *v) {
            return add_header(k, v);
        }

        inline Response& set_cookie(const char *k, const char *v) {
            ice_glue_response_set_cookie(handle, k, v);
            return *this;
        }

        inline ResponseStream stream(Context& ctx) {
            return ResponseStream(ice_glue_response_stream(handle, ctx._get_handle()));
        }

        inline void send() {
            ice_core_fire_callback(call_info, handle);
        }

        inline bool _consume_rendered_template(char *output) {
            return ice_glue_response_consume_rendered_template(handle, output);
        }
};

class Request {
    private:
        Resource call_info;
        Resource handle;

    public:
        Request(Resource _call_info) {
            call_info = _call_info;
            handle = ice_core_borrow_request_from_call_info(call_info);
        }

        Response create_response() {
            return Response(call_info);
        }

        inline Context get_context() {
            return Context(ice_glue_request_borrow_context(handle));
        }

        inline CustomProperties borrow_custom_properties() {
            return CustomProperties(ice_glue_request_borrow_custom_properties(handle));
        }

        inline const char * get_remote_addr() {
            return ice_glue_request_get_remote_addr(handle);
        }

        inline const char * get_method() {
            return ice_glue_request_get_method(handle);
        }

        inline const char * get_uri() {
            return ice_glue_request_get_uri(handle);
        }

        inline const char * get_session_item(const char *k) {
            return ice_glue_request_get_session_item(handle, k);
        }

        inline const char * get_stats() {
            return ice_glue_request_get_stats(handle);
        }

        inline const char * get_header(const char *k) {
            return ice_glue_request_get_header(handle, k);
        }

        inline const char * get_cookie(const char *k) {
            return ice_glue_request_get_cookie(handle, k);
        }

        inline const u8 * get_body(u32 *len_out) {
            return ice_glue_request_get_body(handle, len_out);
        }

        inline std::unordered_map<std::string, const char *> get_headers() {
            return read_map(ice_glue_request_get_headers(handle));
        }

        inline std::unordered_map<std::string, const char *> get_cookies() {
            return read_map(ice_glue_request_get_cookies(handle));
        }

        inline std::unordered_map<std::string, std::string> get_session_items() {
            auto m = read_map(ice_glue_request_get_session_items(handle));
            std::unordered_map<std::string, std::string> ret;
            for(auto& p : m) {
                // Raw data may become invalid.
                if(p.second) ret[std::move(p.first)] = std::string(p.second);
            }
            return ret;
        }

        inline void set_session_item(const char *k, const char *v) {
            ice_glue_request_set_session_item(handle, k, v);
        }

        inline void set_custom_stat(const char *k, const char *v) {
            ice_glue_request_set_custom_stat(handle, k, v);
        }

        inline bool render_template(Response& resp, const char *name, const char *data) {
            char *r = ice_glue_request_render_template_to_owned(handle, name, data);
            if(!r) {
                return false;
            }
            resp._consume_rendered_template(r);
            return true;
        }
};

class Server {
    private:
        Resource handle;
        uv_loop_t *ev_loop;
        uv_async_t task_async;
        std::unordered_map<int, DispatchTarget> dispatch_table;
        std::mutex pending_mutex;
        std::deque<Task> pending;
        std::string async_endpoint_cb_signature;

    public:
        Server(uv_loop_t *loop) {
            handle = ice_create_server();
            if(loop) {
                ev_loop = loop;
            } else {
                ev_loop = uv_default_loop();
            }
            uv_async_init(ev_loop, &task_async, ice::dispatch_task);
            task_async.data = (void *) this;

            ice_server_set_custom_app_data(handle, (void *) this);
            ice_server_set_async_endpoint_cb(handle, dispatch_async_endpoint_cb);
        }

        ~Server() {}

        void listen(const char *addr) {
            ice_server_listen(handle, addr);
        }

        bool load_bitcode(const char *name, const u8 *data, u32 len) {
            return ice_server_cervus_load_bitcode(handle, name, data, len);
        }

        bool load_bitcode_from_file(const char *name, const char *path) {
            std::ifstream file(path, std::ios::binary | std::ios::ate);
            std::streamsize size = file.tellg();
            file.seekg(0, std::ios::beg);

            std::vector<char> buffer(size);
            if(file.read(buffer.data(), size)) {
                return load_bitcode(name, (const u8 *) &buffer[0], size);
            } else {
                return false;
            }
        }

        void add_endpoint(const char *path, DispatchTarget handler, std::vector<std::string>& flags) {
            int id = -1;

            if(path && path[0]) {
                Resource ep = ice_server_router_add_endpoint(handle, path);
                for(auto& f : flags) {
                    ice_core_endpoint_set_flag(ep, f.c_str(), true);
                }

                id = ice_core_endpoint_get_id(ep);
            }

            dispatch_table[id] = handler;
        }

        void add_endpoint(const char *path, DispatchTarget handler) {
            std::vector<std::string> flags;
            add_endpoint(path, handler, flags);
        }

        void route_sync(const char *path, SyncDispatchTarget handler, std::vector<std::string>& flags) {
            add_endpoint(path, [=](Request req) {
                Response resp = handler(req);
                resp.send();
            }, flags);
        }

        void route_sync(const char *path, SyncDispatchTarget handler) {
            std::vector<std::string> flags;
            route_sync(path, handler, flags);
        }

        void route_async(const char *path, DispatchTarget handler, std::vector<std::string>& flags) {
            add_endpoint(path, handler, flags);
        }

        void route_async(const char *path, DispatchTarget handler) {
            std::vector<std::string> flags;
            route_async(path, handler, flags);
        }

        void route_threaded(const char *path, SyncDispatchTarget handler, std::vector<std::string>& flags) {
            add_endpoint(path, [=](Request req) {
                std::thread t([=]() {
                    Response resp = handler(req);
                    resp.send();
                });
                t.detach();
            }, flags);
        }
        
        void route_threaded(const char *path, SyncDispatchTarget handler) {
            std::vector<std::string> flags;
            route_threaded(path, handler, flags);
        }

        void disable_request_logging() {
            ice_server_disable_request_logging(handle);
        }

        void set_session_cookie_name(const char *name) {
            ice_server_set_session_cookie_name(handle, name);
        }

        void set_session_timeout_ms(u64 t) {
            ice_server_set_session_timeout_ms(handle, t);
        }

        bool add_template(const char *name, const char *content) {
            return ice_server_add_template(handle, name, content);
        }
        
        void set_max_request_body_size(u32 size) {
            ice_server_set_max_request_body_size(handle, size);
        }

        void set_endpoint_timeout_ms(u64 t) {
            ice_server_set_endpoint_timeout_ms(handle, t);
        }

        static void dispatch_async_endpoint_cb(int ep_id, Resource call_info) {
            Server *server = (Server *) ice_core_get_custom_app_data_from_call_info(call_info);
            server -> async_endpoint_cb(ep_id, call_info);
        }

        void async_endpoint_cb(int ep_id, Resource call_info) {
            pending_mutex.lock();
            pending.push_back(Task(ep_id, call_info));
            pending_mutex.unlock();

            uv_async_send(&task_async);
        }

        void run(const char *addr) {
            listen(addr);
            uv_run(ev_loop, UV_RUN_DEFAULT);
        }

        void dispatch_task() {
            pending_mutex.lock();
            std::deque<Task> current_tasks = std::move(pending);
            pending = std::deque<Task>();
            pending_mutex.unlock();

            for(auto& t : current_tasks) {
                Request req(t.call_info);

                auto target = dispatch_table[t.ep_id];
                if(!target) {
                    //std::cerr << "Error: Calling an invalid endpoint: " << ep_id << std::endl;
                    req.create_response().set_status(404).set_body("Invalid endpoint").send();
                    continue;
                }

                target(req);
            }
        }
};

static void dispatch_task(uv_async_t *task_async) {
    Server *server = (Server *) (task_async -> data);
    server -> dispatch_task();
}

} // namespace ice

#endif
