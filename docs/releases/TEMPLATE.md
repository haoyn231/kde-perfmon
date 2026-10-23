# kde-perfmon @VERSION@

> 通用 Release 模板。若不存在 `docs/releases/v@VERSION@.md`，发布工作流会使用本文件并把 `@VERSION@` 替换为版本号。

## 更新内容 / What's Changed

- 请在此补充本版本的变更 / Add the changes for this release here
- See the commit history for the full list

## 安装教程（KDE Plasma 6）

### 环境要求

- KDE Plasma 6（Qt 6.5+）
- Kirigami（Plasma 自带）

### 从 Release 安装（推荐）

1. 下载本页附件中的 `kde-perfmon-v@VERSION@.tar.gz` 与 `.sha256` 文件。
2. 校验并解压到当前用户的 Plasma 小程序目录：
   ```bash
   sha256sum -c kde-perfmon-v@VERSION@.tar.gz.sha256
   mkdir -p ~/.local/share/plasma/plasmoids
   tar -xzf kde-perfmon-v@VERSION@.tar.gz -C ~/.local/share/plasma/plasmoids
   ```
3. 重新加载 Plasma：
   ```bash
   systemctl --user restart plasma-plasmashell.service
   ```
4. 右键面板 → **进入编辑模式**（或「添加小组件」）→ 搜索 **性能监视器** / **Performance Monitor** → 拖到面板。
5. 点击面板里的 CPU 或内存百分比打开进程列表。

### 从源码构建

```bash
sudo pacman -S --needed base-devel cmake qt6-base qt6-declarative qt6-svg
git clone https://github.com/haoyn231/kde-perfmon.git
cd kde-perfmon
./install.sh
systemctl --user restart plasma-plasmashell.service
```

### 卸载

```bash
rm -rf ~/.local/share/plasma/plasmoids/org.hao.perfmon
systemctl --user restart plasma-plasmashell.service
```

## Installation (KDE Plasma 6)

### Requirements

- KDE Plasma 6 (Qt 6.5+)
- Kirigami (shipped with Plasma)

### Install from the release (recommended)

1. Download `kde-perfmon-v@VERSION@.tar.gz` and `kde-perfmon-v@VERSION@.tar.gz.sha256` from the assets below.
2. Verify and extract the archive into your user Plasma widget directory:
   ```bash
   sha256sum -c kde-perfmon-v@VERSION@.tar.gz.sha256
   mkdir -p ~/.local/share/plasma/plasmoids
   tar -xzf kde-perfmon-v@VERSION@.tar.gz -C ~/.local/share/plasma/plasmoids
   ```
3. Reload Plasma:
   ```bash
   systemctl --user restart plasma-plasmashell.service
   ```
4. Right-click the panel → **Enter Edit Mode** (or **Add Widgets**) → search for **Performance Monitor** → drag it onto the panel.
5. Click the CPU or memory percentage in the panel to open the process list.

### Build from source

```bash
sudo pacman -S --needed base-devel cmake qt6-base qt6-declarative qt6-svg
git clone https://github.com/haoyn231/kde-perfmon.git
cd kde-perfmon
./install.sh
systemctl --user restart plasma-plasmashell.service
```

### Uninstall

```bash
rm -rf ~/.local/share/plasma/plasmoids/org.hao.perfmon
systemctl --user restart plasma-plasmashell.service
```
