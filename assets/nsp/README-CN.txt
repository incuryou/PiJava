NRO与NSP共用assets/icon.jpg。control目录的两个icon_*.dat是该JPEG的逐字节副本，用作未来NSP Control NCA图标。
运行python3 tools/render_icon.py会从assets/app-logo.png重新生成全部图标，不会构建NRO或NSP。
NSP打包时将此control目录中的图标复制到打包器control输入目录；control.nacp和程序应在后续获准构建时生成。本源码未新增完整NSP打包入口，也未生成安装包。
图标格式依据hacBrewPack文档：256x256 JPEG，icon_{Language}.dat。参考https://github.com/pplatoon/hacBrewPack 。
