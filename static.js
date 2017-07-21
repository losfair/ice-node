const path = require("path");
const lib = require("./lib.js");

module.exports = function (p) {
    p = path.resolve(p);
    return function (req, mw) {
        let url_path = req.url.substr(mw.prefix.length);
        while(url_path.startsWith("/")) {
            url_path = url_path.substr(1);
        }

        let target = path.normalize(path.join(p, url_path));
        if(!target.startsWith(p + "/")) {
            throw new lib.Response({
                status: 403,
                body: "Illegal request"
            });
        }

        throw lib.Response.file(target);
    }
}