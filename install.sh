#!/usr/bin/env bash
# Build the QML plugin and install the plasmoid for the current user.
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
build_dir="${BUILD_DIR:-$root/build}"
dest="${XDG_DATA_HOME:-$HOME/.local/share}/plasma/plasmoids/org.hao.perfmon"

cmake -S "$root" -B "$build_dir" -DCMAKE_BUILD_TYPE=Release
cmake --build "$build_dir" -j"$(nproc)"

rm -rf "$dest"
mkdir -p "$dest"
cp -r "$root/package/." "$dest/"
install -m 755 "$build_dir/libperfmonplugin.so" "$dest/contents/lib/Hao/PerfMon/libperfmonplugin.so"

echo "Installed Performance Monitor to $dest"
echo "Try it with: plasmawindowed org.hao.perfmon"
