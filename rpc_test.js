const lib = require("./lib.js");
const rpc = lib.rpc;

let cfg = new rpc.RpcServerConfig();
cfg.addMethod("add", (ctx) => {
    ctx.end(
        rpc.RpcParam.buildI32(ctx.getParam(0).getI32() + ctx.getParam(1).getI32())
    );
});

let server = new rpc.RpcServer(cfg);
server.start("127.0.0.1:1653");
