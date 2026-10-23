.pragma library

function formatBytes(bytes) {
    const value = Number(bytes) || 0;
    if (value <= 0) {
        return "0 B";
    }
    const units = ["B", "KiB", "MiB", "GiB", "TiB"];
    let scaled = value;
    let unit = 0;
    while (scaled >= 1024 && unit < units.length - 1) {
        scaled /= 1024;
        unit += 1;
    }
    const digits = unit === 0 ? 0 : (scaled >= 100 ? 0 : 1);
    return scaled.toFixed(digits) + " " + units[unit];
}

function percent(value) {
    return (Number(value) || 0).toFixed(1) + "%";
}
