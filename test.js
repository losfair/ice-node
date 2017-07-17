const core = require("./build/Release/ice_node_core");

let server = core.create_server();
console.log(server);

core.add_endpoint(server, "/hello_world", call_info => {
    let resp = core.create_response();
    core.set_response_status(resp, 404);
    core.fire_callback(call_info, resp);
});

core.listen(server, "127.0.0.1:1122");
