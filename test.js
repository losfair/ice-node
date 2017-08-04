const core = require("./build/Release/ice_node_core");

let server = new core.Server({
    disable_request_logging: true
});
server.route("/hello_world", req => {
    req.createResponse().send();
});
server.route("/leak_request", req => {
});
server.route("/leak_response", req => {
    req.createResponse();
});
server.listen("127.0.0.1:9132");
