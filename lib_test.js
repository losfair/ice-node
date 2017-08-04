const ice = require("./lib.js");

const app = new ice.Application({
    disable_request_logging: true
});

app.route("GET", "/hello_world", (req, resp) => {
    resp.body("Hello world!");
});

app.prepare();
app.listen("127.0.0.1:1479");
