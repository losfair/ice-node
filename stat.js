const os = require("os");

let last_update_time = 0;

module.exports.update_system_stats = update_system_stats;
function update_system_stats(req) {
    if(Date.now() - last_update_time < 10000) {
        return;
    }
    last_update_time = Date.now();

    req.set_custom_stat("Uptime", get_uptime_str());
    req.set_custom_stat("Load average", os.loadavg().map(v => v.toFixed(2)).join(" "));
    req.set_custom_stat("Free memory", bytes_to_mb(os.freemem()));
    req.set_custom_stat("Total memory", bytes_to_mb(os.totalmem()));
}

function get_uptime_str() {
    let t = os.uptime(); // in seconds
    let d = Math.floor(t / 86400);
    let h = Math.floor((t % 86400) / 3600);
    let m = Math.floor((t % 3600) / 60);
    let s = t % 60;
    return `${d} d ${h} h ${m} m ${s} s`;
}

function bytes_to_mb(b) {
    return Math.floor(b / 1048576.0).toString() + " MB";
}
