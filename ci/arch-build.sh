#!/usr/bin/env bash
# Build, test and smoke-render kde-perfmon inside an Arch Linux container.
# Used by .github/workflows/ci.yml and .github/workflows/release.yml.
set -euo pipefail

pacman -Syu --noconfirm --needed
pacman -S --noconfirm --needed \
    base-devel cmake \
    qt6-base qt6-declarative qt6-svg \
    kirigami libplasma ttf-dejavu

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"

./build/perfmon-tests

/usr/lib/qt6/bin/qmllint -I package/contents/lib package/contents/ui/*.qml

export QT_QPA_PLATFORM=offscreen
export QT_QUICK_BACKEND=software
./build/perfmon-preview package/contents/ui/FullRepresentation.qml /tmp/perfmon-preview
test -s /tmp/perfmon-preview-1.png

# Sort / freeze / back-to-top regression run. It asserts on the visible state
# (first row on top, no frozen badge after a re-sort, frozen while scrolled) and
# exits non-zero on a mismatch, so a view left stuck on "Paused" fails the build.
PERFMON_PREVIEW_SORT=1 ./build/perfmon-preview package/contents/ui/FullRepresentation.qml /tmp/perfmon-preview-sort
test -s /tmp/perfmon-preview-sort-sort.png

echo "Build, tests and UI smoke render succeeded."
