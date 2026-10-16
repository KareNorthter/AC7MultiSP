# 开发与恢复

在 E:/AC7_MultiSP 的 main 开发；授权、版本与待办以 [AGENTS](../AGENTS.md) 为准。本轮不执行以下构建/安装命令。

## 有效入口

| 工作 | 工具 |
|---|---|
| 完整候选构建 | src/research/build_su30sm_candidate.py，包含固定挂载宏，输出全新 Temp 子目录 |
| 本机相邻版本升级 | src/research/install_su30sm_three_sp.py，默认预检，显式 --apply 安装 |
| 1.1.0 打包 | src/package-release.py，绑定正式来源摘要/回执，已有 ZIP 不覆盖 |

构建示例：`python -X utf8 src/research/build_su30sm_candidate.py --tools-root E:/AC7_MultiSP/tools --output-dir Temp/build-next`。语法/导出/文件核对不等于实玩。

普通 build.cmd/build-core-portable.cmd 缺少正式挂载条件，旧 install-resupply-candidate.py --experience 选择旧核心，都不是当前完整交付入口。
工具保留研究名称；单机型安装器也仍被当前安装器导入，不能按名字直接删文件。

## 来源与交付

Lua 唯一源 dist/DualSP/Scripts，共十个脚本；核心 dist/DualSP/cores/1.1.0/DualSP.dll，UI dist/DualSP/bin/DualSPUI.dll；旧 bin/DualSP.dll 仅作保护基线。
加载器/许可从 payload 显式取用；[工具清单](../tools/README.md)、音频、运行库、ZIP 是本地依赖，不入 Git。

打包器读取[构建回执](../evidence/1.1.0-build.json)并核对来源、DLL、签名与 ZIP 成员。[现有包](../Release/README.md)保持[发布回执](../evidence/1.1.0-release.json)的原字节，文档编辑不会自动重打包。
安装器仍读取 1.0.2、固定挂载候选.2、三SP候选.11 等回执；保留路径和字节。部署任务获授权后须重新核对实际进程/安装版本，再完整备份、暂存、校验；不从旧 PID 或发布号推断现场。

## 待处理的工程问题

- 核心复用只核对两个文件，来源摘要缺部分头文件/第三方源/编译条件，不能证明完整依赖未变；功能变动后在有授权时完整构建。
- 正式/研究工具入口待统一；旧 tests 主要对应双SP，不能宣称覆盖1.1.0，未经要求不新增或运行。
- 核心、主调度、页面适配器职责集中，后续渐进拆分，本轮不改功能。

## 未合入成果和恢复

`codex/pending-ui-polish` 保存 f800518、07c81c0、7e2fe4a 三个提交；逐项评估标题、弹药高亮和中央提示，不能整枝覆盖主线。
原根目录36个未提交文件存于提交 `0637abc` / 标签 `archive/20261016/three-sp-research`。其余旧分支有同前缀恢复标签；完整映射在[仓库整理回执](../evidence/repository-consolidation-20261016.json)。
五个旧工作区保留本地产物并锁定，只有 AGENTS 有封存提示；不在那里继续开发。备份位于 backup/maintenance-20261016，含已核验 bundle。

历史说明已从当前目录移除，完整整理前快照为 `archive/20261016/docs-before-prune`。需要某个旧结论时，用 `git show <标签>:<当时路径>` 定向读取；不要恢复整套历史文档。
恢复代码应从归档标签新建 codex/ 分支，不重置 main。通常无需查历史，先读当前架构和相关代码。

## 文档分工

AGENTS 只维护状态/规则；architecture 维护模块和实现约束；本页维护工具/恢复；两个 TXT 供用户及打包器。
不再独立维护状态页、管理专题、挂载专题或整理流水账。必要回执按[证据索引](../evidence/README.md)读取，禁止每次改动再复制一套交接说明。
