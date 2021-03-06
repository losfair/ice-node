const core = require("./build/Release/ice_node_v4_core");

let cfg = core.http_server_config_create();
core.http_server_config_set_listen_addr(cfg, "127.0.0.1:6018");
core.http_server_config_set_num_executors(cfg, 4);
let server = core.http_server_create(cfg);
core.http_server_start(server);

let rt = core.http_server_route_create("/hello_world", (ctx, req) => {
    let resp = core.http_response_create();
    core.http_response_set_body(
        resp,
        Buffer.from("Hello world!")
    );
    core.http_response_set_header(
        resp,
        "X-Powered-By",
        "Ice-node"
    );
    core.http_server_endpoint_context_end_with_response(
        ctx,
        resp
    );
});
core.http_server_add_route(server, rt);
