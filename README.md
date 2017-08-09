**This documentation is for v0.2.x. The latest documentation for v0.3.x is not ready yet.**

Ice-node is the **fastest** framework for building node web applications, based on [Ice Core](https://github.com/losfair/IceCore).

[![Build Status](https://travis-ci.org/losfair/ice-node.svg?branch=master)](https://travis-ci.org/losfair/ice-node)

[简体中文](https://github.com/losfair/ice-node/blob/master/README-zh_CN.md)

# Why Ice-node ?

### Fast

Ice-node is based on Ice Core, which is written in Rust and provides high-performance abstractions for the Web.

When serving the "Hello world!" text, Ice-node is about 70% faster than the raw Node.js http implementation, while providing full routing support.

##### Requests per second, higher is better

![Benchmark result](https://i.imgur.com/4uBIYMC.png)

For practical applications that do database queries and some logic, Ice-node is also at least 30% faster than traditional Node web frameworks like Express.

We wrote a [test application](https://github.com/losfair/ice-node-perf-tests) that simulates some common API services like login and data fetching and do MongoDB queries when processing requests,
and fire up 500 concurrent clients, each doing 101 requests:

##### Time for 500 * 101 requests, lower is better:

![Benchmark Result](http://i.imgur.com/OIEUPOr.png)

[Raw Results](https://gist.github.com/losfair/4d219e98b2e207ad4985b75304321292)

### Easy to use

Ice-node makes use of ES6 and later features like async functions to provide a better coding experience for developers.

A simple server using Ice-node looks like this:

    const ice = require("ice-node");
    const app = new ice.Application();

    app.get("/", (req, resp) => resp.body("Hello world!"));

    app.prepare();
    app.listen("127.0.0.1:3535");

# Install

Ice-node requires node v7.6.0 or higher for ES2015 and async function support, and, of course, Ice Core for everything.

**If you don't have Ice Core installed on your system yet, go to [Ice Core](https://github.com/losfair/IceCore) for instructions.**

You can quickly install a supported version of node with your favorite version manager:

    $ nvm install 7
    $ npm install --save ice-node
    $ node my-ice-node-app.js

# Application

With `const app = new ice.Application()`, you creates an Ice-node **Application** - an object describing how requests will be handled and how your data will be presented to users,
containing arrays of `routes`, `middlewares`, key-value mappings of `templates`, and a `config` object.

Every middleware and routing endpoint will receive a `Request` object once reached, providing access to details of the request, including URL, params, headers, cookies, remote address, method and body.

A simple app that responds with the `text` param:

    const ice = require("ice-node");
    const app = new ice.Ice();

    app.get("/:text", (req, resp) => resp.body(req.params.text));

    app.prepare();
    app.listen("127.0.0.1:3536");

### The Request object

The Request object, which is passed to middlewares and endpoints, contains the following fields:

- `headers` (object): Key-value mappings of request headers, with all keys in lower case.
- `uri` (string): The full URI of the request.
- `url` (string): Request URL.
- `remote_addr` (string): Remote address.
- `method` (string): Request method, in upper case (`GET`, `POST` etc.) .
- `cookies` (object): Key-value mappings of cookies in the `Cookie` header.
- `session` (object): Key-value read and write access to the session of the request.
- `params` (object): Key-value mappings of params in request URL.

For example, the following code:

    app.get("/ip", (req, resp) => resp.body(req.remote_addr.split(":")[0]));

will return the visitor's IP address.

### Middlewares and Endpoints

Similar to Ice Core, Ice-node is targeted at **endpoint**s, the last handler requests may reach.

Compared to some other node web frameworks targeting middlewares, our design makes it easier to optimize the performance and simplify user logics.

Due to the way Ice-node handle endpoints and middlewares, only requests hitting an endpoint will be processed by middlewares. For example,

    app.use("/", req => console.log(req.url));
    app.get("/hello_world", req => "Hello world!");

will:

1. GET `/hello_world` => Log the request URL and respond with "`Hello world!`"
2. POST `/hello_world` => Log the request URL and respond with 405 error
3. GET `/test` => No log, respond with 404 Not Found

because the core router can find a path to the endpoint `/hello_world` but not to `/test`.

### Return values and exceptions

All middlewares and endpoints may throw exceptions, terminating the current request.

For middlewares, a Response object as exception will be sent to the client, and all other exceptions lead to a 500 error.

For endpoints, any exceptions thrown lead to a 500 error.

Only endpoints' return values are processed. If it is a Response object, it will be sent to the client. Otherwise, Ice-node will try to construct a Response object from it.

### Async functions as endpoint handlers

Async functions can be used as endpoint handlers. For example:

    function sleep(ms) {
        return new Promise(cb => setTimeout(() => cb(), ms));
    }

    app.get("/sleep/:time", async req => {
        await sleep(parseInt(req.params.time));
        return "OK";
    });

However, middlewares are not `await`ed and **MUST NOT** bring the Request object into the next tick - it leads to undefined behavior.
Consider to store your state in sessions, to be accessed fast and synchronously.

### Flags

Flags, which are directly passed to the core, can be declared in the form of middlewares:

    app.use("/user/", new ice.Flag("init_session"));

Flags are used to enable or disable core features like session management and request body reading.

### Sessions

Ice Core provides built-in session support and is available from Ice-node.

Currently, sessions are stored in memory and are concurrently garbage-collected based on timeouts.

Sessions can be enabled by the flag `init_session`, and are automatically created, renewed and destroyed.

To set session timeout, pass `session_timeout_ms` parameter when creating the application:

    const app = new ice.Ice({ session_timeout_ms: 1800000 });

To access the session, use `req.session` object:

    app.use("/user/", new ice.Flag("init_session"));

    app.get("/user/logout", req => {
        if(req.session.logged_in == "true") {
            req.session.logged_in = "false";
            return "OK";
        } else {
            return "Not logged in";
        }
    });

To remove a session item, set it to `null`.

Note that all session keys and values must be strings.

# Server

By `app.listen("127.0.0.1:3535")`, Ice-node builds up handlers, initializes the server instance and fires it up.

# Bugs & improvements

Both Ice Core and Ice-node are still in early development, and contributions are welcome.

Use [Issues](https://github.com/losfair/ice-node/issues) and pull requests to let us know your ideas!
