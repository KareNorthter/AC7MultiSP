# 本机资产索引

这里供AI和开发者查原料、历史现场及恢复位置。玩家文档仍在docs/user/。本机主根为`E:/AC7_MultiSP`；下文相对路径从该根开始。GitHub保存源码和索引，不包含这些被忽略的大文件。

## 先找哪一类

| 用途 | 本机位置 | 使用方法与边界 |
|---|---|---|
| 当前77资源输入 | Temp/integration-2.23.0-refresh/Temp/mount-assets-2.23.0-refresh/source | 原位保留154成员；来源见[开发说明](../engineering/development.md)。旧2.14/2.20输入不能代替 |
| 挂载及动画待续原料 | Temp/remaining-native-mount-research/、Temp/mount-assets-*、Temp/f22a-*20261017-1144* | 原位保留；部分manifest和研究脚本读取绝对路径，搬动会使旧处理失败。具体方案见[研究索引](../research/README.md) |
| 已封存工作树及冻结产物 | 工作区索引列出的Temp/integration-*及研究目录 | 原位保留；不要从文件夹名猜版本，也不要直接在封存树继续写。新工作从main开分支 |
| 未接收的原根目录资料 | Temp/preserved-local-20261023 | 27项原未提交资料与兼容现场；本身是Git工作树，不是普通临时文件 |
| 崩溃与诊断现场 | Temp/crash-capture-*、Temp/pw01-*及其他诊断目录 | 完整转储和现场原位保留，按回执定位；不能把普通退出记录当作已找到根因 |
| 安装器、Vortex与兼容研究 | Temp/delivery-*、Temp/vortex-*、Temp/compatibility-*等 | 保留现有来源与现场；安装器代码仍在独立分支，本轮未接收 |
| Mod机体分组续研 | Temp/tasks/20261023-mod-aircraft-compat/、backup/aircraft-mod-profiles-*；游戏根AC7AircraftProfiles/ | 本轮分组原料、未解析引用清单、安装快照和切换事务；按[续研正文](../research/mod-aircraft-compatibility.md)恢复，不清理分组库或把临时名称当可删依据 |
| Mod机体长按兼容静态原料 | Temp/tasks/20261024-mod-hold-switch-static/ | 作者附加类索引、历史按下/松开采样的离线反汇编及摘要；不含本轮进程采集，原始历史采样仍在归档位置，见[续研正文](../research/mod-aircraft-compatibility.md#长按武器切换键的附加功能) |
| 已归组的散落脚本、报告和附件 | backup/local-assets-20261023/maintenance-scripts、reports、notes、misc等 | 保留原文件名，按下节清单从原路径或新路径检索；属于历史材料，不作为现行构建入口 |
| 旧检查、安装暂存与编译中间件 | 同归档根verification-fixtures、empty-staging、build-output | 集中保存旧检查副本、空暂存目录及build-ui对象文件；不是源码，也不表示本轮运行过检查 |
| 个人资料、安装保护与历史恢复 | backup/内原有各目录 | 保持原位，不合并覆盖不同次备份；项目级恢复入口见[归档说明](README.md) |

## 按原文件名定位

主页及发布新增材料见[主页资产](../project/pages-site.md#现有资产及接收状态)：`Temp/tasks/20261024-release-2.25.2/`是本次EXE、Nexus通用ZIP和新主页发布材料；`Temp/tasks/20261024-updates-web-publish/`保留来源线程旧主页ZIP、清单及快照，`backup/pages-intake-20261024/`保留接收前备份。原资产清单是2026-10-23快照，不回写旧清单来假装包含这些后续材料。

[资产清单](../../evidence/local-assets-20261023.json)逐项列出整理前的原路径、整理后的实际路径、用途、大小、已知引用与处置。清单是本机整理快照，不是文件运行时扫描结果；后续新文件仍需核对现场。

- 找旧脚本、截图或JSON时，先在清单搜索原文件名，再使用current字段的位置。原名保留，不会因分组而改名。
- 迁移的文件在移动前后核对了SHA256；完整成员清单、移动日志和核对结果留在`backup/local-assets-20261023/`。未移动的115GB级原料仅做目录、数量和大小盘点，没有重新读取全量字节核验。
- 仍按自身目录定位、被主线/安装器/保留Temp脚本引用、带链接或属于Git登记的目录，保持原位。源码中的旧资源默认路径没有因此自动更新。
- 历史脚本可能依赖旧源码、旧版本或原绝对路径。重放前先看引用及代码，必要时从归档复制回清单original位置；确认目标不存在或已单独备份，不覆盖当前成果。需要构建、测试、安装或运行游戏时仍按当次授权执行。
- `Temp/weapon-model-json`和`Temp/hud-assets`等旧脚本默认输入目前缺失；`remaining-native-mount-research/intake.json`也含已缺失工作树的旧路径。现存索引不证明这些原料已恢复，继续前先说明具体缺项。

## 后续文件怎么放

新任务的临时脚本、报告和截图统一放`Temp/tasks/日期-主题/`，不再直接写满Temp根目录。任务结束后把独有结论写入对应工程/研究正文；需要长期保留的原料和现场登记位置，不能只留一串聊天记录。

已经冻结、被回执引用或供研究处理的目录，不为目录美观再搬一次。目录名出现Temp不表示可以删除。清单中的“原位保留”与“归组保存”都不代表准许运行、安装或清理。

本轮整理保存原文件，没有永久删除或以压缩丢弃内容，也没有以释放空间为目标。大文件和本机工具继续留在本地，GitHub上传范围见[仓库与本机边界](../project/repository.md)。
