const fs = require("fs");
const express = require("./lib.js").express;

const app = express({
    disable_request_logging: true
});

app.loadCervusModule("test", fs.readFileSync("test_module.bc"));

app.addTemplate("test_template", `
<h1>It works!</h1>
<h3>{{ param }}</h3>
`);

app.get("/hello_world", (req, resp) => {
    resp.send("Hello world!");
});

app.use("/echo/", (req, resp, next) => {
    resp.header("Request-Id", req.custom.request_id);
    next();
});

app.get("/echo/:text", (req, resp) => {
    resp.send(req.params.text);
});

app.use("/form/", express.bodyParser.urlencoded());
app.post("/form/info", (req, resp) => {
    resp.json(req.body);
});

app.get("/render", (req, resp) => resp.render("test_template", {
    param: "Hello world!"
}));

app.listen(9812);
