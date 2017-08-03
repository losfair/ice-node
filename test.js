const core = require("./build/Release/ice_node_core");

let server = core.create_server();
console.log(server);
core.disable_request_logging(server);

core.add_endpoint(server, "", call_info => {
    let resp = core.create_response(call_info);
    core.set_response_status(resp, 404);
    core.set_response_body(resp, Buffer.from("Not found"));
    core.fire_callback(call_info, resp);
});

core.add_endpoint(server, "/hello_world", call_info => {
    //core.get_request_info(call_info);
    let resp = core.create_response(call_info);
    //core.set_response_body(resp, Buffer.from("Hello world!"));
    core.fire_callback(call_info, resp);
});

core.add_endpoint(server, "/delayed", call_info => {
    setTimeout(() => {
        let resp = core.create_response(call_info);
        core.fire_callback(call_info, resp);
    }, 10);
});

core.add_endpoint(server, "/echo", call_info => {
    let info = core.get_request_info(call_info);
    let body = core.get_request_body(call_info);
    let r = `
Remote address: ${info.remote_addr}
Request URI: ${info.uri}
Request method: ${info.method}
Request body:
${body}
    `.trim() + "\n";
    let resp = core.create_response(call_info);
    core.set_response_header(resp, "Content-Type", "text/plain");
    core.set_response_body(resp, Buffer.from(r));
    core.fire_callback(call_info, resp);
}, ["read_body"])

core.listen(server, "127.0.0.1:1122");
