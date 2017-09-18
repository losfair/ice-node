const lib = require("./lib.js");
const router = lib.router;

let server = new lib.HttpServer(
    new lib.HttpServerConfig().set_num_executors(4).set_listen_addr("127.0.0.1:6851")
);

let rt = new router.Router();
//rt.use("/", (req) => console.log(req));
rt.route("POST", "/echo", (req) => {
    let result = [];

    req.intoBody((data) => {
        result.push(data);
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
rt.build(server);

server.start();
