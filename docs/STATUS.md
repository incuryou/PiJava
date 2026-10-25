# 当前状态 / Status

版本：`0.2.114-keyui5`。阶段：实验性源码公开首发。
本文是当前能力和验证边界的说明，不是游戏兼容性排行榜。

## 实现范围

| 范围 | 当前情况 |
| --- | --- |
| Switch 宿主 | 游戏列表、皮肤、滤镜、每游戏速度、采样 FPS 与游戏内菜单 |
| Linux 宿主 | 无窗口运行、日志与 PNG 输出，用于开发和回归 |
| VM 与类库 | C++17 虚拟机与随源码提供的 Java 类库，按项目实现范围运行 MIDlet |
| 2D / 字体 | Canvas / GameCanvas、图像、文字、字体与编码路径；需逐游戏确认 |
| 音频 | MIDI / WAV 与 FFmpeg 压缩音频路径；音色、格式和时序并非原手机的完整复刻 |
| 3D / 厂商 API | 有部分 M3G、Mascot Capsule 与厂商扩展实现，覆盖不完整 |
| 数据 | 独立 RMS、J2S5 即时存档；跨版本和类布局兼容不能一概保证 |

## keyui5 相关行为

- 每个手机按键三个映射槽，支持当前游戏覆盖与全局默认。
- 左摇杆默认使用手机方向键，不属于数字 2468 的默认绑定。
- 十字键遵循游戏方向模式；右摇杆方向保留数字 2468 默认绑定。
- `L3` 固定打开游戏内菜单，`L / R` 切换存档页与映射页。
- 映射输入列表可按住方向键滚动，`A` 确认草稿，`+` 保存。
- Pro 手柄预览及游戏皮肤的文字标注跟随映射；保留旧手机皮肤。
- 游戏列表左右仅切换面板，设置值由 `A` 更改。

## 已验证的层级

上游 keyui5 工程的开发记录包含定点检查，以及 Linux 与 Switch 各一次构建通过。
定点检查关注映射页选择与保存流程、按住滚动及其复位边界、手柄预览和皮肤相关行为。
仓库保留相应测试源码，可参考 [BUILDING](BUILDING.md) 在自己的环境复现。

公开导出未改动 `src/` 和 `java/`，将映射页手机示意替换为原创无品牌资源，
并完成本机资产验证。原运行时文件名保留，SVG、PNG 与生成脚本一并提供。
替换后的公开整树尚未重新完成 Switch 构建，仍需使用者构建验证。
上游的定点与构建记录不能作为此公开提交的完整通过结论。

请按以下层级理解结果：

1. **源码与定点测试**：证明指定断言在指定测试环境下成立。
2. **Linux 构建**：证明主机编译、链接完成；运行结果仍取决于输入和运行库。
3. **Switch 构建**：证明交叉编译、链接和 NRO 结构检查完成。
4. **Switch 实机**：还需要独立确认启动、按键、显示、音频、存档、退出与长期运行。
5. **游戏兼容性**：还需要明确游戏版本与场景，不能由以上检查自动推导。

没有将本次构建记录表述为全量测试、所有游戏通过或全部实机验收。
公开导出也不包含开发过程中使用的全部游戏输入、私人测试数据或历史执行证据。
如需发布新的兼容性结论，应记录当前提交、命令、工具链和可复现输入。

## 已知限制与待验证事项

- 不同厂商与地区发行的同名游戏可能使用不同 API、编码和时序；请提供 JAR SHA-256。
- M3G 与厂商扩展的实现不完整，不能声明“完整 3D 支持”，也不能据此说完全没有 3D 实现。
- Native / 30 / 60 为速度目标；采样 FPS 不等于游戏逻辑 FPS。
- J2S1–J2S4 被明确拒绝；J2S5 的类布局、JAR 身份和版本条件仍需符合恢复要求。
- RMS 与即时存档独立，恢复快照不能当成完整磁盘回滚。
- 默认画布为 240×320，其他逻辑尺寸也映射到固定 480×640 显示区，可能拉伸。
- 通用异常恢复、无响应检测及硬卡死兜底仍有设计与实现工作，不能保证所有故障都能返回菜单。
- Linux 前端用于诊断，没有 Windows / macOS GUI 的正式支持承诺。

## 反馈方式

请报告具体的“预期行为 / 实际行为 / 复现步骤”，并说明是构建、主机诊断还是实机问题。
兼容性报告请分清已到达的场景，例如标题画面、开始游戏、关卡、声音、保存、继续、退出。
若仅完成启动，请只写启动结果。
附件限制和最小复现方式见 [CONTRIBUTING](../CONTRIBUTING.md)。

## English summary

PiJava is experimental. The upstream keyui5 project recorded focused checks and
a single Linux build and Switch build. The public export replaces the phone
illustration with an original unbranded asset and includes local asset checks.
The complete public tree has not yet been rebuilt for Switch; users still need
to build and validate it. Upstream records do not establish a complete pass for
this public commit, all-game compatibility or full physical Switch acceptance.
Partial 3D and vendor APIs are present but incomplete.
General recovery from errors and hard hangs remains unfinished.
Treat speed targets, sampled FPS and savestate format tags within their stated
limits, and report reproducible scenarios with the exact version and JAR hash.
