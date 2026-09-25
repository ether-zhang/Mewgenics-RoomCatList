# 房间猫咪列表 / RoomCatList

适用于 Mewgenics Windows x64，Steam Build 25143593。支持 English / 简体中文，首次使用默认英文。

## 功能

- 在家园侧栏打开猫咪列表，查看当前房间或全屋猫咪。
- 查看年龄、真实/遗传属性、职业、父母、突变和疾病；年龄与属性可排序，悬停可看效果说明。
- 从列表调房、装箱，或将猫咪送至可接收的 NPC。
- 新生猫筛选和老猫数量控制：可调整规则，先复查方案，确认后才执行。
- 支持鼠标与手柄，记住窗口位置和语言选择。

## 安装

1. 安装 [Mewjector](https://github.com/githubuser508/mewjector)（API v3），并退出游戏。
2. 从 [Releases](https://github.com/ether-zhang/Mewgenics-RoomCatList/releases) 下载 ZIP，解压到 `Mewgenics/mods/`，确保路径为 `Mewgenics/mods/RoomCatList/RoomCatList.dll`。
3. 在游戏根目录 `chainloader.ini` 的 `[LoadOrder]` 下添加 `ModN=RoomCatList\RoomCatList.dll`，`N` 使用下一个连续编号。启动游戏即可。

## 编译

需要 Visual Studio 2022 C++ Build Tools、Windows SDK、Python 3（含 Pillow）和已安装的游戏。将仓库放在 `Mewgenics/mods/RoomCatList` 后运行：

```bat
git submodule update --init --recursive
build.cmd
```

产物位于 `build/`。退出游戏后运行 `apply-update.cmd`，安装编译出的 DLL、字体和侧栏资源。

## 鸣谢

感谢 [Mewjector](https://github.com/githubuser508/mewjector)、[MewUI API](https://github.com/Pseudonym-Tim/mewgenics-ui-api) 和 [Dear ImGui](https://github.com/ocornut/imgui)。第三方许可证见 `licenses/`。原游戏资源归游戏权利人所有。
