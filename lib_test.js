const lib = require("./lib.js");
const router = lib.router;

let server = new lib.HttpServer(
    new lib.HttpServerConfig().setNumExecutors(4).setListenAddr("127.0.0.1:6851")
);

let rt = new router.Router();

rt.use("/info/", (req) => {
    req.mwHit = true;
});

rt.route("GET", "/info/uri", (req) => {
    if(!req.mwHit) {
        throw new Error("Middleware not called");
    }

    return req.getUri();
});

rt.route("POST", "/echo", (req) => {
    let result = [];

    req.intoBody((data) => {
        result.push(data);
        return true;
    }, (ok) => {
        req.createResponse().setBody(Buffer.concat(result)).send();
    });

    return new router.Detached();
});
rt.route("GET", "/hello_world", (req) => {
    return req.createResponse().setBody("Hello world!\n");
});
rt.route("GET", "/some_file", (req) => {
    return req.createResponse().sendFile("lib_test.js");
});
rt.route("GET", "/delay", (req) => new Promise(cb => setTimeout(() => cb(
    req.createResponse().setBody("OK")
), 1000)));
rt.build(server);

server.start();
