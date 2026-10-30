PiJava NSP 构建（Fit25）

这是独立应用安装包，程序取自本轮 Switch ELF；不需要在 SD 卡另放一个 NRO。
沿用历史 Title ID 05004A324D450000。图标来自 assets/nsp/control，生产 ROMFS
与同轮 NRO 相同，不包含商业游戏或用户存档。NSP 属于自制应用包，使用既有
支持自制应用的系统环境；内容校验不等于实体 Switch 安装/运行验证。

1. 配置 DEVKITPRO，按项目 README 完成 SDK 环境，先构建：
   python3 -B tools/build.py --switch --frozen-java

2. 解压本目录 hacBrewPack-source.zip 到 build/nsp-toolchain 等临时目录。
   必须先核对 hacBrewPack-source.json 中的 SHA256；源码保持上游原样并带许可证。
   进入解压后的 hacBrewPack-master，把 config.mk.template 复制为 config.mk，运行 make。
   这只构建打包工具；不需要把工具二进制放入源码交付。

3. 私有 keyset 需提供 header_key 和 key_area_key_application_00。
   keyset 必须由使用者自行提供；源码与补丁不包含此输入。构建过程只在临时私有
   文件中留下这两项，结束后自动删除。不要把 prod.keys/title.keys 放入源码目录。

4. 打包与完整性校验：
   python3 -B tools/build_nsp.py --hacbrewpack /absolute/path/hacbrewpack --keyset /private/path/prod.keys

默认产物为 build/J2me-vm-<VERSION>.nsp。build/nsp/build-result.json 记录产物
身份和验证结果；校验检查三个 NCA、CNMT 引用、文件哈希/分区完整性、Title ID、
应用版本、NSO 与 ELF 对应关系、NPDM 配置、图标及全部生产 ROMFS 字节。
Nintendo 固定签名和 ACID 官方签名为空是自制包的预期属性，不能声称官方签名
验证成功；程序 NCA 的 NPDM 签名与所有内容哈希仍须通过。

打包器使用随机加密材料时，重复打包的 NSP 字节可能不同；通过同一程序/资源
逐字节校验和实际产物 SHA256 追溯，不承诺不同机器重复打包的加密容器 SHA 相同。

配置采用当前 switch-tools NPDM 格式，保留应用执行、三核线程和 JIT 所需权限。
debug flags 仅启用 allow_debug，符合新版 npdmtool 的互斥约束，也避免在较旧
系统/CFW 中使用当时仍为保留位的 force_debug 位。JIT SVC、核心和线程优先级
权限保持不变；旧版 hactool 对新 debug 位的文字显示不作为能力验证依据。
生产运行时与 Java/ROMFS 不因为 NSP 打包而改变。

官方/上游格式依据：
https://github.com/rlaphoenix/hacBrewPack
https://github.com/switchbrew/switch-tools
https://github.com/SciresM/hactool
