# 构建 Pi Java模拟器

当前公开源码来自 KeyUI6 / GPUF5 / Save1，生产 `src/`、`java/`、`shaders/` 与全部 ROMFS 文件保持上传源码包的字节。测试与历史工程记录不在公开树中。

## Linux 桌面程序

需要 GCC/G++（C++17 / C11）、Python 3.10+；重编 Java 需要支持 `-source 8 -target 8` 的 JDK。默认路径的压缩音频运行库 ABI 为 FFmpeg 7.1 系列 avcodec61 / avformat61 / avutil59。桌面程序无窗口，用于开发诊断。

```sh
python3 -B tools/build.py --frozen-java
```

产物为 `build/j2mevm`。`--frozen-java` 保留提供的 Java API class 字节；去掉该选项将从 `java/` 重编 API 并更新绑定清单。`examples/` 只保留空目录占位，未包含示例/测试类或游戏。

## Switch NRO

自行安装 [devkitPro](https://devkitpro.org/wiki/Getting_Started)，配置 devkitA64、libnx、deko3d、switch-tools、UAM 与 Switch portlibs。当前基线使用 devkitA64 15.2、libnx 4.12.0、deko3d 0.5.0，以及 switch-ffmpeg 7.1-5；历史版本信息不是重新构建结果。

静态链接库包括 `avformat avcodec avutil swresample dav1d bz2 z` 及平台库。匹配的依赖源码、补丁、配方入口见 [DEPENDENCY-SOURCES.md](DEPENDENCY-SOURCES.md)。字体输入和着色器源码已在本树内；不需要 Nintendo 系统字体文件或商业游戏。

```sh
export DEVKITPRO=/opt/devkitpro
python3 -B tools/check_switch_env.py
python3 -B tools/build.py --switch --frozen-java
```

GPU 着色器通过 UAM 离线生成 DKSH 并固定其身份，无运行时着色器编译。NRO 图标直接嵌入 `assets/icon.jpg`。默认输出为 `build/J2me-vm-0.2.114-keyui5-gpuf5-save1.nro`，NACP 显示短版本 `0.2.114-k5-sv1`。

## NSP 与本次发布

Release 中的 NSP 为用户提供、已经构建的独立应用包，Title ID `05004A324D450000`。它与本次 NRO 使用同一构建 ELF/生产资源的关系来自所附构建报告；公开整理时额外核对了文件哈希和 NRO 全部资源，并未重新打包或进行设备安装验收。

NSP 打包需要独立准备开源打包工具与自己的合法输入；仓库不提供密钥、SDK 安装包或私有打包服务。不要把测试报告中的示例路径当成本机构建路径。

## 源码完整性

```sh
python3 -B tools/verify_source_manifest.py
```

公开清单只覆盖本次筛选后的公开文件，不冒充原始完整源码包的树。构建与 API 重生成会改变清单记录；请在自己的新目录中构建，保留原始快照。
