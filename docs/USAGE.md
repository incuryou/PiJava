# 使用 / Usage

本文对应 `0.2.114-keyui5`；构建方法见 [BUILDING](BUILDING.md)。
请使用自己有权运行的游戏。仓库不提供商业游戏下载。

## SD 卡目录

```text
SD:/switch/j2me/
├── J2me-vm-0.2.114-keyui5.nro
├── games/
│   ├── MyGame.jar
│   └── MyGame.jar.screen       # 可选逻辑分辨率
├── logs/                      # 运行时日志
├── state/                     # 即时存档
├── rms/                       # RecordStore 数据
├── menu.cfg                   # 宿主显示设置
├── speed.cfg                  # 每游戏速度目标
└── keymap.cfg                 # 全局及每游戏映射
```

数据目录和配置由程序按需创建，不需要手工填写空配置文件。
仅扫描 `games/` 直属目录下的 JAR，扩展名大小写均可。
程序内部日志将 SD 路径写作 `sdmc:/`，文档中的 `SD:/` 指 SD 卡根目录。

当前日志为 `logs/j2me-vm.log`；上一次运行的日志轮换为 `logs/j2me-vm.prev.log`。
出现问题后先复制相关日志，再重新启动，以免覆盖有用信息。

## 游戏列表

| 输入 | 行为 |
| --- | --- |
| 上 / 下 | 选择游戏 |
| `L / ZL`、`R / ZR` | 上一页、下一页 |
| `A` | 从头启动所选游戏 |
| `X` | 读取所选游戏已有的即时存档 |
| 右 | 转到设置面板 |
| 左 / `B` | 返回游戏列表 |
| `+` | 打开所选游戏的删除管理界面，按屏幕确认提示操作 |

设置面板中，上下选择皮肤、滤镜、速度或 FPS，按 `A` 更改。
左右只切面板，不更改设置值。设置写入失败时会显示提示。
皮肤、滤镜和 FPS 开关为全局宿主设置；速度目标跟随所选游戏。

## 游戏内默认输入

| Switch 输入 | 默认手机输入 |
| --- | --- |
| `X` / `B` | 左软键 / 右软键 |
| `A` | 根据游戏方向模式发送确认或数字 `5` |
| `Y` | 手机确认 |
| `L` / `R` | `1` / `3` |
| `ZL` / `ZR` | `7` / `9` |
| `−` / `+` | `*` / `#` |
| `R3` | `0` |
| 左摇杆四方向 | 手机方向键 |
| 十字键四方向 | 根据游戏方向模式发送手机方向键或 `2 / 8 / 4 / 6` |
| 右摇杆四方向 | `2 / 8 / 4 / 6` |
| `L3` | 固定打开 / 关闭游戏内菜单 |

左摇杆默认不绑定数字 `2 / 4 / 6 / 8`。
上表描述自动默认映射；全局或每游戏自定义配置可以覆盖默认输入。
每游戏配置优先于全局配置。`L3` 始终保留为菜单输入。

## 修改按键映射

游戏中按 `L3` 打开“存档与退出”，再按 `L / R` 切到“修改按键映射”。

1. 方向键选择手机按键，`A` 进入它的映射槽。
2. 左右选择三个槽之一，`A` 打开 Switch 输入列表。
3. 选择输入或“未设置”，`A` 将选择写入当前草稿。
4. 按 `+` 保存，确认页面出现保存成功提示。

输入列表支持按住方向键滚动。`B` 依次返回槽、手机键盘和游戏。
未按 `+` 保存的草稿不会成为已保存映射。
`X` 恢复自动默认草稿，随后仍需 `+` 保存。
`Y` 在当前游戏与全局保存范围间切换，页脚显示当前可切换的操作。
保存全局配置会移除当前游戏的单独覆盖；其他已有的每游戏覆盖仍优先。

一个手机按键最多三个输入；多个手机按键也可共用一个实体输入。
米白、蓝色、黑色 Pro 皮肤与映射页中的手柄标注会反映有效绑定。

