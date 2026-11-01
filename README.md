# AC7MultiSP

在《皇牌空战7》单人任务中使用1至3种特殊兵装，分别保留弹药和装填，提供选装/HUD、多SP模型及视角辅助功能。

## 源码公开与使用许可 / Source availability and license

公开作者有权公开的代码和其他原创成果，允许永久免费使用、学习、修改和非商业免费分享；禁止未经作者事先书面许可的商业使用、出售或变卖。分享时保留作者署名、来源及许可，修改版须注明改动。完整条款及与现有玩家说明的关系见[源码使用许可](LICENSE.txt)；第三方内容继续遵守各自许可。这是带非商业限制的源码公开项目。

The author's code and other original works that the author is entitled to publish are available for free use, study, modification and free noncommercial sharing. Commercial use, sale or resale requires the author's prior written permission. Retain attribution, source and license when sharing, and identify modified versions. See the [source license](LICENSE.txt) for the full terms and its relationship to existing player notices. Third-party content retains its own licenses. This project is source-available with a noncommercial restriction.

## 当前状态

| 项目 | 状态与依据 |
|---|---|
| 正式版本 | **2.27.0，作者已确认发布成功**（2026-11-01），见[发布确认](evidence/2.27.0-publication-confirmation-20261101.json)。beta7运行源码转正式版，使用作者最新中文及同步英文，见[发布准备](evidence/2.27.0-release-preparation-20261031.json)和[交付](evidence/2.27.0-release-delivery-20261031.json)。本次依作者反馈登记，未独立下载或部署核验。 |
| 本轮候选阶段 | **2.27.0 beta7**运行功能已纳入本次正式版；原[候选EXE及回执](evidence/2.27.0-beta7-exe-delivery.json)保持。20ms目标仍待实测，不以正式编号代替性能验收。 |
| 上一正式版本 | **2.26.1**，已发布交付记录及旧包保持原样；2.26.0等更早版本见证据及归档入口。 |
| 正式源码入口 | 公开仓库main；清理后的公开工作区E:/AC7_MultiSP-public。E:/AC7_MultiSP及其旧工作树保留私有历史、原料和正式产物，后续公开接收须按[仓库说明](docs/project/repository.md)操作 |
| GitHub源码仓库 | [KareNorthter/AC7MultiSP](https://github.com/KareNorthter/AC7MultiSP)，公开；保留脱敏后的主线历史、正式标签及研究恢复锚。原仓库另名私有保存；[源码许可](LICENSE.txt)及[仓库说明](docs/project/repository.md)分别维护授权和同步边界 |
| 发布历史 | [2.24.1发布回执](evidence/2.24.1-release.json)、[玩家主要更新](docs/user/更新日志.txt)；旧候选及验收记录各保留原阶段含义 |
| 现成正式EXE与通用ZIP | 已交付正式[2.27.0 EXE](Release/2.27.0/AC7MultiSP_Installer.exe)，SHA256为bae31857…，及[通用ZIP](Release/2.27.0/AC7MultiSP-2.27.0-Universal.zip)，f13994a8…；最新八份正文同源，作者中文保持原字节，见[本次交付](evidence/2.27.0-release-delivery-20261031.json)。2.26.1及旧候选原包保持。 |
| 实机验证边界 | 作者已确认2.26.2 beta7及本聊天交付的2.27.0 beta5功能正常；反馈不扩大为所有故障注入、异机恢复、异常边界或性能专项均已通过。beta6新增姿态天地显示及首次构建的X-02S EML常驻不在此前验收范围。 |
| Mod机体实机反馈 | 作者于2026-10-28确认X-02R A/B、J-20A/B/S及RH-172六个机体入口功能正常，见[用户验收](evidence/mod-aircraft-user-acceptance-20261028.json)。正式版本不变；此前ZSD组合冲突不据此记为已修复。 |
| 当前真实冲突测试环境 | 作者授权启用XFA-36A包、F-17V、Gleipnir，与B组X-02R/UI-4054混装，目录179条；已核对6个资源成员内容差异。作者确认F-17V、Gleipnir、X-02R A/B及UI-4054三SP正常，XFA-36激光收尾修复已编译，作者确认 OSL 正常。真实武器表冲突测试未触发错误，不能据此证明所有冲突可检测；作者已停止测试并恢复兼容表。见[作者反馈](evidence/real-conflict-user-feedback-20261028.json)和[检测/恢复流程](docs/engineering/aircraft-fault-testing.md#2026-10-28真实冲突测试组)。 |
| 主页与发布流程 | 作者已确认发布完成；site/stable.json已原字节同步[2.27.0完整Pages包](Release/2.27.0/AC7MultiSP-Pages-homepage.zip)的清单，保留R2唯一入口及历史签名。[原交付](evidence/2.27.0-release-delivery-20261031.json)保留制作时待上传、远端未核事实；后续作者反馈见[发布确认](evidence/2.27.0-publication-confirmation-20261101.json)，本轮未独立联网核验部署。 |
| 当前主线源码 | VERSION为**2.27.0**，candidate=false；beta7运行源码不变，姿态v3、管理协议26、配置19保持。正式构建来源b9c3f1a，正文及签名核对记录保持；本轮仅同步已发布清单、作者发布确认及GitHub源码，不重编、不安装或操作游戏。 |
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
