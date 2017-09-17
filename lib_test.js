const lib = require("./lib.js");

let server = new lib.HttpServer(
    new lib.HttpServerConfig().set_num_executors(4).set_listen_addr("127.0.0.1:6851")
);

server.route("/", (req) => {
    let totalLength = 0;
    let result = "";
    req.intoBody((data) => {
        totalLength += data.length
        result += data.toString();
    }, (ok) => {
        console.log(totalLength);
        console.log(result);
        req.createResponse().send();
    });
});

server.start();
