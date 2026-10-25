# AC7MultiSP

在《皇牌空战7》单人任务中使用1至3种特殊兵装，分别保留弹药和装填，提供选装/HUD、多SP模型及视角辅助功能。

## 当前状态

| 项目 | 状态与依据 |
|---|---|
| 正式发布版本 | **2.26.0**，作者于2026-10-25确认安装包和Mod功能正常，并要求接收当前main及同步GitHub；[验收与文档接收](evidence/2.26.0-main-acceptance-20261025.json)记录本次事实。已交付EXE/ZIP不重建或改写 |
| 本轮统一自测候选 | **2.26.1 alpha3**；基线main/19b1528，接收原生英文标签3d9ceb9、性能73366ce、兼容流程及CFA机腹舱b95afa2。已完整构建并交付EXE至`E:/AC7_MultiSP/Release/2.26.1-alpha3/AC7MultiSP_Installer.exe`，SHA256为647be5d4…；来源f047b70，详情见[交付回执](evidence/2.26.1-alpha3-integration-delivery.json)。尚未安装或游戏验收，正式版仍为2.26.0；覆盖步骤见[统一测试](docs/research/runtime-performance.md#统一游戏对照alpha3集成候选) |
| 上一正式版本 | **2.25.2**，原[发布准备与核对](evidence/2.25.2-publication-preparation.json)及旧产物保持原样 |
| 正式源码入口 | main；主工作区E:/AC7_MultiSP。旧正式产物及验收记录不回改 |
| GitHub源码备份 | [KareNorthter/AC7MultiSP](https://github.com/KareNorthter/AC7MultiSP)，私有；主线历史、正式标签及研究恢复锚，范围见[仓库说明](docs/project/repository.md) |
| 发布历史 | [2.24.1发布回执](evidence/2.24.1-release.json)、[玩家主要更新](docs/user/更新日志.txt)；旧候选及验收记录各保留原阶段含义 |
| 现成正式EXE与通用ZIP | 作者已将安装器修订2等文件移到Release/2.26.0根目录；[当前EXE](Release/2.26.0/AC7MultiSP_Installer.exe)版本2.26.0，e878c0f5…；[原修订回执](evidence/2.26.0-installer-homepage-revision2.json)保留交付时路径，冻结原件及旧记录不改写 |
| 实机验证边界 | 作者已确认2.26.0安装包及Mod功能正常；这是用户总体反馈，不追认未逐项核实的异常验签、异机恢复或性能测量。F6状态发布性能、F-2A掉帧及XFA-33 B/Su-57 TGM公共舱待办继续保留 |
| 主页与发布流程 | 已交付[仅R2网页包](Release/2.26.0/AC7MultiSP-Pages-R2-only.zip)，暂不提供蓝奏云，隐藏备用下载区，完整日志不变。R2已流式核对为安装器修订2（e878c0f5…），无需重编EXE；[网页核对](evidence/2.26.0-pages-r2-only.json)记录来源和待部署事实 |
| 当前正式源码 | VERSION为**2.26.0**、candidate=false；完整运行构建来源38f1255，安装器修订2来源2923ed6。已原样接收作者最新中英文TXT并同步供后续构建使用的生成文字；按作者要求不重编/重包，现成EXE、ZIP及网页包正文仍是各自冻结快照 |
| 最近验收安装包 | 2.26.0安装器修订2，SHA256为e878c0f5…；作者已确认安装包与Mod功能正常，[原修订回执](evidence/2.26.0-installer-homepage-revision2.json)保留制作时路径与核对事实。本次TXT细节修订不追改现成包 |

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
