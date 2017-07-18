const lib = require("./lib.js");

let app = new lib.Ice();

function sleep(ms) {
    return new Promise(cb => setTimeout(() => cb(), ms));
}

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

app.listen("127.0.0.1:1122");
