const lib = require("./lib.js");

let app = new lib.Ice({
    session_timeout_ms: 10000
});

function sleep(ms) {
    return new Promise(cb => setTimeout(() => cb(), ms));
}

app.use("/static/", lib.static("."));

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
    return lib.Response.json(req.get_stats());
});

app.listen("127.0.0.1:1122");
