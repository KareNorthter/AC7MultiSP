# AC7MultiSP

在《皇牌空战7》单人任务中使用1至3种特殊兵装，分别保留弹药和装填，提供选装/HUD、多SP模型及视角辅助功能。

## 当前状态

| 项目 | 状态与依据 |
|---|---|
| 正式功能版本 | **2.24.1**，发布锚v2.24.1，原功能/发布记录截至39141fd |
| 正式源码入口 | main；主工作区E:/AC7_MultiSP。旧正式产物及验收记录不回改 |
| GitHub源码备份 | [KareNorthter/AC7MultiSP](https://github.com/KareNorthter/AC7MultiSP)，私有；主线历史、正式标签及研究恢复锚，范围见[仓库说明](docs/project/repository.md) |
| 发布历史 | [2.24.1发布回执](evidence/2.24.1-release.json)、[玩家主要更新](docs/user/更新日志.txt)；合入、构建、安装、验收、发布分别记录 |
| 本地交付候选 | **2.24.2 / 原安装器0.3.0**：[交付回执](evidence/2.24.2-delivery.json)记录来源和核对范围；用户于2026-10-24反馈已安装且基本功能无误，未据此扩大为全部升级、卸载或异常分支验收 |
| 当前EXE交付候选 | **2.25.1**：[EXE安装器](Release/2.25.1/AC7MultiSP_Installer.exe)及[交付回执](evidence/2.25.1-exe-delivery.json)保留。用户确认安装后可快速进入任务，但本项目三SP功能失效；已定位到开场辅助异常导致初始化中止，尚未完成整体游戏验收 |
| 本轮接收范围 | 在2.25.0基础上修正F6布局、Patch机体许可分类，以及主菜单出击准备和菜单实例校验。真实DLC继续校验许可；快速入口当前单种SP，M13保留任务指定SP，不保证全部CG覆盖 |
| 待下次更新的源码 | 片头/开场跳过失败不再冒泡阻断三SP初始化，快速任务卡片已移至调试页倒数第二项、日志上方；游戏内效果待新包验证 |
| 已知本机记录 | 用户已安装2.25.1并确认快速进入任务成功；本轮只读既有日志，未替换游戏文件或操作游戏。三SP中止的[日志依据](evidence/quick-mission-trisp-failure-20261024.json)与[源码修复](docs/engineering/quick-mission-launch.md#2251快速出击成功后三sp失效)分开记录 |
| 本轮边界 | 本轮仅保存辅助错误隔离和快速卡片顺序修复，更新中英文说明并静态核对；尚未编译、运行测试、安装或上传，随下次更新打包。当前EXE不包含本轮源码修改；旧产物/回执及主目录原有两项未提交改动保留 |

旧研究曾使用2.25/2.26等编号，不代表当时正式发布；本轮2.25.1为新整合的本地验收候选，须以本轮来源锚和回执区分。判断新旧看Git祖先关系、正式标签和回执，不按目录名排序。

## 阅读入口

- **找资料：**[文档索引](docs/README.md)，按读者、主题、位置和内容检索。
- **开始工作：**[开发规范](docs/project/workflow.md)、[工作区索引](docs/project/workspaces.md)。
- **玩家阅读与审核：**[功能说明](docs/user/功能说明.txt)、[安装说明](docs/user/安装说明.txt)、[更新日志](docs/user/更新日志.txt)、[作者与授权](docs/user/作者与授权.txt)。四主题均有英文稿，具体路径见文档索引；EXE、ZIP和F6按[复用约定](docs/project/player-docs.md)共用。作者最新免费及禁止二次变卖条款已同步。
- **理解实现：**[架构](docs/engineering/architecture.md)、[开发与恢复](docs/engineering/development.md)、[挂载设计](docs/engineering/multi-sp-mount-design.md)。
- **避免重复踩坑：**[工程经验](docs/engineering/engineering-lessons.md)、[待续研究](docs/research/README.md)。
- **追溯历史：**[开发版本旧记录](docs/archive/development-history.md)、[证据索引](evidence/README.md)、[归档与恢复](docs/archive/README.md)。
- **找本机原料与现场：**[资产索引](docs/archive/local-assets.md)，逐项清单同时保留原路径和整理后的实际路径。

支持边界和默认设置只在用户说明维护；技术限制只在对应设计/工程正文维护，不在各入口复制。
