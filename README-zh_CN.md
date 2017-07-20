Ice-node 是目前**最快的** Node Web 框架，基于 [Ice Core](https://github.com/losfair/IceCore) 核心。

[![Build Status](https://travis-ci.org/losfair/ice-node.svg?branch=master)](https://travis-ci.org/losfair/ice-node)

# 为什么选择 Ice-node ?

### 高效

Ice-node 基于 Ice Core ，其使用 Rust 和 C++ 编写，提供对 Web 服务的高性能抽象。

对于一个 Hello world 服务， Ice-node 比 Node HTTP 库快 10% ，比 Koa 快 80% ，比 Express 快 100% 。

##### 每秒请求数，越高越好

![Benchmark Result](http://i.imgur.com/TkV8IxE.png)

[原始数据](https://gist.github.com/losfair/066b04978d6a5b27418d85a6305ecd5c)

对于执行数据库请求和简单逻辑的 Web 应用， Ice-node 比 Express 至少快 30% 。
这是一个[测试应用](https://github.com/losfair/ice-node-perf-tests)，
模拟了登录、获取数据等一些常见的 API 服务，并在处理请求时查询 MongoDB 数据库。
测试程序模拟了 500 个客户端同时发起 101 个有状态的连续请求：

##### 处理 500 * 101 个请求的时间，越低越好 (毫秒)

![Benchmark Result](http://i.imgur.com/OIEUPOr.png)

[原始数据](https://gist.github.com/losfair/4d219e98b2e207ad4985b75304321292)

### 易用

Ice-node 使用了 ES6 和更新标准包含的新特性 (如 `async` 函数) ，提升编写 Web 应用的效率。

一个简单的 Ice-node 应用看起来像这样:

    const ice = require("ice-node");
    const app = new ice.Ice();

    app.get("/", req => "Hello world!");

    app.listen("127.0.0.1:3535");

# 安装

Ice-node 需要 Node v7.6.0 或更高版本，和 Ice Core 核心库。

**如果你的系统上没有安装 Ice Core 核心库，请到 [Ice Core](https://github.com/losfair/IceCore) 根据说明安装。**

你可以用你喜欢的 Node 版本管理工具快速安装受支持的 Node:

    $ nvm install 7
    $ npm install --save ice-node
    $ node my-ice-node-app.js

# 构建一个 Ice-node 应用

通过 `const app = new ice.Ice()`, 你创建了一个 **Ice-node 应用** `app` —— 一个描述请求将怎样被处理，和你的数据将怎样被展示给用户的的对象。
这个对象包含 `routes`、`middlewares` 数组，`templates` Key-Value 映射，和一个 `config`对象。

每个中间件和路由端点将接收到一个 `Request` 对象作为参数，提供对请求信息的访问，包括 URL、参数、HTTP 头、Cookies、远程地址、请求方式和 body 。

这是一个简单的返回 `text` 参数的应用:

    const ice = require("ice-node");
    const app = new ice.Ice();

    app.get("/:text", req => req.params.text);

    app.listen("127.0.0.1:3536");

### Request 对象

被传递给中间件和路由端点的 Request 对象包含这些键：

- `headers` (object): 请求头部的 Key-Value 映射，所有 Key 为小写。
- `uri` (string): 请求的完整 URI
- `url` (string): 请求的完整 URL
- `remote_addr` (string): 远程地址，包括 IP 和端口
- `method` (string): 请求方式，大写（如 `GET`、`POST` 等）
- `host` (string): 即 `headers.host`
- `cookies` (proxied object): 请求携带的 Cookies （只读）
- `session` (proxied object): 请求对应的 Session （读写）
- `params` (proxied object): 请求 URL 中的参数 （只读）

例如，这段代码：

    app.get("/ip", req => req.remote_addr.split(":")[0]);

将返回访问者的 IP 地址。

所有代理对象执行 Lazy Load， 不被访问时的开销为零。

### 中间件和路由端点

与 Ice Core 的设计相似， Ice-node 的路由机制设计是面向**端点**的，即请求可到达的最后一个处理函数。

与其他一些面向中间件的 Node Web 框架相比， Ice-node 的这个设计使性能优化更容易，且可能简化用户逻辑的编写。

由于 Ice-node 处理中间件和端点的方式，只有命中端点的请求会触发分发，并被对应的中间件处理。例如：

    app.use("/", req => console.log(req.url));
    app.get("/hello_world", req => "Hello world!");

这段代码会执行以下行为：

1. GET `/hello_world` => 输出请求的 URL，响应 "`Hello world`"
2. POST `/hello_world` => 输出请求的 URL，响应 405 错误
3. GET `/text` => 不输出，响应 404 错误

因为核心路由组件能找到端点为 `/hello_world` 的路由，但不能找到端点为 `/test` 的路由。

### 返回值和异常

所有中间件和端点处理器都可以抛出异常，结束当前请求的处理。

对于中间件，一个 `Response` 对象作为异常会被作为响应发送给客户端，而所有其他异常会导致 500 错误。

对于端点，所有异常都会导致 500 错误。

所有导致 500 错误的异常会被输出到 stdout 。

### `async` 函数作为端点处理器

`async` 函数可以被用作端点处理器。例如:

    function sleep(ms) {
        return new Promise(cb => setTimeout(() => cb(), ms));
    }

    app.get("/sleep/:time", async req => {
        await sleep(parseInt(req.params.time));
        return "OK";
    });

但是，中间件不会被 `await` ，并且不允许将 `Request` 对象的引用复制或带出当前 tick ，否则会导致未定义的行为。

### Flags

`Flag` 是用来为路由端点启用或禁用 Ice Core 功能的参数，可以用中间件的形式定义，例如：

    app.use("/user/", new ice.Flag("init_session"));

### 会话 (Sessions)

Ice Core 提供内置的会话管理支持，并且可以通过 Ice-node 启用和管理。

目前，会话被存储在内存中，并基于设定的超时时间执行垃圾回收。

会话支持可以通过 Flag `init_session` 启用。

创建应用时传递 `session_timeout_ms` 参数可以设置自定义的会话超时时间:

    const app = new ice.Ice({ session_timeout_ms: 1800000 });

对于启用了会话支持的端点和其路径上的中间件，可以通过 `req.session` 对象访问会话:

    app.use("/user/", new ice.Flag("init_session"));

    app.get("/user/logout", req => {
        if(req.session.logged_in == "true") {
            req.session.logged_in = "false";
            return "OK";
        } else {
            return "Not logged in";
        }
    });

要移除一个 Session 键，将其值设置为 `null` 即可。

注意：所有有效的 Session 键和值都必须是 `string` 类型。

# Server

通过 `app.listen("127.0.0.1:3535")`， Ice-node 将根据 `app` 所描述的应用行为构造处理器，初始化并启动 Server 实例。

# Bugs & improvements

Ice Core 和 Ice-node 都处于早期开发阶段，
如果你有任何问题 / feature request ，请使用 [Issues](https://github.com/losfair/ice-node/issues) 和 pull requests 来参与修复和改进。
