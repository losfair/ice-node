const app = require("./lib.js").Application({
    disable_request_logging: true
});
const express_api = require("./express_api.js");

express_api.patchApplication(app);

app.get("/hello_world", (req, resp) => {
    resp.send("Hello world!");
});

app.use("/echo/", (req, resp, next) => {
    resp.header("Some-Header", "Some-Data");
    next();
});

app.get("/echo/:text", (req, resp) => {
    resp.send(req.params.text);
});

app.listen(9812);
