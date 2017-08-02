#ifndef _ICE_CORE_IMPORTS_H_
#define _ICE_CORE_IMPORTS_H_

extern "C" {
    typedef void * Resource;
    typedef unsigned char u8;
    typedef unsigned short u16;
    typedef unsigned int u32;
    typedef unsigned long long u64;

    typedef void (*AsyncEndpointHandler) (int id, Resource call_info);
    typedef Resource (*CallbackOnRequest) (const char *uri); // returns a Response

    Resource ice_create_server();
    Resource ice_server_listen(Resource handle, const char *addr);
    Resource ice_server_router_add_endpoint(Resource handle, const char *p);
    void ice_server_set_static_dir(Resource handle, const char *d);
    void ice_server_set_session_cookie_name(Resource handle, const char *name);
    void ice_server_set_session_timeout_ms(Resource handle, u64 t);
    bool ice_server_add_template(Resource handle, const char *name, const char *content);
    void ice_server_set_max_request_body_size(Resource handle, u32 size);
    void ice_server_disable_request_logging(Resource handle);
    void ice_server_set_async_endpoint_cb(Resource handle, AsyncEndpointHandler cb);
    void ice_server_set_endpoint_timeout_ms(Resource handle, u64 t);
    void ice_server_set_custom_app_data(Resource handle, Resource data);
    
    void ice_context_set_custom_app_data(Resource handle, Resource data);

    const char * ice_glue_request_get_remote_addr(Resource req);
    const char * ice_glue_request_get_method(Resource req);
    const char * ice_glue_request_get_uri(Resource req);
    const char * ice_glue_request_get_session_id(Resource req);
    const char * ice_glue_request_get_session_item(Resource req, const char *k);
    void ice_glue_request_set_session_item(Resource req, const char *k, const char *v);
    const char * ice_glue_request_get_stats(Resource req);
    void ice_glue_request_set_custom_stat(Resource req, const char *k, const char *v);
    void ice_glue_request_add_header(Resource t, const char *k, const char *v);
    const char * ice_glue_request_get_header(Resource t, const char *k);
    const char * ice_glue_request_get_cookie(Resource t, const char *k);
    const u8 * ice_glue_request_get_body(Resource t, u32 *len_out);
    char * ice_glue_request_render_template_to_owned(Resource t, const char *name, const char *data);
    Resource ice_glue_request_borrow_context(Resource t);

    Resource ice_glue_request_create_header_iterator(Resource t);
    const char * ice_glue_request_header_iterator_next(Resource t, Resource itr_p);
    void ice_glue_destroy_header_iterator(Resource itr_p);

    Resource ice_glue_create_response();
    void ice_glue_response_set_body(Resource t, const u8 *body, u32 len);
    void ice_glue_response_set_file(Resource t, const char *path);
    void ice_glue_response_set_status(Resource t, u16 status);
    bool ice_glue_response_consume_rendered_template(Resource t, char *output);
    void ice_glue_response_add_header(Resource t, const char *k, const char *v);
    const char * ice_glue_response_get_header(Resource t, const char *k);
    void ice_glue_response_set_cookie(Resource t, const char *k, const char *v, const char *options);
    Resource ice_glue_response_stream(Resource t, Resource ctx);

    bool ice_core_fire_callback(Resource call_info, Resource resp);
    Resource ice_core_borrow_request_from_call_info(Resource call_info);
    Resource ice_core_get_custom_app_data_from_call_info(Resource call_info);
    int ice_core_endpoint_get_id(Resource ep);
    void ice_core_endpoint_set_flag(Resource ep, const char *name, bool value);
    void ice_core_stream_provider_send_chunk(Resource sp, const u8 *data, u32 len);
    void ice_core_destroy_stream_provider(Resource sp);
}

#endif
