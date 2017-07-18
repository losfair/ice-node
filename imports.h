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
    void ice_server_set_session_timeout_ms(Resource handle, u64 t);

    const char * ice_glue_request_get_remote_addr(Resource req);
    const char * ice_glue_request_get_method(Resource req);
    const char * ice_glue_request_get_uri(Resource req);
    bool ice_glue_request_load_session(Resource req, const char *id);
    void ice_glue_request_create_session(Resource req);
    const char * ice_glue_request_get_session_id(Resource req);
    const char * ice_glue_request_get_session_item(Resource req, const char *k);
    void ice_glue_request_set_session_item(Resource req, const char *k, const char *v);
    void ice_glue_request_remove_session_item(Resource req, const char *k);

    void ice_glue_request_add_header(Resource t, const char *k, const char *v);
    const char * ice_glue_request_get_header(Resource t, const char *k);

    Resource ice_glue_request_create_header_iterator(Resource t);
    const char * ice_glue_request_header_iterator_next(Resource t, Resource itr_p);
    void ice_glue_destroy_header_iterator(Resource itr_p);

    void ice_glue_response_add_header(Resource t, const char *k, const char *v);
    const char * ice_glue_response_get_header(Resource t, const char *k);
    void ice_glue_response_set_cookie(Resource t, const char *k, const char *v, const char *options);

    Resource ice_glue_create_response();
    void ice_glue_response_set_body(Resource t, const u8 *body, u32 len);
    const u8 * ice_glue_request_get_body(Resource t, u32 *len_out);

    void ice_glue_response_set_status(Resource t, u16 status);

    void ice_glue_register_async_endpoint_handler(AsyncEndpointHandler);

    void ice_core_fire_callback(Resource call_info, Resource resp);
    Resource ice_core_borrow_request_from_call_info(Resource call_info);
    int ice_core_endpoint_get_id(Resource ep);

    void ice_core_endpoint_set_flag(Resource ep, const char *name, bool value);
}
