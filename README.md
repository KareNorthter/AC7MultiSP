# AC7MultiSP

在《皇牌空战7》单人任务中使用1至3种特殊兵装，分别保留弹药和装填，提供选装/HUD、多SP模型及视角辅助功能。

## 当前状态

| 项目 | 状态与依据 |
|---|---|
| 正式版本目标 | **2.26.0**，作者确认alpha5“都正常了”并明确要求正式发布；版本与中英文日志已整理，正式EXE/ZIP尚未制作、未上传，见[验收与版本确认](evidence/2.26.0-release-authorization.json) |
| 上一正式版本 | **2.25.2**，原[发布准备与核对](evidence/2.25.2-publication-preparation.json)及旧产物保持原样 |
| 正式源码入口 | main；主工作区E:/AC7_MultiSP。旧正式产物及验收记录不回改 |
| GitHub源码备份 | [KareNorthter/AC7MultiSP](https://github.com/KareNorthter/AC7MultiSP)，私有；主线历史、正式标签及研究恢复锚，范围见[仓库说明](docs/project/repository.md) |
| 发布历史 | [2.24.1发布回执](evidence/2.24.1-release.json)、[玩家主要更新](docs/user/更新日志.txt)；旧候选及验收记录各保留原阶段含义 |
| 现成正式EXE与通用ZIP | [2.25.2 EXE](Release/2.25.2/AC7MultiSP_Installer.exe)及内嵌同字节通用ZIP；[原交付回执](evidence/2.25.2-exe-delivery.json)保留候选构建来源，不能改名冒充2.26.0 |
| 实机验证边界 | 作者确认alpha4基本通过，随后确认alpha5均正常；记录为用户实测反馈，不追认未逐项核实的异常验签、异机恢复或性能测量。F6状态发布性能、F-2A掉帧及XFA-33 B/Su-57 TGM公共舱待办继续保留 |
| 主页与发布流程 | 已接收双语主页，统一[新版发布流程](docs/project/release-workflow.md)；工具准备EXE、Nexus通用ZIP、完整Pages包和摘要，R2/蓝奏云/Nexus/Pages均由作者手动上传。生成与实际部署分别记录 |
| 当前正式源码 | VERSION为**2.26.0**、candidate=false；汇总2.25系列至alpha5主要变化，玩家日志移除alpha明细，旧2.25.X及更早历史保留。生成文字已同步，尚未构建正式包 |
| 最近验收安装包 | [2.25.3 alpha5 EXE](Release/2.25.3-alpha5/AC7MultiSP_Installer.exe)，来源db0d2c9，作者本次确认正常；[原交付回执](evidence/2.25.3-alpha5-exe-delivery.json)记录制作时的核对事实，旧包和回执不改作正式版本 |

旧研究曾使用2.25/2.26等编号，不代表当时正式发布；2.25.2以本次来源锚和用户发布确认为准。原构建元数据中的candidate仍保留当时事实，后续构建由整合任务统一处理。判断新旧看Git祖先关系、正式标签和回执，不按目录名排序。

## 阅读入口

- **找资料：**[文档索引](docs/README.md)，按读者、主题、位置和内容检索。
- **开始工作：**[开发规范](docs/project/workflow.md)、[工作区索引](docs/project/workspaces.md)。
- **玩家阅读与审核：**[功能说明](docs/user/功能说明.txt)、[安装说明](docs/user/安装说明.txt)、[更新日志](docs/user/更新日志.txt)、[作者与授权](docs/user/作者与授权.txt)。四主题均有英文稿，具体路径见文档索引；EXE、ZIP和F6按[复用约定](docs/project/player-docs.md)共用。作者最新免费及禁止二次变卖条款已同步。
- **理解实现：**[架构](docs/engineering/architecture.md)、[开发与恢复](docs/engineering/development.md)、[挂载设计](docs/engineering/multi-sp-mount-design.md)。
- **避免重复踩坑：**[工程经验](docs/engineering/engineering-lessons.md)、[待续研究](docs/research/README.md)。
- **追溯历史：**[开发版本旧记录](docs/archive/development-history.md)、[证据索引](evidence/README.md)、[归档与恢复](docs/archive/README.md)。
- **找本机原料与现场：**[资产索引](docs/archive/local-assets.md)，逐项清单同时保留原路径和整理后的实际路径。

支持边界和默认设置只在用户说明维护；技术限制只在对应设计/工程正文维护，不在各入口复制。
