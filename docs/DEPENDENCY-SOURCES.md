# 原生依赖源码与构建配方

本次 NRO/NSP 使用的 FFmpeg 为 GPL 配置。依赖源代码与应用源代码分别提供，第三方原许可保留。此补充仅含上游源码、补丁、配方与许可文本，没有 SDK 可执行程序、密钥、商业游戏或本项目测试报告。

[下载原生依赖源码补充包](https://github.com/incuryou/PiJava/releases/download/v0.2.114-keyui6-gpuf5-save1/PiJava-native-dependency-sources.zip)

补充包 SHA256：`902a59c53f49aaf4789d4921a4d0008a5b21f67141a161cbdd4bb8689ba0f984`。

devkitPro 配方固定提交：[`b9e8b0de6a6423557fd2a57dc100bf327c79dd4a`](https://github.com/devkitPro/pacman-packages/tree/b9e8b0de6a6423557fd2a57dc100bf327c79dd4a)。

| 依赖 | 配方版本 | 原始源码 | 配方 |
| --- | --- | --- | --- |
| FFmpeg | 7.1-5 | [官方源码](https://ffmpeg.org/releases/ffmpeg-7.1.tar.xz) | [PKGBUILD](https://github.com/devkitPro/pacman-packages/blob/b9e8b0de6a6423557fd2a57dc100bf327c79dd4a/switch/ffmpeg/PKGBUILD) |
| dav1d | 1.5.0-1 | [官方源码](https://code.videolan.org/videolan/dav1d/-/archive/1.5.0/dav1d-1.5.0.tar.gz) | [PKGBUILD](https://github.com/devkitPro/pacman-packages/blob/b9e8b0de6a6423557fd2a57dc100bf327c79dd4a/switch/dav1d/PKGBUILD) |
| bzip2 | 1.0.8-1 | [官方源码](https://sourceware.org/pub/bzip2/bzip2-1.0.8.tar.gz) | [PKGBUILD](https://github.com/devkitPro/pacman-packages/blob/b9e8b0de6a6423557fd2a57dc100bf327c79dd4a/switch/bzip2/PKGBUILD) |
| zlib | 1.3.1-1 | [官方源码](https://github.com/madler/zlib/releases/download/v1.3.1/zlib-1.3.1.tar.xz) | [PKGBUILD](https://github.com/devkitPro/pacman-packages/blob/b9e8b0de6a6423557fd2a57dc100bf327c79dd4a/switch/zlib/PKGBUILD) |
| libnx | 4.12.0-1 | [官方源码](https://github.com/switchbrew/libnx/archive/refs/tags/v4.12.0.tar.gz) | [PKGBUILD](https://github.com/devkitPro/pacman-packages/blob/b9e8b0de6a6423557fd2a57dc100bf327c79dd4a/libnx/PKGBUILD) |
| deko3d | 0.5.0-1 | [官方源码](https://github.com/devkitPro/deko3d/archive/refs/tags/v0.5.0.tar.gz) | [PKGBUILD](https://github.com/devkitPro/pacman-packages/blob/b9e8b0de6a6423557fd2a57dc100bf327c79dd4a/deko3d/PKGBUILD) |

FFmpeg 7.1 原始压缩包及 `ffmpeg-7.1.patch`、`tls.patch`，dav1d 配套补丁，以及各包许可均在补充包中。上游存档保持原样，包含其完整源树；这与本项目排除游戏测试数据和私有报告的规则分别处理。

版本依据包括交接源码的依赖记录、产物内版本字符串和历史 SDK 信息。本轮核对了配方指定源码/补丁的哈希；交付报告没有提供本次完整 SDK 包清单或静态库逐项哈希，未据此声称已从零独立重建这些库或通过设备验收。完整索引和证据边界见补充包 `SOURCE-INDEX.json`。

应用源代码见本仓库相同 Release 的源码附件或对应 Git tag。FFmpeg 不得被描述为仅 LGPL；其不同源文件和其他依赖保留原许可。
