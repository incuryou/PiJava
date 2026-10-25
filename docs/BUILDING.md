# 构建 / Building

本文对应 `0.2.114-keyui5`。所有命令均从仓库根目录执行。
首发以源码为主；自行构建所需的编译器与 SDK 不随仓库提供。

## Linux 桌面诊断程序

需要：

- Linux 和 GCC / G++，C++ 编译器支持 C++17，C 编译器支持 C11。
- Python 3.10+；普通构建使用 Python 标准库。
- 完整 JDK，包含 `javac` 或 `jdk.compiler` 模块，并支持 `-source 8 -target 8`。
- 压缩音频诊断需要 FFmpeg 7 系列共享库，ABI 为 avcodec 61、avformat 61、avutil 59。

上游 keyui5 工程的验证环境使用过 GCC 14、JDK 21、Python 3.13 与上述 FFmpeg ABI。
这是已使用的环境组合，不表示所有版本组合都经过验证。
普通编译不需要 Pillow；重新制作皮肤等资源时才需要相应制作工具的额外依赖。
所需字体资源已包含在公开源码中，无需寻找早期私有源码包。

公开导出替换了映射页的手机示意资源，完成本机资产验证，但尚未重新完成整树 Switch 构建。
请在自己的环境按本文构建并验证；上游构建记录不代表此公开树已经全部验证通过。

```sh
python3 -B tools/build.py
```

输出为 `build/j2mevm`。它是无窗口诊断程序，不提供桌面游戏 GUI。
编译器可通过 `CC` / `CXX` 环境变量或 `--cc` / `--cxx` 指定，Java 编译器可用 `--javac` 指定。

构建脚本会重新编译 `java/`，将 Java 8 字节码写入 `romfs/classes/`，
并更新用于类库内建绑定校验的生成清单。
如果本地 `romfs/jar/` 已有与 `examples/` 对应的示例 JAR，桌面构建会重编其中的类，
保留非 class 资源与明确列出的手写字节码夹具。
源码导出不保证附带这些生成后的 JAR；构建脚本不会凭空为每个示例目录新建 JAR。

压缩音频在 Linux 通过动态加载 `libavcodec.so.61`、`libavformat.so.61`
和 `libavutil.so.59` 实现。能链接桌面程序不代表这些运行库已经安装。
不同主 ABI 不能直接代替；不要通过重命名不兼容的共享库绕过检查。

## Switch NRO

请从 [devkitPro 官方文档](https://devkitpro.org/wiki/Getting_Started) 安装适合你的系统的工具链。
需要 devkitA64、libnx、switch-tools，以及默认 GPU 呈现构建所需的 deko3d。
还需要 Switch portlibs 中的静态库：

```text
avformat avcodec avutil swresample dav1d bz2 z
```

FFmpeg 头文件主 ABI 必须为 `avcodec=61`、`avformat=61`、`avutil=59`。
当前适配器面向 FFmpeg 7.1 公共 API；更新到其他主 ABI 需要相应代码适配与验证。
工具链、静态库及其再分发义务由构建者自行准备和核实。
本仓库不会下载 SDK 或使用私有仓库中的工具链压缩包。

先设置 `DEVKITPRO` 为你实际安装的 devkitPro 根目录。
例如在采用其标准 Linux 安装位置时：

```sh
export DEVKITPRO=/opt/devkitpro
python3 -B tools/check_switch_env.py
python3 -B tools/build.py --switch
```

检查脚本会核对编译器目标、依赖文件和 FFmpeg 头文件 ABI，
并将依赖版本与文件摘要写入 `build/switch-toolchain.json`。
该检查不安装软件、不下载文件，也不验证 Switch 实机运行。

默认输出：

| 文件 | 用途 |
| --- | --- |
| `build/J2me-vm-0.2.114-keyui5.nro` | 自行部署到 Switch 的程序 |
| `build/switch/j2mevm.elf` | 本地链接产物 |
| `build/switch-toolchain.json` | 当前构建环境记录 |

构建会重新编译 Java 类库，并将 classes、字体、皮肤和 UI 资源打包进 ROMFS。
Switch 生产 ROMFS 不包含示例游戏 JAR。
脚本检查 NRO 头、ROMFS、图标及默认启用的 NACP 元数据。
这些检查不替代真机启动、声音、输入、退出和长期运行测试。

可选参数：

```sh
# 不编译 deko3d 呈现后端；仍需其余 Switch 依赖
python3 -B tools/build.py --switch --switch-cpu-only

# 查看全部参数
python3 -B tools/build.py --help
```

其他参数包括 `--no-nacp`、`--app-title` 和 `--app-author`。
CPU-only 构建不代表游戏 3D API 已关闭；它控制的是宿主最终呈现后端。

## 可选：重建公开手机示意资源

普通构建直接使用已打包资源，无需重新制作皮肤或其他现有资产。
映射页的原创无品牌手机示意可由以下脚本完整重建，需要 Pillow，不需要字体或外部图片：

```sh
python3 -B tools/generate_public_phone.py --check
python3 -B tools/generate_public_phone.py
```

`--check` 仅验证已提交资产，不写文件；不带参数时重建 SVG、PNG 和运行时 RGBA。
运行时文件保留历史名称 `romfs/ui/nokia5300.rgba`，供原有载入路径使用；图像本身没有品牌标识。

## 本地验证

以下定点检查使用主机 G++ 编译小型测试程序，并将证据写入指定目录：

```sh
python3 -B tools/test_keymap_pro_ui.py --evidence build/checks/keymap
python3 -B tools/test_controller_skins.py --evidence build/checks/skins
python3 -B tools/test_filters.py --evidence build/checks/filters
```

测试不是一个可替代全部游戏与实机验收的总开关。
部分历史测试依赖特定本地游戏或生成夹具；运行前先看脚本参数和输入要求。
提交结果时请附命令、工具版本、退出码与覆盖范围，避免笼统写“全通过”。

## English notes

Build on Linux with GCC/G++ (C++17), Python 3.10+, and a full JDK supporting
Java 8 bytecode output. The project has used GCC 14, JDK 21 and Python 3.13.
Run `python3 -B tools/build.py` for the headless `build/j2mevm` executable.
Compressed audio loads FFmpeg ABI 61/61/59 shared libraries at runtime.

For Switch, install devkitPro/devkitA64, libnx, switch-tools, deko3d and the
listed portlibs yourself, set `DEVKITPRO`, then run the environment check and
`python3 -B tools/build.py --switch`. The default NRO embeds classes, fonts,
skins and UI resources, but excludes example JARs. No SDK download is supplied.
Build and dependency checks do not constitute physical Switch validation.
Upstream keyui5 build records predate the public phone illustration replacement;
the complete public tree still requires Switch build validation.
Optionally use `tools/generate_public_phone.py --check` to verify that asset, or
run it without arguments to regenerate it. This requires Pillow but no fonts
or external image input; normal builds use the existing packed assets.
