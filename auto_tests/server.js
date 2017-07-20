const lib = require("../lib.js");
const path = require("path");

let app = new lib.Ice();

function sleep(ms) {
    return new Promise(cb => setTimeout(() => cb(), ms));
}

let template = `
<p>Template OK: {{ param }}</p>
`.trim();
app.add_template("test.html", template);

let my_path = path.join(__dirname, "server.js");

app.get("/get/sync", req => {
    return "OK";
});

app.get("/get/async_immediate", async req => {
    return "OK";
});

app.get("/get/async_delayed/:time", async req => {
    let t = parseInt(req.params.time);
    await sleep(t);
    return "OK";
});

app.post("/post/echo/raw", req => {
    return req.body();
});

app.post("/post/echo/json", req => {
    return lib.Response.json(req.json());
});

app.post("/post/echo/form_to_json", req => {
    return lib.Response.json(req.form());
});

app.use("/session", new lib.Flag("init_session"));
app.get("/session", req => {
    let count = req.session.count || "0";
    req.session.count = "" + (parseInt(count) + 1);
    return count.toString();
});

app.get("/exception/sync", req => {
    throw new Error("Sync exception");
});

app.get("/exception/async_immediate", async req => {
    throw new Error("Async exception (immediate)");
});

app.get("/exception/async_delayed/:time", async req => {
    let t = parseInt(req.params.time);
    await sleep(t);
    throw new Error("Async exception (delayed)");
});

app.get("/template/:param", req => {
    return new lib.Response({
        template_name: "test.html",
        template_params: {
            param: req.params.param
        }
    });
});

app.get("/code", req => lib.Response.file(my_path));

app.listen("127.0.0.1:5329");
