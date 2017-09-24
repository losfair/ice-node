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

let client = new rpc.RpcClient("127.0.0.1:1653");
let maxSeq = 1000000;
let seq = 0;

setTimeout(() => {
    for(let i = 0; i < 32; i++) {
        client.connect(conn => {
            console.log("Connected");
            testCall(conn);
        });
    }
}, 3000);

function testCall(conn) {
    if(seq == maxSeq) {
        process.exit(0);
    }
    seq++;
    conn.call("add", [
        rpc.RpcParam.buildI32(0),
        rpc.RpcParam.buildI32(seq)
    ], ret => {
        let v = ret.getI32();
        console.log(v);
        ret.destroy();
        testCall(conn);
    });
}