## 显示与速度

默认皮肤为“手柄米白”，还有“手柄蓝”“手柄黑”、三款旧手机皮肤和无边框。
滤镜按 `A` 循环：原画 → 双线性 → Scale2x → LCD → CRT → ScanlinesDX。
滤镜处理最终像素，不改变游戏逻辑。

默认 Java 画布是 `240×320`。需要不同逻辑尺寸时，在 JAR 旁创建同名附加文件：

```text
SD:/switch/j2me/games/MyGame.jar.screen
```

文件内容为一行，例如 `240x320`。宽高各允许 `16..640`。
文件无效或不可读取时不会选用其尺寸。
Switch 上的显示区域固定为 `480×640`，其他逻辑尺寸也拉伸到该区域，
不会自动按游戏宽高比增加黑边。
改变尺寸后请从头启动验证，避免将旧即时存档当成分辨率转换工具。

Native 尽量遵循游戏本身的节奏；30 / 60 为速度目标。
游戏逻辑、绘制、睡眠及负载都会影响实际表现，选择 60 不保证达到 60 FPS。
FPS 显示统计的是宿主交付画面的采样频率，不能直接解释为游戏逻辑帧率。

## 保存、继续与备份

在 `L3` 菜单“存档与退出”页，上下选择保存即时存档、删除即时存档或退出游戏。
按 `A` 执行；删除和退出有二次确认，`B` 取消。
保存后回到游戏列表，`X` 尝试继续当前选择的游戏。
每个 JAR 文件名对应一个即时存档位置，例如 `state/MyGame.jar.sav`。
更换同名 JAR 不会让原存档自动适配新游戏；恢复仍会核对游戏身份。

即时存档采用 J2S5，拒绝 J2S1–J2S4。
即使标记相同，也不要假定不同类库布局、版本或 JAR 的快照可以混读。
RMS 位于 `rms/`，按游戏身份隔离，与即时快照独立。
恢复快照不会回滚 RMS；删除快照不会删除游戏内保存的数据。

更新程序或尝试迁移前，退出程序并备份自己的整个 `SD:/switch/j2me/` 数据目录。
不要只复制一个快照后就删除原始 RMS。
若恢复失败，保留备份和最小错误日志，再尝试从头启动该游戏。
旧 RMS 导入属于显式兼容操作，普通使用不需要；不要随意混入其他游戏的数据。

## Linux 无窗口运行

```sh
J2ME_FRAMES=120 J2ME_RMS_DIR=build/local-rms ./build/j2mevm \
  --cp romfs/classes --jar MyGame.jar \
  -o build/game.png --log build/game.log
```

将 `MyGame.jar` 换成自己的文件。`--cp` 指向重新构建的类库。
`-o` 指定 PNG；`--log` 指定日志；`--size 240x320` 可显式指定逻辑画布。
`J2ME_FRAMES` 是诊断主循环次数，不保证与游戏帧数一一对应。
默认 RMS 使用临时目录；用 `J2ME_RMS_DIR` 可为本次测试隔离数据。
没有交互窗口或完整的 Switch 菜单模拟。

## English quick reference

Place your JAR directly in `SD:/switch/j2me/games/` and launch your locally built NRO.
In the list: up/down select, `A` starts, `X` resumes, shoulder buttons page,
and left/right switch between list and settings. Change settings with `A`.

During gameplay, `L3` opens Save / Exit; `L / R` switches pages. On the mapping
page, confirm the phone key, slot and input with `A`, then persist with `+`.
`X` resets the draft; `Y` switches per-game/global scope. `L3` stays reserved.
Save deletion and game exit ask for confirmation.

Back up the entire data directory before updates. J2S5 rejects J2S1–J2S4,
and does not promise compatibility between different layouts or builds.
RMS is independent of instant savestates. Logs rotate between the current and
previous log files. Never attach private saves, commercial JARs or keys to an issue.
