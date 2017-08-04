const core = require("./build/Release/ice_node_core");

let server = new core.Server({
    disable_request_logging: true
});

server.route("", req => {
    let resp = req.createResponse();
    resp.status(404);
    resp.body(Buffer.from("Not found"));
    resp.send();
});

server.route("/hello_world", req => {
    let resp = req.createResponse();
    resp.body(Buffer.from("Hello world!"));
    resp.send();
});
server.route("/leak_request", req => {
});
server.route("/leak_response", req => {
    req.createResponse();
});
server.route("/stream", req => {
    let resp = req.createResponse();
    let stream = resp.stream();
    resp.send();
    stream.write(Buffer.from("Hello world! (Stream)"));
    stream.close();
})
server.listen("127.0.0.1:9132");
