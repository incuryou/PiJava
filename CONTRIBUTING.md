# 参与贡献 / Contributing

欢迎报告可复现的问题、完善文档、编写最小 MIDlet 与提交小范围修复。
提交前请阅读 [README](README.md)、[构建](docs/BUILDING.md) 和[当前状态](docs/STATUS.md)。

## 报告问题

请搜索已有 Issue，并提供：

- PiJava 版本或提交 ID，使用自行构建的程序时附编译器和依赖版本。
- Switch 型号、系统及自制软件启动方式，或 Linux 发行版和运行命令。
- 游戏名称、发行版本、目标分辨率和 JAR SHA-256；这些信息通常比文件名更可靠。
- 从启动开始的最短复现步骤、预期结果和实际结果。
- 相关皮肤、滤镜、Native / 30 / 60、按键映射与是否使用即时存档。
- 经过删减的相关日志，明确问题发生在启动、游戏中、保存、继续还是退出。

不要上传商业 JAR、游戏素材、RMS、即时存档、账号信息、密钥或个人数据。
如需日志，先删除无关路径、个人信息及其他敏感内容。
不要将包含整个 SD 卡目录或运行环境的压缩包作为复现材料。
优先使用你自己编写、允许公开的最小测试 MIDlet。
若只能用商业游戏触发，提供其摘要与步骤，不附游戏文件。

在 Linux 上可计算摘要：

```sh
sha256sum MyGame.jar
```

兼容性报告应标明实际覆盖场景。只进入标题页时，不要写“全流程通过”。
构建成功、PNG 输出正常和 Switch 实机正常是不同结论。

## 提交修改

先说明问题、触发条件及修改后的行为。较大兼容性或结构变更可先用 Issue 讨论。
保持修改集中，避免在修复中混入无关的重命名、格式重排或资源替换。
修复 VM、调度、GC、存档或 API 行为时，尽量附能区分修复前后的最小测试。
文档或静态资源的小改动，请按实际影响做适当检查。

修改 `java/` 后，应通过构建脚本重新生成类库。
如果触及经过审核的类库内建绑定契约，不要绕过摘要校验；解释语义变化并同步审查绑定。
修改即时存档字段、类布局或游戏身份规则时，应明确兼容性影响。
不要为了让测试通过而静默接受旧格式或混用他人的数据。

提交或 PR 描述请写清：

1. 解决了什么具体问题，行为如何改变。
2. 实际运行的命令、平台和结果；未运行的相关验证也应说明。
3. 已知限制，以及是否影响存档、RMS、按键配置或第三方依赖。

测试入口见 [BUILDING](docs/BUILDING.md)。不要求把未运行的测试写为通过。
请勿提交本地 `build/`、运行日志、个人配置、游戏数据、SDK 或商业游戏文件。

## 许可与第三方内容

项目自有代码采用 `GPL-3.0-or-later`，见 [LICENSE](LICENSE)。
提交自有修改时，请确保你有权按项目许可提供该内容。
引入第三方内容必须保留原始版权与许可说明，并说明来源、版本和修改情况。
字体、图片、头文件和音频资源同样需要可再分发的授权。
依赖或二进制打包变化请同步更新 [THIRD_PARTY.md](THIRD_PARTY.md) 及相关许可记录。

## English

Please include the exact PiJava version, platform and launch method, reproduction
steps, expected/actual behavior, JAR SHA-256, relevant settings and a small redacted
log. Do not attach commercial JARs, assets, RMS, savestates, keys or personal data.
Prefer a small, shareable MIDlet you wrote yourself.

Keep patches focused and document tests actually run, including their platform
and scope. VM, scheduling, GC, API and save-format fixes benefit from a minimal
regression case. Rebuild Java classes through the build script and do not bypass
reviewed intrinsic checks. Describe save/configuration compatibility impacts.
Project-owned code is GPL-3.0-or-later; preserve third-party terms and attribution.
