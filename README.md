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
| 当前EXE交付候选 | **2.25.1**：放大F6水印、统一常显反馈卡片，修复两处Mod机体候选误筛选，并接收964507d快速任务/Logo修复；完整重编核心/UI并生成双语[EXE安装器](Release/2.25.1/AC7MultiSP_Installer.exe)。[交付回执](evidence/2.25.1-exe-delivery.json)记录来源、内嵌载荷和摘要；未安装或实机验收，非正式发布 |
| 本轮接收范围 | 在2.25.0基础上修正F6布局、Patch机体许可分类，以及主菜单出击准备和菜单实例校验。真实DLC继续校验许可；快速入口当前单种SP，M13保留任务指定SP，不保证全部CG覆盖 |
| 已知本机记录 | 用户已安装2.25.0并反馈快速任务失败，[失败记录](evidence/quick-mission-failure-20261024.json)保留当时日志。2.25.1未部署；本轮只读既有日志和机体资料，不能把源码修复或编译当作游戏验收 |
| 本轮边界 | 用户2026-10-24授权修改、接收快速任务新成果和编译EXE；完整构建、封装回读及文件核对已完成。未运行测试模块、安装器、离线UI预览或游戏，未安装、关闭游戏或上传；旧产物/回执和主目录原有两项未提交改动保留 |

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
