# 文档索引

正式状态见[项目入口](../README.md)。本索引说明位置、内容和用途，不重复正文。默认只读当前任务相关项。

## 目录分工

| 目录 | 放什么 |
|---|---|
| user/ | 用户功能、操作、安装参考、授权与致谢 |
| project/ | 开发规范、分支与任务工作区登记 |
| engineering/ | 当前架构、开发依赖、挂载设计和踩坑结论 |
| releases/ | 唯一版本日志和本版摘要；发布包不放这里 |
| research/ | 待续研究索引和含用户反馈的设计原件 |
| archive/ | 历史恢复索引、旧交付资料；大文件归档位置见索引 |

本层只保留总索引。按用途放入现有目录，不再把交接、版本说明副本散放在本层。旧脚本中的文档路径待后续交付改造适配，迁移影响见[开发与恢复](engineering/development.md#文档迁移后的工具边界)。

## 使用与规范

| 文档 | 内容与用途 |
|---|---|
| [AGENTS](../AGENTS.md) | AI开工顺序、保护约束和授权边界 |
| [开发规范](project/workflow.md) | 工作、分支、并行、提交、版本、日志、接收与清理规则 |
| [工作区索引](project/workspaces.md) | 正式入口、活动成果、保留工作树与整理记录 |
| [中文用户说明](user/README_zh-CN.txt) | 当前功能、操作、默认值、适配与限制 |
| [English guide](user/README_en.txt) | 中文用户正文的等义英文版 |
| [安装参考](user/安装说明.txt) | 现有包结构及安装参考；交付流程待重做 |
| [本版摘要](releases/release.txt) | 最近正式版本的简短变化 |
| [版本更新说明](releases/版本更新说明.md) | 唯一完整日志，区分正式、候选和研究编号 |
| [授权与致谢原文](user/授权.txt) | 用户双语署名、致谢和第三方许可证位置 |

## 技术与研究

| 文档 | 内容与维护边界 |
|---|---|
| [架构](engineering/architecture.md) | 当前模块、跨语言接口、生命周期与保护机制，不写逐轮交付 |
| [开发与恢复](engineering/development.md) | 源码/原料路径、工具依赖、接收恢复和未解决事项 |
| [挂载设计](engineering/multi-sp-mount-design.md) | 挂点、净空、布局、配弹、显隐及当前取舍 |
| [工程经验](engineering/engineering-lessons.md) | 重要故障与性能经验，含证据和适用边界 |
| [研究索引](research/README.md) | 弹舱动画等未采用/待接收半成品和继续条件 |
| [归档与恢复](archive/README.md) | Git锚、原料、未提交快照和历史位置 |
| [证据索引](../evidence/README.md) | 验收、安装、发布、研究回执检索；原始记录不回改 |
| [工具说明](../tools/README.md) | 本机辅助工具来源用途，工具本体不入Git |
| [既有检查说明](../tests/README.md) | 已有检查用途和条件，不构成自动测试授权 |
| [提示音来源](../dist/DualSP/Audio/README.md) | 本机音频原料来源、格式与恢复摘要，不是发布状态 |
| [历史核心与发布包](archive/README.md#历史核心与发布包) | 已归档DLL、包和源码备份的位置；必须原位保留的依赖 |

## 独立成果与外部资料

- 安装器用户原文：codex/delivery-installation:docs/安装程序交互.md；实现设计：同分支docs/交付与安装设计.md。实际路径见工作区索引；不另复制并列设计或接收代码。
- [Pages与R2说明](archive/delivery/Pages与R2发布更新说明.md)：既有站点/清单操作参考，作为后续交付重做输入，不授权操作线上服务。
- [管理界面设计原件](research/management-design/index.html)、[版本与更新页设计原件](research/management-design/version-updates.html)：保留交互、回传来源和用户批注；是历史设计证据，不代表全部界面已经实现。范围见[研究索引](research/README.md#界面设计原件)。
- 完整第三方许可证在payload/ue4ss/Mods/DualSP/licenses/和各vendor组件目录，保留原文；致谢不替代完整许可文本。
- 根目录原未提交兼容资料、旧中文说明及用户日志草稿保存在E:/AC7_MultiSP/Temp/preserved-local-20261023，另有快照。它们是待甄别原件，不能整套覆盖当前正文。

## 检索与维护

先定位主题，再在目标目录检索。日常默认排除evidence/references/vendor；查证据按链接、文件名或版本前缀定向读。历史全文按归档标签读取，不把旧“当前”“未构建”当今天状态。

新建、更名或合并文档时同时更新本索引和入链；先查已有唯一正文。合并后的旧全文移出日常树，恢复锚留归档索引，不保留第二份可编辑同义文档。
