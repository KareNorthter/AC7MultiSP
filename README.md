# AC7MultiSP

在《皇牌空战7》单人任务中使用1至3种特殊兵装，分别保留弹药和装填，提供选装/HUD、多SP模型及视角辅助功能。

## 当前状态

| 项目 | 状态与依据 |
|---|---|
| 正式版本 | **2.27.0**；作者本次要求将2.27.0 beta7作为正式2.27.0，接收最新自改中文、同步英文并制作EXE、通用ZIP及网页包，见[本次发布准备](evidence/2.27.0-release-preparation-20261031.json)。此前[源码确认](evidence/2.27.0-source-confirmation-20261030.json)保留。本地制作不代表R2、Nexus或Pages已更新。 |
| 本轮候选阶段 | **2.27.0 beta7**运行功能已纳入本次正式版；原[候选EXE及回执](evidence/2.27.0-beta7-exe-delivery.json)保持。20ms目标仍待实测，不以正式编号代替性能验收。 |
| 上一正式版本 | **2.26.1**，已发布交付记录及旧包保持原样；2.26.0等更早版本见证据及归档入口。 |
| 正式源码入口 | main；主工作区E:/AC7_MultiSP。旧正式产物及验收记录不回改 |
| GitHub源码备份 | [KareNorthter/AC7MultiSP](https://github.com/KareNorthter/AC7MultiSP)，私有；主线历史、正式标签及研究恢复锚，范围见[仓库说明](docs/project/repository.md) |
| 发布历史 | [2.24.1发布回执](evidence/2.24.1-release.json)、[玩家主要更新](docs/user/更新日志.txt)；旧候选及验收记录各保留原阶段含义 |
| 现成正式EXE与通用ZIP | 已交付正式[2.27.0 EXE](Release/2.27.0/AC7MultiSP_Installer.exe)，SHA256为bae31857…，及[通用ZIP](Release/2.27.0/AC7MultiSP-2.27.0-Universal.zip)，f13994a8…；最新八份正文同源，作者中文保持原字节，见[本次交付](evidence/2.27.0-release-delivery-20261031.json)。2.26.1及旧候选原包保持。 |
| 实机验证边界 | 作者已确认2.26.2 beta7及本聊天交付的2.27.0 beta5功能正常；反馈不扩大为所有故障注入、异机恢复、异常边界或性能专项均已通过。beta6新增姿态天地显示及首次构建的X-02S EML常驻不在此前验收范围。 |
| Mod机体实机反馈 | 作者于2026-10-28确认X-02R A/B、J-20A/B/S及RH-172六个机体入口功能正常，见[用户验收](evidence/mod-aircraft-user-acceptance-20261028.json)。正式版本不变；此前ZSD组合冲突不据此记为已修复。 |
| 当前真实冲突测试环境 | 作者授权启用XFA-36A包、F-17V、Gleipnir，与B组X-02R/UI-4054混装，目录179条；已核对6个资源成员内容差异。作者确认F-17V、Gleipnir、X-02R A/B及UI-4054三SP正常，XFA-36激光收尾修复已编译，作者确认 OSL 正常。真实武器表冲突测试未触发错误，不能据此证明所有冲突可检测；作者已停止测试并恢复兼容表。见[作者反馈](evidence/real-conflict-user-feedback-20261028.json)和[检测/恢复流程](docs/engineering/aircraft-fault-testing.md#2026-10-28真实冲突测试组)。 |
| 主页与发布流程 | 已交付[2.27.0完整Pages包](Release/2.27.0/AC7MultiSP-Pages-homepage.zip)：最新七条更新完整提取，双语历史排除2.27.0，八份正文同源，沿用R2唯一下载并保留三组可信历史签名。材料待上传、远端未核；site/stable.json仍保留已发布2.26.1，作者部署后另行确认并同步，见[本次交付](evidence/2.27.0-release-delivery-20261031.json)及[发布流程](docs/project/release-workflow.md)。 |
| 当前主线源码 | VERSION为**2.27.0**，candidate=false；beta7运行源码不变，姿态v3、管理协议26、配置19保持。正式构建来源b9c3f1a；作者最新两份中文原字节接收，英文和生成文字已同步，完整构建、签名及包内字节核对通过。本次未推送、上传或操作游戏。 |
| 最近验收安装包 | **2.27.0 beta5**，作者于2026-10-31反馈“功能都没问题”，见[验收记录](evidence/2.27.0-beta5-user-acceptance-20261031.json)。原[交付回执](evidence/2.27.0-beta5-exe-delivery.json)及旧包保持，不把未编译的新源码视为已安装。 |

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
