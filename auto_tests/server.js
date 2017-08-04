const lib = require("../lib.js");
const path = require("path");

let app = new lib.Application({
    max_request_body_size: 100000
});

function sleep(ms) {
    return new Promise(cb => setTimeout(() => cb(), ms));
}

let template = `
<p>Template OK: {{ param }}</p>
`.trim();
app.addTemplate("test.html", template);

let my_path = path.join(__dirname, "server.js");

app.use("/files/", lib.static(__dirname));

app.get("/get/sync", (req, resp) => {
    resp.body("OK");
});

app.get("/get/async_immediate", async (req, resp) => {
    resp.body("OK");
});

app.get("/get/async_delayed/:time", async (req, resp) => {
    resp.body("Not implemented");
    /*
    let t = parseInt(req.params.time);
    await sleep(t);
    resp.body("OK");
    */
});

app.post("/post/echo/raw", (req, resp) => {
    resp.body(req.body());
});

app.post("/post/echo/json", (req, resp) => {
    resp.json(req.json());
});

app.post("/post/echo/form_to_json", (req, resp) => {
    resp.json(req.form());
});

app.use("/session", new lib.Flag("init_session"));
app.get("/session", (req, resp) => {
    let count = req.session.count || "0";
    req.session.count = "" + (parseInt(count) + 1);
    resp.body(count.toString());
});

app.get("/exception/sync", (req, resp) => {
    throw new Error("Sync exception");
});

app.get("/exception/async_immediate", async (req, resp) => {
    throw new Error("Async exception (immediate)");
});

app.get("/exception/async_delayed/:time", async (req, resp) => {
    resp.body("Not implemented");
    /*
    let t = parseInt(req.params.time);
    await sleep(t);
    throw new Error("Async exception (delayed)");
    */
});

app.get("/template/:param", (req, resp) => {
    resp.renderTemplate("test.html", {
        //param: req.params.param
        param: req.url.split("/").pop()
    });
});

app.get("/code", (req, resp) => resp.file(my_path));

app.prepare();
app.listen("127.0.0.1:5329");
