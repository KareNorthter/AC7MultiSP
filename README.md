# AC7MultiSP

在《皇牌空战7》单人任务中使用1至3种特殊兵装，分别保留弹药和装填，提供选装/HUD、多SP模型及视角辅助功能。

## 当前状态

| 项目 | 状态与依据 |
|---|---|
| 正式版本 | **2.27.0**；作者确认beta7测试无误，并明确将已接收改动及最新玩家正文合入main作为2.27.0，见[源码确认](evidence/2.27.0-source-confirmation-20261030.json)。本轮仅同步源码到GitHub，暂不交付安装包或网页，不代表R2、Nexus及Pages已更新。 |
| 本轮候选阶段 | **2.27.0 beta2**，增加默认关闭的SPEED下方第三人称姿态指示器，包含PW HUD排版微调及视角修订；已制作[候选EXE](evidence/2.27.0-beta2-exe-delivery.json)，待作者实机验证。旧beta1包与回执保留；同号正式版晚于beta候选。 |
| 上一正式版本 | **2.26.1**，已发布交付记录及旧包保持原样；2.26.0等更早版本见证据及归档入口。 |
| 正式源码入口 | main；主工作区E:/AC7_MultiSP。旧正式产物及验收记录不回改 |
| GitHub源码备份 | [KareNorthter/AC7MultiSP](https://github.com/KareNorthter/AC7MultiSP)，私有；主线历史、正式标签及研究恢复锚，范围见[仓库说明](docs/project/repository.md) |
| 发布历史 | [2.24.1发布回执](evidence/2.24.1-release.json)、[玩家主要更新](docs/user/更新日志.txt)；旧候选及验收记录各保留原阶段含义 |
| 现成正式EXE与通用ZIP | 已交付[2.26.1 EXE](Release/2.26.1/AC7MultiSP_Installer.exe)，SHA256为30745081…，及[通用ZIP](Release/2.26.1/AC7MultiSP-2.26.1-Universal.zip)，79b95836…；两者含作者最新四份中文原字节和完整重译英文，见[交付回执](evidence/2.26.1-exe-delivery.json)。旧正式及候选产物保留。 |
| 实机验证边界 | 作者已确认2.26.2 beta7测试无误；2.27.0沿用其运行逻辑并接收最新正文及正式版本标识。不追认故障注入、异机恢复或所有边界已完成专项测试。 |
| Mod机体实机反馈 | 作者于2026-10-28确认X-02R A/B、J-20A/B/S及RH-172六个机体入口功能正常，见[用户验收](evidence/mod-aircraft-user-acceptance-20261028.json)。正式版本不变；此前ZSD组合冲突不据此记为已修复。 |
| 当前真实冲突测试环境 | 作者授权启用XFA-36A包、F-17V、Gleipnir，与B组X-02R/UI-4054混装，目录179条；已核对6个资源成员内容差异。作者确认F-17V、Gleipnir、X-02R A/B及UI-4054三SP正常，XFA-36激光收尾修复已编译，作者确认 OSL 正常。真实武器表冲突测试未触发错误，不能据此证明所有冲突可检测；作者已停止测试并恢复兼容表。见[作者反馈](evidence/real-conflict-user-feedback-20261028.json)和[检测/恢复流程](docs/engineering/aircraft-fault-testing.md#2026-10-28真实冲突测试组)。 |
| 主页与发布流程 | 已交付[2.26.1完整Pages包](Release/2.26.1/AC7MultiSP-Pages-homepage.zip)，顶部只显示2.26.1两条更新，底部历史由最早至2.26.0（含2.25.X），下载区仅描述R2并保留历史签名。作者已确认完成发布，site/stable.json已按交付材料同步2.26.1；[原网页回执](evidence/2.26.1-pages-delivery.json)保留制作时远端未核事实，[发布确认](evidence/2.26.1-publication-confirmation-20261025.json)记录作者后续反馈。本轮未独立下载或部署核验。 |
| 当前主线源码 | VERSION为**2.27.0 beta2**，candidate=true；[整合回执](evidence/2.27.0-beta2-source-integration.json)记录姿态图、HUD及视角修订。4d2c053与a1868a0已纳入本次完整构建，实际布局、垂直环绕和姿态刷新待实机验证；正式2.27.0来源及确认记录保留。 |
| 最近验收安装包 | **2.26.2 beta7**，作者于2026-10-30反馈测试无误；[交付记录](evidence/2.26.2-beta7-exe-delivery.json)保留原包身份，不追认2.27.0新字节已安装。 |

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
