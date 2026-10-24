<div align="center">
  <h1 align="center">kde-perfmon</h1>
  <p align="center">A KDE Plasma 6 panel performance monitor with CPU / memory preview, process list and system header</p>
  <p align="center">English | <a href="README.md">中文</a></p>

  <a href="https://github.com/haoyn231/kde-perfmon/releases/latest"><img src="https://img.shields.io/github/v/release/haoyn231/kde-perfmon?display_name=tag&label=Version&color=blue" alt="Version"></a>
  <a href="./LICENSE"><img src="https://img.shields.io/badge/License-MIT-green" alt="MIT License"></a>
  <img src="https://img.shields.io/badge/Plasma-6-1D99F3" alt="KDE Plasma 6">
  <img src="https://img.shields.io/badge/Qt-6.5%2B-41CD52" alt="Qt 6.5+">
  <img src="https://img.shields.io/badge/C%2B%2B-17-00599C" alt="C++17">
  <img src="https://img.shields.io/badge/Linux-x86__64-informational" alt="Linux x86_64">
  <br>
  <a href="https://github.com/haoyn231/kde-perfmon/actions/workflows/ci.yml"><img src="https://github.com/haoyn231/kde-perfmon/actions/workflows/ci.yml/badge.svg" alt="CI"></a>
  <a href="https://github.com/haoyn231/kde-perfmon/stargazers"><img src="https://img.shields.io/github/stars/haoyn231/kde-perfmon.svg" alt="GitHub stars"></a>
</div>

---

<p align="center">
  <img width="70%" src="./docs/images/screenshot.png" alt="kde-perfmon panel preview and process list">
</p>

<p align="center"><sub>Panel CPU / memory preview and the process list sorted by memory</sub></p>

A KDE Plasma 6 panel widget (Plasmoid) that previews CPU and memory usage in the panel, and opens a sortable per-process list. The popup header shows the distro logo, distro name, hostname, user avatar and username. All colors follow the system light/dark color scheme.

