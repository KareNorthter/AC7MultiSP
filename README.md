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
| 当前EXE交付候选 | **2.24.3 DPI修订**：安装器增加系统DPI感知，完成本机125%中英文页面离线预览；[DPI回执](evidence/2.24.3-dpi-exe-delivery.json)记录修订EXE及复用载荷。产物保留在Release/2.24.3-dpi；用户随后要求无需继续编译，停止后续构建。原[2.24.3交付回执](evidence/2.24.3-exe-delivery.json)和成品保留，仍待人工使用验证 |
| 未构建源码改动 | F6更新栏新增默认开启的“启动游戏时检查更新”，随设置保存，保留配置19和个人方案；管理协议22，未编译或运行测试，现有EXE不包含此新增选项 |
| 已知本机记录 | 安装器0.1.0记录用户确认EXE安装2.24.1；0.2.0只生成新安装器。本轮未检查游戏目录，不把记录当成实时状态 |
| 本轮边界 | DPI修订EXE在用户要求停止编译前已生成；仅运行不联网、不搜索游戏的离线页面预览，未进入正常安装模式，未运行测试模块、安装、操作游戏或上传。旧产物与回执保留，不再继续构建 |

研究曾使用2.25/2.26等编号，相关提交已进入后来的正式主线；这些不是比2.24.1更新的正式发布。判断新旧看Git祖先关系、正式标签和回执，不按目录名排序。

## 阅读入口

- **找资料：**[文档索引](docs/README.md)，按读者、主题、位置和内容检索。
- **开始工作：**[开发规范](docs/project/workflow.md)、[工作区索引](docs/project/workspaces.md)。
- **玩家阅读与审核：**[功能说明](docs/user/功能说明.txt)、[安装说明](docs/user/安装说明.txt)、[更新日志](docs/user/更新日志.txt)、[作者与授权](docs/user/作者与授权.txt)。四主题均有英文稿，具体路径见文档索引；EXE、ZIP和F6按[复用约定](docs/project/player-docs.md)共用。作者最新免费及禁止二次变卖条款已同步。
- **理解实现：**[架构](docs/engineering/architecture.md)、[开发与恢复](docs/engineering/development.md)、[挂载设计](docs/engineering/multi-sp-mount-design.md)。
- **避免重复踩坑：**[工程经验](docs/engineering/engineering-lessons.md)、[待续研究](docs/research/README.md)。
- **追溯历史：**[开发版本旧记录](docs/archive/development-history.md)、[证据索引](evidence/README.md)、[归档与恢复](docs/archive/README.md)。
- **找本机原料与现场：**[资产索引](docs/archive/local-assets.md)，逐项清单同时保留原路径和整理后的实际路径。

支持边界和默认设置只在用户说明维护；技术限制只在对应设计/工程正文维护，不在各入口复制。
