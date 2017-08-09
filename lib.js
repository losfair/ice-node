const base = require("./base.js");
const express = require("./express_api.js");

Object.assign(module.exports, base);
module.exports.express = express.createApplication;
Object.assign(module.exports.express, express);