Inspired by [DankMaterialShell (DMS)](https://github.com/AvengeMedia/DankMaterialShell) (`MonitorPill`, `ProcessSummary`, `ProcessesView`, `ProcessSystemHeader`) and by the sampling semantics of [dgop](https://github.com/AvengeMedia/dgop).

## Features

- Compact panel preview: two `icon + percentage` pairs (CPU, memory)
- Click the CPU number to open the process list sorted by CPU; click memory to sort by memory
- Popup header: distro logo, distro name, hostname, user avatar and username
- `CPU` / `Memory` sort chips; click the active chip again to flip ascending/descending; the arrow button does the same
- Process rows: name, full command line (elided), CPU%, RSS, PID — no process icons
- Click a row to expand the full command, PPID and memory percentage, then send SIGTERM or SIGKILL (force kill only for PIDs above 1000, with an inline confirmation)
- Scroll position is preserved across the 1-second list refresh; the PID column is not covered by the overlay scrollbar
- When the popup is closed only total CPU/memory are sampled; `/proc/<pid>` scanning is disabled
- Everything is bound to `Kirigami.Theme`, so light/dark switches apply live

## Install (KDE Plasma 6)

### Option 1: install from a Release (recommended)

The release archive contains a **prebuilt** plugin; no compiler is needed.

1. Requirements: KDE Plasma 6 (Qt 6.5+), Kirigami (shipped with Plasma). The release is built on Arch Linux / Plasma 6.8 / Qt 6.12. If the plugin fails to load on another distribution, use Option 2 and build from source.
2. Download `kde-perfmon-vX.Y.Z.tar.gz` and the matching `.sha256` from [Releases](https://github.com/haoyn231/kde-perfmon/releases), then verify:
   ```bash
   sha256sum -c kde-perfmon-v1.0.2.tar.gz.sha256
   ```
3. Extract it into your user Plasma widget directory:
   ```bash
   mkdir -p ~/.local/share/plasma/plasmoids
   tar -xzf kde-perfmon-v1.0.2.tar.gz -C ~/.local/share/plasma/plasmoids
   ```
   You should then have:
   ```
   ~/.local/share/plasma/plasmoids/org.hao.perfmon/metadata.json
   ~/.local/share/plasma/plasmoids/org.hao.perfmon/contents/lib/Hao/PerfMon/libperfmonplugin.so
   ```
4. Reload Plasma (recommended after upgrading; a first install is usually picked up automatically):
   ```bash
   systemctl --user restart plasma-plasmashell.service
   ```
   Logging out and back in also works.
5. Add the widget: right-click the panel → **Enter Edit Mode** (or **Add Widgets**) → search for **Performance Monitor** / **性能监视器** → drag it onto the panel.
6. Click the CPU or memory percentage in the panel to open the process list.

Uninstall:
```bash
rm -rf ~/.local/share/plasma/plasmoids/org.hao.perfmon
systemctl --user restart plasma-plasmashell.service
```

### Option 2: build from source

Dependencies (Arch Linux):

```bash
sudo pacman -S --needed base-devel cmake qt6-base qt6-declarative qt6-svg
```

Build and install for the current user:

```bash
git clone https://github.com/haoyn231/kde-perfmon.git
cd kde-perfmon
./install.sh
systemctl --user restart plasma-plasmashell.service
```

Then add the widget to the panel as described in Option 1, step 5.

## Usage

- The two numbers in the panel are CPU and memory usage.
- Clicking the CPU number presets sorting by CPU before opening the popup; clicking memory presets sorting by memory.
- While the popup is open, clicking the CPU or memory preview again closes it.
- Inside the popup:
  - click `CPU` / `Memory` to change the sort column;
  - click the active column again to flip ascending/descending;
  - the arrow button flips the current sort direction;
  - use the mouse wheel to scroll the process list.
- Click a process row to expand or collapse it:
  - the expanded view shows the full command line, `PPID` and memory percentage;
  - `Terminate` sends SIGTERM, `Force kill` sends SIGKILL (PIDs above 1000 only);
  - clicking an action requires an inline confirmation;
  - failed signals are reported inline with the system error text.

## Data semantics

One sample per second, read entirely from `/proc`, with no background daemon:

| Metric | Calculation |
| --- | --- |
| Total CPU | delta of `/proc/stat`, `(total - idle - iowait) / total` |
| Process CPU | `Δ(utime+stime) / Δt / HZ / CPU count * 100`, capped at 100% (100% = whole machine, same as dgop) |
| Process memory | RSS (`/proc/<pid>/stat` rss field), sorted by RSS bytes |
| System memory used | `total - free - buffers - (Cached + SReclaimable - Shmem)`, same as gopsutil / DMS |
| Swap | `SwapTotal - SwapFree` |

Sorting matches DMS: changing columns starts descending; clicking the active column flips direction; ties are broken by ascending PID.

## System header

- Distro name / ID / LOGO come from `/etc/os-release`
- Distro logo is looked up at `/usr/share/pixmaps/<id>-logo.svg|png` first (Arch ships `archlinux-logo.svg`), then the `distributor-logo-<id>` theme icon, then `computer-symbolic`
- Avatar lookup order: `/var/lib/AccountsService/icons/$USER`, `~/.face`, `~/.face.icon`, `/usr/share/sddm/faces/$USER.face.icon`; falls back to `user-identity-symbolic`
- Username comes from `getpwuid`; hostname from `QSysInfo::machineHostName()`
- The avatar is clipped to a circle and everything follows the active theme

## Theming

No foreground or background color is hardcoded:

- Text: `Kirigami.Theme.textColor` / `disabledTextColor`
- Card and chip backgrounds: translucent overlays derived from `textColor`
- Accent: `Kirigami.Theme.highlightColor` (follows the KDE accent color)
- High-load thresholds: `neutralTextColor` / `negativeTextColor`

Switching from `OrchisDark` to `BreezeLight` while running updates the UI immediately.

## Development and testing

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"

# unit tests + live /proc sampling assertions
./build/perfmon-tests

# QML lint (on Arch the Qt 6 binary is /usr/lib/qt6/bin/qmllint)
/usr/lib/qt6/bin/qmllint -I package/contents/lib package/contents/ui/*.qml

# render the real popup QML to PNG without a panel (prints texts and theme colors)
QT_QPA_PLATFORMTHEME=KDE QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software \
    ./build/perfmon-preview package/contents/ui/FullRepresentation.qml /tmp/preview
```

CI installs Qt 6 / Kirigami / libplasma in an Arch Linux container and runs the build, unit tests, `qmllint` and an offscreen render. Pushing a tag matching `v*.*.*` (for example `v1.0.0`) triggers the Release workflow, which builds the plugin, attaches an installable archive and a SHA-256 checksum to the GitHub Release.

Repository layout:

```
src/
  procdata.{h,cpp}       /proc parsing, CPU percentage, sorting (unit-testable)
  processmodel.{h,cpp}   process list QAbstractListModel
  perfbackend.{h,cpp}    1s sampler, sort state, system metadata, QML API
  perfmonplugin.cpp      QML extension plugin (exposes the PerfBackend singleton)
package/
  metadata.json          Plasma widget metadata
  contents/ui/           main / compact / popup / process row / sort chip / cards
  contents/lib/Hao/PerfMon/  qmldir + compiled plugin .so
tests/tests.cpp          QtTest unit tests + live sampling assertions
tools/preview.cpp        offscreen rendering helper
ci/arch-build.sh        shared Arch-container build script for CI and releases
.github/workflows/      ci.yml + release.yml
docs/releases/          per-release notes and the generic template
```

## License

MIT, see [LICENSE](LICENSE).
