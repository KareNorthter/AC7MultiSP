# AC7MultiSP

在《皇牌空战7》单人任务中使用1至3种特殊兵装，分别保留弹药和装填，提供选装/HUD、多SP模型及视角辅助功能。

## 当前状态

| 项目 | 状态与依据 |
|---|---|
| 正式发布版本 | **2.26.1**，作者于2026-10-25验收alpha3：CFA-44 EML持续掉帧已消除、舱门正常，F-2A普通飞行性能恢复，其他功能总体正常；要求合main并制作正式EXE，见[验收与授权](evidence/2.26.1-release-authorization.json)。 |
| 上一正式版本 | **2.26.0**；[原验收](evidence/2.26.0-main-acceptance-20261025.json)及EXE/ZIP保持原样。 |
| 正式源码入口 | main；主工作区E:/AC7_MultiSP。旧正式产物及验收记录不回改 |
| GitHub源码备份 | [KareNorthter/AC7MultiSP](https://github.com/KareNorthter/AC7MultiSP)，私有；主线历史、正式标签及研究恢复锚，范围见[仓库说明](docs/project/repository.md) |
| 发布历史 | [2.24.1发布回执](evidence/2.24.1-release.json)、[玩家主要更新](docs/user/更新日志.txt)；旧候选及验收记录各保留原阶段含义 |
| 现成正式EXE与通用ZIP | 作者最新四份中文已原字节接收并同步英文，正在完整制作2.26.1正式EXE、通用ZIP和Pages上传材料；旧2.26.0、alpha候选及暂停构建现场不覆盖。 |
| 实机验证边界 | 作者已验收alpha3的性能、舱门及其他功能，详见[性能验收](docs/research/runtime-performance.md#alpha3验收与正式2261)。这是用户反馈，不追认异常验签、故障注入或异机恢复等专项；XFA-33 B/Su-57 TGM公共舱继续暂缓。 |
| 主页与发布流程 | 已交付[仅R2网页包](Release/2.26.0/AC7MultiSP-Pages-R2-only.zip)，暂不提供蓝奏云，隐藏备用下载区，完整日志不变。R2已流式核对为安装器修订2（e878c0f5…），无需重编EXE；[网页核对](evidence/2.26.0-pages-r2-only.json)记录来源和待部署事实 |
| 当前正式源码 | VERSION为**2.26.1**、candidate=false；接收3d9ceb9、73366ce、b95afa2，运行逻辑沿用已验收alpha3，仅正式版本元数据与同源文字更新。 |
| 最近验收安装包 | **2.26.1 alpha3**，SHA256为647be5d4…；[原交付回执](evidence/2.26.1-alpha3-integration-delivery.json)不回改。正式2.26.1重新构建后单独记录，不将候选验收冒充新字节已安装。 |

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
