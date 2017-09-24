const lib = require("./lib.js");
const rpc = lib.rpc;
const assert = require("assert");

let cfg = new rpc.RpcServerConfig();
cfg.addMethod("ping", (ctx) => {
    ctx.end(
        rpc.RpcParam.buildString("Pong")
    );
});
cfg.addMethod("add", (ctx) => {
    ctx.end(
        rpc.RpcParam.buildI32(ctx.getParam(0).getI32() + ctx.getParam(1).getI32())
    );
});
cfg.addMethod("add_string", (ctx) => {
    ctx.end(
        rpc.RpcParam.buildString(
            ctx.getParam(0).getString() + ctx.getParam(1).getString()
        )
    );
});

let server = new rpc.RpcServer(cfg);
server.start("127.0.0.1:1653");

let client = new rpc.RpcClient("127.0.0.1:1653");
client.connect(async conn => {
    try {
        console.log("Connected");
        await testPing(conn);
        await testAdd(conn);
        await testAddString(conn);
        console.log("Done");
    } catch(e) {
        console.log(e);
    }
    process.exit(0);
});

function testPing(conn) {
    return new Promise(cb => {
        conn.call("ping", [], ret => {
            assert(ret.getString() == "Pong");
            ret.destroy();
            console.log("[+] testPing OK");
            cb();
        });
    });
}

function testAdd(conn) {
    return new Promise(cb => {
        conn.call("add", [
            rpc.RpcParam.buildI32(1),
            rpc.RpcParam.buildI32(2)
        ], ret => {
            let v = ret.getI32();
            assert(v === 3);
            ret.destroy();
            console.log("[+] testAdd OK");
            cb();
        });
    });
}

function testAddString(conn) {
    return new Promise(cb => {
        conn.call("add_string", [
            rpc.RpcParam.buildString("Hello "),
            rpc.RpcParam.buildString("world")
        ], ret => {
            let v = ret.getString();
            assert(v === "Hello world");
            ret.destroy();
            console.log("[+] testAddString OK");
            cb();
        });
    });
}
