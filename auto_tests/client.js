const rp = require("request-promise");
const assert = require("assert");

const REMOTE = "http://127.0.0.1:5329";

let template_param = "Hello";
let expected_rendered_template = "<p>Template OK: Hello</p>";

async function run() {
    console.log("Testing GET (Sync)");
    assert((await rp.get(REMOTE + "/get/sync")) == "OK");

    console.log("Testing GET (Async, immediate)");
    assert((await rp.get(REMOTE + "/get/async_immediate")) == "OK");

    console.log("Testing GET (Async, delayed 100ms)");
    let t1 = Date.now();
    assert((await rp.get(REMOTE + "/get/async_delayed/100")) == "OK");
    let t2 = Date.now();

    if(Math.abs((t2 - t1) - 100) > 20) {
        throw new Error("Incorrect delay time");
    }

    console.log("Testing POST (echo, raw)");
    assert((await rp.post(REMOTE + "/post/echo/raw", {
        body: "Hello world!"
    })) == "Hello world!");

    console.log("Testing POST (echo, json)");
    let json_data = {
        some_key_1: "some_value_1",
        some_key_2: {
            a: 1,
            b: 1.5
        }
    };
    let json_data_str = JSON.stringify(json_data);

    assert(JSON.stringify(await rp.post(REMOTE + "/post/echo/json", {
        json: json_data
    })) == json_data_str);

    console.log("Testing POST (echo, form_to_json)");
    let form_data = {
        some_key_1: "aaa",
        some_key_2: "bbb"
    };
    let form_data_as_json = JSON.stringify(form_data);

    assert(JSON.stringify(JSON.parse(await rp.post(REMOTE + "/post/echo/form_to_json", {
        form: form_data
    }))) == form_data_as_json);

    console.log("Testing session");
    let r = await rp.get(REMOTE + "/session", {
        resolveWithFullResponse: true
    });
    assert(r.body == "0");
    assert((await rp.get(REMOTE + "/session", {
        headers: {
            Cookie: r.headers["set-cookie"]
        }
    })) == "1");

    console.log("Testing exception handling (Sync)");
    let ok = false;
    try {
        console.log(await rp.get(REMOTE + "/exception/sync"));
    } catch(e) {
        assert(e.response.body == "Internal error" && e.statusCode == 500);
        ok = true;
    }
    if(!ok) throw new Error("Exception not handled properly");

    console.log("Testing exception handling (Async, immediate)");
    ok = false;
    try {
        console.log(await rp.get(REMOTE + "/exception/async_immediate"));
    } catch(e) {
        assert(e.response.body == "Internal error" && e.statusCode == 500);
        ok = true;
    }
    if(!ok) throw new Error("Exception not handled properly");

    console.log("Testing exception handling (Async, delayed 100ms)");
    ok = false;
    t1 = Date.now();

    try {
        console.log(await rp.get(REMOTE + "/exception/async_delayed/100"));
    } catch(e) {
        t2 = Date.now();
        assert(e.response.body == "Internal error" && e.statusCode == 500);
        if(Math.abs((t2 - t1) - 100) > 20) {
            throw new Error("Incorrect delay time");
        }
        ok = true;
    }
    if(!ok) throw new Error("Exception not handled properly");

    console.log("Testing template rendering")
    assert((await rp.get(REMOTE + "/template/" + template_param)) == expected_rendered_template);

    console.log("Everything OK");
    process.exit(0);
}

run().then(_ => {}).catch(e => {
    console.log(e);
    process.exit(1);
});

setTimeout(() => {
    console.log("Timeout");
    process.exit(1);
}, 120000);
