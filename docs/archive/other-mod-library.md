# 其他 AC7 Mod 的本机管理

本页记录 2026-10-27 对其他 AC7 Mod 原包和管理入口的整理；它不代表安装状态，也不替代作者的说明。集中入口为 `E:/AC7_MOD/`，第三方文件和个人状态不提交 Git。

## 从哪里管理

`E:/AC7_MOD/管理入口/` 集中放置快捷方式。快捷方式指向原工具，保留原脚本及其相邻依赖；更换盘符或移动原工具后须重新核对目标。

| 入口 | 用途与边界 |
|---|---|
| 机体-A组、机体-B组 | 使用原分组工具切换机体组，退出游戏后使用 |
| 机体-XFA-33组 | 切换到 XFA-33 替换组，会替换 Su-30M2、Su-33 和 Su-30SM |
| 机体-恢复原安装组 | 使用同一工具恢复原安装组；不负责恢复自定义呼号包 |
| 一键切换Gunplay | 关闭游戏后双击，在启用与停用之间切换 Enhanced Gunplay A5 主文件 |
| 恢复B组_结束真实冲突测试 | 退出游戏后先保存日志，再将XFA-36A/F-17V/Gleipnir真实冲突组恢复到测试前B组；流程见[故障测试指导](../engineering/aircraft-fault-testing.md#2026-10-28真实冲突测试组) |
| AC7 Integrated-图形界面 | 第三方鼠标飞控及附加 Mod 工具；功能、适用范围和操作以其自带说明为准 |
| Vortex-Mod管理器 | 打开本机 Vortex；其管理状态、下载和部署仍由 Vortex 自己维护 |

四个机体入口原文件在 `E:/SteamLibrary/steamapps/common/ACE COMBAT 7/`，实际程序、清单、分组和事务在其 `AC7AircraftProfiles/`。工具的分组、保护及已知限制见[机体续研](../research/mod-aircraft-compatibility.md#分组与安装方式)。AC7 Integrated 入口指向 `E:/AC7_MultiSP/tools/AC7-Integrated-v0.7.0-beta.1/Start-GUI.cmd`，该工具有自己的配置和管理范围，并非上述机体分组工具。

不要通过移动 PAK 或混用管理器推断当前启用状态；具体操作先退出游戏，并查看相应工具的提示和原说明。快捷方式只提供统一入口，本次没有运行切换、安装或游戏。

Gunplay入口调用本项目的[本机切换脚本](../../src/research/gunplay_toggle.py)，只处理主文件 `!EOM_EnhancedGunplay_A5_P.pak`，沿用此前低优先级命名；其他Mod的同名资源可能覆盖其部分效果。脚本在游戏退出后与安装器共用锁，并核对主文件 SHA256；有其他Gunplay PAK、补丁、链接或未完成准备文件时停止，提示人工核对。停用时主文件移入 `E:/AC7_MOD/管理入口/Gunplay停用仓库/`；首次启用时从 `E:/AC7_MultiSP/backup/gunplay-hangar-isolation-20261021-230355/` 的已核对原件复制，不改原备份。2026-10-27建立入口时主文件处于停用状态，本次没有实际运行启停，效果和兼容性待用户验证。此前机库崩溃记录未证明Gunplay是唯一原因，见[隔离记录](../../evidence/special-aircraft-native-fallback-20261022.json)。

`E:/AC7_MOD/部署与分组文件清单.csv` 是 2026-10-27 的只读磁盘快照，列出游戏部署目录和机体分组库中的 PAK、大小与链接目标。它不是实时列表，不证明每个文件已启用；有变化后需重新盘点。

## 原包分类

| 集中位置 | 内容 |
|---|---|
| `机体大全/` | 原 `下载收藏/ACE7/` 的完整集合，保留其内部原文件名与目录结构 |
| `其他Mod/` | 其余已整理原包直接放在同一目录，包括 AC7MOD、CFA-50、零散下载、QQ 接收、研究、反馈和 Vortex 原包副本 |

2026-10-27 再次整理后，机体大全有 110 个文件，其他 Mod 有 70 个文件。四组同名原包经 SHA256 复核内容一致，每组只保留一份；另有 25 组 ZIP 和 7z 各含同名的单个内部文件，经解压读取并核对内部文件 SHA256 一致，保留机体大全内 ZIP。上述重复副本送回收站，QQ 原件保留。机体大全中四个零字节说明以文件名记录不同信息，保留原样；其他内容或成员不一致的压缩包也保留。逐文件原位置、新位置、处置、大小和 SHA256 在本机 `E:/AC7_MOD/来源与校验清单.csv`，同目录 JSONL 文件提供相同的 209 条来源记录；这份清单不含安装状态。

首次迁入的 22 个原件在迁移前、后核对 SHA256，迁移前的完整校验副本保存在 `E:/AC7_MultiSP/backup/other-mod-library-20261027/入库前已校验副本/`。首次整理过程在 `E:/AC7_MultiSP/Temp/tasks/20261027-mod-consolidation/`；其中中断留下的 F-15SE 部分副本仅作过程留存，不能当作完整原包。本次目录调整的操作日志在 `E:/AC7_MultiSP/Temp/tasks/20261027-simplify-mod-library/`，调整前清单备份在 `E:/AC7_MultiSP/backup/simplify-mod-library-20261027/`。

## 原位保留及原因

- 游戏目录 `Game/Content/Paks/~mods/` 和游戏根 `AC7AircraftProfiles/` 是现有部署及切换工具的数据，移动会影响游戏或分组；本轮按用户选择不移动。
- Vortex 的 `C:/Users/83717/AppData/Roaming/Vortex/acecombat7skiesunknown/` 与 `downloads/` 是管理器工作目录；游戏部署目录中 8 个 Shadow 22 PAK 链接实际指向该管理目录。移动源文件会破坏链接，因此原位保留，集中目录只是下载包副本。
- `F:/下载/ACE7/` 和 XFA-33 原始下载位置被既有研究脚本引用；原文件保留，集中目录提供已校验副本，历史回执不回写。散落在 `F:/下载/` 的 VF-1、UE4SS 和 FOV 编辑器包尚未完整核对全部历史依赖，本轮同样保留原件。
- QQ 的 `F:/Documents/Tencent Files/837179035/FileRecv/` 是聊天接收目录；所选 AC7 机体包复制入库，原件保留，未完成下载和无法确认归属的文件不纳入。
- `E:/反馈/RH172/` 中的原包与反馈日志关联，研究现场 `E:/AC7_MultiSP/Temp/vortex-research-20261021/compatibility-downloads/` 被历史处理引用；两处原件均保留，集中目录提供副本。
- `E:/AC7_MultiSP/Temp/`、`backup/` 中的冻结研究、安装快照及工作树按[本机资产索引](local-assets.md)保留；本项目自身版本和交付包不作为其他 Mod 移动。

不同游戏的 Mod 不混入 AC7 目录；QQ 接收目录中已确认含 Nuclear Option 的汉化和自动驾驶材料，其余归属不明项保留原位。
