# AC7MultiSP

在《皇牌空战7》单人任务中使用1至3种特殊兵装，分别保留弹药和装填，提供选装/HUD、多SP模型及视角辅助功能。

## 当前状态

| 项目 | 状态与依据 |
|---|---|
| 正式功能版本 | **2.24.1**，发布锚v2.24.1，原功能/发布记录截至39141fd |
| 正式源码入口 | main；主工作区E:/AC7_MultiSP。文档整理不改变功能版本或既有发布包 |
| 发布历史 | [2.24.1发布回执](evidence/2.24.1-release.json)、[玩家主要更新](docs/user/更新日志.txt)；合入、构建、安装、验收、发布分别记录 |
| 独立待接收成果 | codex/delivery-installation / a99a404：安装器0.2.0预览，载荷2.24.1。代码未合main，新交互不沿用旧验收结论 |
| 已知本机记录 | 安装器0.1.0记录用户确认EXE安装2.24.1；0.2.0只生成新安装器。本轮未检查游戏目录，不把记录当成实时状态 |
| 整理边界 | 只改文档、规范和仓库组织；不改功能代码、不编译测试、不安装发布。交付流程由用户后续重做 |

研究曾使用2.25/2.26等编号，相关提交已进入后来的正式主线；这些不是比2.24.1更新的正式发布。判断新旧看Git祖先关系、正式标签和回执，不按目录名排序。

## 阅读入口

- **找资料：**[文档索引](docs/README.md)，按读者、主题、位置和内容检索。
- **开始工作：**[开发规范](docs/project/workflow.md)、[工作区索引](docs/project/workspaces.md)。
- **玩家阅读与审核：**[功能说明](docs/user/功能说明.txt)、[安装说明](docs/user/安装说明.txt)、[更新日志](docs/user/更新日志.txt)、[作者与授权](docs/user/作者与授权.txt)。四份独立草稿，等待用户审核；程序内文案尚未同步。
- **理解实现：**[架构](docs/engineering/architecture.md)、[开发与恢复](docs/engineering/development.md)、[挂载设计](docs/engineering/multi-sp-mount-design.md)。
- **避免重复踩坑：**[工程经验](docs/engineering/engineering-lessons.md)、[待续研究](docs/research/README.md)。
- **追溯历史：**[开发版本旧记录](docs/archive/development-history.md)、[证据索引](evidence/README.md)、[归档与恢复](docs/archive/README.md)。

支持边界和默认设置只在用户说明维护；技术限制只在对应设计/工程正文维护，不在各入口复制。
