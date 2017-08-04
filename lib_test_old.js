const lib = require("./lib.js");

let app = new lib.Ice({
    disable_request_logging: true,
    session_timeout_ms: 10000,
    endpoint_timeout_ms: 0
});

function sleep(ms) {
    return new Promise(cb => setTimeout(() => cb(), ms));
}

app.use("/static/", lib.static("."));

app.get("/", req => "Root");

app.get("/hello_world", req => {
    return "Hello world!";
});

app.use("/time", req => {
    console.log(req);
});

app.route(["GET", "POST"], "/time", req => {
    let body = req.json();
    let time;

    if(body && body.time) {
        time = body.time;
    } else {
        time = Date.now();
    }

    return lib.Response.json({
        formatted: new Date(time).toLocaleString()
    });
});

app.use("/session", new lib.Flag("init_session"));
app.get("/session", req => {
    let count = req.session.count || "0";
    req.session.count = "" + (parseInt(count) + 1);
    return count.toString();
});

app.post("/form_to_json", req => {
    return lib.Response.json(req.form());
});

app.get("/cookies", req => {
    return new lib.Response({
        cookies: {
            a: 1,
            b: 2
        }
    });
});

app.get("/delay/:time", async req => {
    await sleep(parseInt(req.params.time));
    return "OK";
});

app.get("/exception/sync", req => {
    throw new Error("Sync exception");
});

app.get("/exception/async/:delay", async req => {
    let delay = parseInt(req.params.delay);
    if(delay) await sleep(delay);
    throw new Error("Async exception");
});

app.get("/two_params/:a/:b", req => {
    return req.params.a + " " + req.params.b;
});

let test_tpl = app.add_template("test.html", `Hello world!
{{ date }}
`);

app.get("/template/render", req => {
    return new lib.Response({
        template_name: "test.html",
        template_params: {
            date: new Date().toLocaleString()
        }
    });
});

app.get("/stats", req => {
    console.log(req.get_stats());
    return lib.Response.json(req.get_stats(true));
});

app.get("/stream", req => {
    return lib.Response.stream(async stream => {
        stream.write("Hello world from a stream\n");
        await sleep(1000);
        stream.write("The second line\n");
    });
});

app.get("/redirect/stream", req => lib.Response.redirect("/stream"));

app.listen("127.0.0.1:1122");