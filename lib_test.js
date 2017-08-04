const ice = require("./lib.js");

const app = new ice.Application({
    disable_request_logging: true
});

app.route("GET", "/hello_world", (req, resp) => {
    resp.body("Hello world!");
});

app.route("GET", "/hello_world_detached", (req, resp) => {
    resp.detach();
    setImmediate(() => resp.body("Hello world! (Detached)").send());
});

app.route("GET", "/leak_request", (req, resp) => resp.detach());

app.prepare();
app.listen("127.0.0.1:1479");
