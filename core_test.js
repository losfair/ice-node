const core = require("./build/Release/ice_node_v4_core");

let cfg = core.http_server_config_create();
core.http_server_config_set_listen_addr(cfg, "127.0.0.1:6018");
core.http_server_config_set_num_executors(cfg, 4);
let server = core.http_server_create(cfg);
core.http_server_start(server);
console.log("OK");
