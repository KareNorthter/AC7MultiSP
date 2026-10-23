# AC7MultiSP 工作入口

先看 Git 状态、所在路径、分支和HEAD，再读[项目入口](README.md)、[文档索引](docs/README.md)及[开发规范](docs/project/workflow.md)。正式状态只在README维护，不从旧聊天、目录名或研究编号推断最新版本。

## 必须遵守

1. 开发交流、代码注释及内部文档使用中文，通俗简洁。所有面向用户的交互界面和文字必须提供中文、英文两种，新增或修改时同步维护，具体范围见[双语规范](docs/project/workflow.md#用户交互与双语)。资料或环境受阻及时报告，不反复试错。
2. main是正式源码入口。一项任务一个责任线程、分支和工作树；从核对后的main建codex/分支，不切换或覆盖其他线程的工作树。命名、提交、接收、关闭见开发规范。
3. 默认不编译、不编写或运行测试、不安装、不操作游戏、不推送或上传；按用户当次授权执行，已获授权不重复询问。文档整理可做静态差异、链接及摘要核对。
4. 改代码同步唯一对应正文，不向多个入口复制交接。新文档登记到文档索引；未完成研究登记到[研究索引](docs/research/README.md)。文档可以多，但不能一事多源或只追加流水账。
   玩家正文只维护功能说明、安装说明、更新日志、作者与授权四个主题，每个主题的中英文各有唯一来源；EXE、ZIP、F6按[复用规则](docs/project/player-docs.md)共用。保留用户人工修改，不再另写长README、独立摘要或内置同义版本。
5. 并行线程只改各自登记范围；公共源码、版本、协议、配置迁移和同一正文须明确整合责任。研究线程不自行分配正式版本。任务登记见[工作区索引](docs/project/workspaces.md)。
6. 不自动联系或指挥其他用户聊天。子代理限授权范围，用户指定GPT-6.1 Sol/high。本管理任务负责文档、接收和整合，不自行启动功能研究。
7. 保护旧核心dist/DualSP/bin/DualSP.dll，SHA256 D7D7055CA7909021EEACA15E63FF5FF253D2EB7E60A3FE3B79DCEEAB231DE339，不得覆盖。不能用重建掩盖冻结差异或改历史回执。
8. 不提交个人配置/状态、存档、游戏资源、音频、第三方PAK、工具、转储、发布ZIP或凭证。Lua唯一源dist/DualSP/Scripts/；原料、冻结产物和历史证据按依赖保护。
9. pwsh7优先，复杂处理用Python；新文件UTF-8，Windows启动脚本保所需BOM。临时Temp/、备份backup/且忽略；无Git改前备份。清理送回收站，不永久删除。
10. 未采用不等于无用：弹舱动画、旧布局、更新器等半成品保留提交锚、未提交差异、原料位置和继续条件，不能因未合main就删除。

## 按需阅读

- [架构](docs/engineering/architecture.md)、[开发与恢复](docs/engineering/development.md)。
- 挂载先读[设计](docs/engineering/multi-sp-mount-design.md)：布局容量、实际配弹、运行显隐分别处理，不为凑外观反改配弹规则。
- [工程经验](docs/engineering/engineering-lessons.md)、[待续研究](docs/research/README.md)、[历史恢复](docs/archive/README.md)、[证据索引](evidence/README.md)。
- 默认搜索通过.ignore排除evidence/references/vendor；按索引定向读历史。

## Nexus使用约定

免费账户，用户2026-10-21曾在内置浏览器登录；先检查并尝试复用，失效再请登录。不把跨会话共享、Vortex/API授权视为已验证。下载、上传和发布按当次授权执行，不保存密码、Cookie、令牌或API密钥。
