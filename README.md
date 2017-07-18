Ice-node is a high-performance framework for writing web applications, based on [Ice Core](https://github.com/losfair/IceCore).

# Why Ice-node ?

### Fast

Ice-node is based on Ice Core, which is written in Rust and C++ and provides high-performance abstractions for the Web.

When serving the "Hello world!" text, Ice-node is about 10% faster than the raw Node.js http implementation, 80% than Koa, and 100% than Express, while providing full routing support.

##### Requests per second, higher is better

![Benchmark Result](http://i.imgur.com/TkV8IxE.png)

[Raw Results](https://gist.github.com/losfair/066b04978d6a5b27418d85a6305ecd5c)

### Easy to use

Ice-node makes use of ES6 and later features like async functions to provide a better coding experience for developers.

A simple server using Ice-node looks like this:

    const ice = require("ice-node");
    const app = ice.Ice();

    app.get("/", req => "Hello world!");

    app.listen("127.0.0.1:3535");

# Install

Ice-node requires node v7.6.0 or higher for ES2015 and async function support, and, of course, Ice Core for everything.

If you don't have Ice Core installed on your system yet, go to [Ice Core](https://github.com/losfair/IceCore) for instructions.

You can quickly install a supported version of node with your favorite version manager:

    $ nvm install 7
    $ npm install --save ice-node
    $ node my-ice-node-app.js

# Application


