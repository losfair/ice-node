const fs = require("fs");
const ice = require("./lib.js");

const app = new ice.Application({
    disable_request_logging: true
});

let template = `
<p>Template OK: {{ param }}</p>
`.trim();

app.loadCervusModule("test", fs.readFileSync("test_module.bc"));

app.addTemplate("test.html", template);

app.route("GET", "/hello_world", (req, resp) => {
    resp.body("Hello world!");
});
app.route("GET", "/echo_param/:p", (req, resp) => {
    resp.body(req.params.p);
});
app.route("GET", "/request_id", (req, resp) => resp.body(req.custom.request_id));

app.route("GET", "/hello_world_detached", (req, resp) => {
    resp.detach();
    setImmediate(() => resp.body("Hello world! (Detached)").send());
});

app.route("GET", "/hello_world_stream", (req, resp) => {
    resp.stream(stream => {
        stream.write(Buffer.from("Hello world! (stream)"));
        stream.close();
    });
});

app.route("GET", "/leak_request", (req, resp) => resp.detach());

app.route("GET", "/render_template", (req, resp) => resp.renderTemplate("test.html", {
    param: new Date().toLocaleString()
}));

app.prepare();
app.listen("127.0.0.1:1479");
