const rp = require("request-promise");
const fs = require("fs");

async function run() {
    console.log("Downloading");
    let data = await rp.get("https://github.com/losfair/IceCore/releases/download/v0.1.1/libice_core.so", {
        encoding: null
    });
    console.log("Done");
    
    fs.writeFileSync("./libice_core.so", data);
    process.env["LD_LIBRARY_PATH"] = ".";

    require("./server.js");
    require("./client.js");
}

run();
