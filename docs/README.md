# 文档索引

正式状态见[项目入口](../README.md)。本索引说明位置、内容和用途，不重复正文。默认只读当前任务相关项。

## 目录分工

| 目录 | 放什么 |
|---|---|
| user/ | 面向玩家的四份唯一正文：功能、安装、主要更新、作者与授权 |
| project/ | 开发规范、分支与任务工作区登记 |
| engineering/ | 当前架构、开发依赖、挂载设计和踩坑结论 |
| research/ | 待续研究索引和含用户反馈的设计原件 |
| archive/ | 历史恢复索引、开发版本旧记录、授权原件、旧交付资料；不作为当前玩家说明 |

本层只保留总索引。按用途放入现有目录，不再把交接、版本说明副本散放在本层。旧脚本中的文档路径待后续交付改造适配，迁移影响见[开发与恢复](engineering/development.md#文档迁移后的工具边界)。

## 玩家文档：先审阅这四份

以下中文重写稿已获用户基本认可，可继续直接修改；英文对应稿待补齐，本Mod自身授权条款按用户意见留“待作者补充”。四个主题独立阅读，不合成一个长README。中英文均为必需；安装器、ZIP和F6尚未改接新正文，复用约定见[玩家文档维护](project/player-docs.md)。

| 唯一正文 | 内容 |
|---|---|
| [功能说明](user/功能说明.txt) | 多兵装、常用操作、完整模型适配名单与例外、其他Mod兼容性 |
| [安装说明](user/安装说明.txt) | Vortex、EXE、手动三种方式，以及更新、卸载注意事项 |
| [更新日志](user/更新日志.txt) | 按主要功能阶段合并，供玩家快速了解变化 |
| [作者与授权](user/作者与授权.txt) | 作者联系方式、本Mod授权待填项、第三方许可与致谢 |

## 开发规范

| 文档 | 内容与用途 |
|---|---|
| [AGENTS](../AGENTS.md) | AI开工顺序、保护约束和授权边界 |
| [开发规范](project/workflow.md) | 工作、分支、并行、提交、版本、日志、接收与清理规则 |
| [工作区索引](project/workspaces.md) | 正式入口、活动成果、保留工作树与整理记录 |
| [玩家文档维护](project/player-docs.md) | 四份正文的维护、审核和EXE／ZIP／F6复用规则 |
| [仓库与本机边界](project/repository.md) | GitHub私有仓库、上传范围、研究恢复锚及公开前的材料边界 |

## 技术与研究

| 文档 | 内容与维护边界 |
|---|---|
| [架构](engineering/architecture.md) | 当前模块、跨语言接口、生命周期与保护机制，不写逐轮交付 |
| [开发与恢复](engineering/development.md) | 源码/原料路径、工具依赖、接收恢复和未解决事项 |
| [挂载设计](engineering/multi-sp-mount-design.md) | 挂点、净空、布局、配弹、显隐及当前取舍 |
| [工程经验](engineering/engineering-lessons.md) | 重要故障与性能经验，含证据和适用边界 |
| [研究索引](research/README.md) | 弹舱动画等未采用/待接收半成品和继续条件 |
| [Mod机体兼容性续研](research/mod-aircraft-compatibility.md) | 剩余机体分组安装、原包缺项、已解决反馈和后续待办 |
| [归档与恢复](archive/README.md) | Git锚、原料、未提交快照和历史位置 |
| [本机资产索引](archive/local-assets.md) | Temp、备份、研究原料和归组文件的实际位置与使用条件 |
| [证据索引](../evidence/README.md) | 验收、安装、发布、研究回执检索；原始记录不回改 |
| [工具说明](../tools/README.md) | 本机辅助工具来源用途，工具本体不入Git |
| [既有检查说明](../tests/README.md) | 已有检查用途和条件，不构成自动测试授权 |
| [提示音来源](../dist/DualSP/Audio/README.md) | 本机音频原料来源、格式与恢复摘要，不是发布状态 |
| [历史核心与发布包](archive/README.md#历史核心与发布包) | 已归档DLL、包和源码备份的位置；必须原位保留的依赖 |

## 独立成果与外部资料

- 安装器用户原文：codex/delivery-installation:docs/安装程序交互.md；实现设计：同分支docs/交付与安装设计.md。实际路径见工作区索引；不另复制并列设计或接收代码。
- [Pages与R2说明](archive/delivery/Pages与R2发布更新说明.md)：既有站点/清单操作参考，作为后续交付重做输入，不授权操作线上服务。
- [开发版本旧记录](archive/development-history.md)：截至2.24.1的版本与研究编号对照，保留查证，不再追加，不向玩家展示。
- [授权与致谢原件](archive/legal/授权与致谢原文.txt)：原双语文本保留原字节，仅作来源；玩家阅读上方“作者与授权”。
- [管理界面设计原件](research/management-design/index.html)、[版本与更新页设计原件](research/management-design/version-updates.html)：保留交互、回传来源和用户批注；是历史设计证据，不代表全部界面已经实现。范围见[研究索引](research/README.md#界面设计原件)。
- 完整第三方许可证在payload/ue4ss/Mods/DualSP/licenses/和各vendor组件目录，保留原文；致谢不替代完整许可文本。
- 根目录原未提交兼容资料、旧中文说明及用户日志草稿保存在E:/AC7_MultiSP/Temp/preserved-local-20261023，另有快照。它们是待甄别原件，不能整套覆盖当前正文。

## 检索与维护

先定位主题，再在目标目录检索。日常默认排除evidence/references/vendor；查证据按链接、文件名或版本前缀定向读。历史全文按归档标签读取，不把旧“当前”“未构建”当今天状态。

新建、更名或合并文档时同时更新本索引和入链；先查已有唯一正文。合并后的旧全文移出日常树，恢复锚留归档索引，不保留第二份可编辑同义文档。
