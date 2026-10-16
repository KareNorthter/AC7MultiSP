# 开发与恢复

主工作区 E:/AC7_MultiSP，开发分支从 main 创建；授权、版本与待办以 [AGENTS](../AGENTS.md) 为准。

## 有效入口

| 工作 | 工具 |
|---|---|
| 完整构建 | src/build.py，成套构建核心/UI，输出全新 Temp 子目录 |
| 自动检查 | tests/run.py --build-dir 指向上述目录，失败不会写通过回执 |
| 本机升级 | src/install.py，默认预检，显式 --apply 安装，不启动游戏 |
| 本地打包 | src/package-release.py，使用当前产品清单与完整构建回执，已有 ZIP 不覆盖 |

获授权后，在根目录依次执行：

```powershell
python -X utf8 src/build.py --output-dir Temp/build-2.0-verified
python -X utf8 tests/run.py --build-dir Temp/build-2.0-verified
python -X utf8 src/install.py --build-dir Temp/build-2.0-verified
python -X utf8 src/install.py --build-dir Temp/build-2.0-verified --apply
```

构建输出必须是全新目录；默认工具根为 tools，可指定 --tools-root。每次完整编译，不再接受旧 --reuse-core-build。自动检查不等于实玩。

build.cmd/build-core-portable.cmd/build-ui.cmd 和旧研究构建入口只转发 build.py，全部构建成套产物，不直接覆盖 dist。旧三SP安装入口只转发 install.py；单机型安装入口明确拒绝执行，原实现可从 Git 恢复。其余历史研究/补给安装器不用于当前版本。

## 来源与交付

产品名称与 Lua 清单在 src/product.json；版本在 VERSION。Lua 唯一源 dist/DualSP/Scripts，共十个脚本；当前核心 dist/DualSP/cores/2.0/DualSP.dll，UI dist/DualSP/bin/DualSPUI.dll；旧 bin/DualSP.dll 仅作保护基线。
加载器/许可从 payload 显式取用；[工具清单](../tools/README.md)、音频、运行库、ZIP 是本地依赖，不入 Git。

构建回执包含 src 产品 C++/头文件、vendor 全树、所有 Lua、正式工具与参数，以及编译命令和工具链全树指纹；构建前后核对一致。安装/打包再次核对整个来源集合，不能只比对清单中少数文件。旧格式回执不接受。

本机升级基线由 product.json 的 upgrade_receipt 指定，本轮为三SP .11；也可显式 --base-receipt 指定 evidence 中完整 runtime_files 回执。要求现有 schema 2 配置，不自动执行历史迁移，不读取无关的1.0.2/.2回执。
安装必须有匹配构建和测试源码的通过回执。退出游戏后整套暂存、核对、备份模组和存档，再交换目录；保留个人配置、其他模组及外部 PAK。第二步交换失败会尝试归还原目录，不启动游戏。

当前构建、测试凭据保存 evidence/2.0-build.json、2.0-tests.json；安装记录为 evidence/2.0-installed.json，已完整备份并安装，未重新启动游戏。验证通过后，当前构建 DLL 原样保存到上述 dist 路径，回执从构建目录原样保留。后续打包读取同版本构建回执，核对来源、DLL、签名与 ZIP 成员；本轮未制作2.0 ZIP。[现有1.1.0包](../Release/README.md)保持原字节。

## 待处理的工程问题

- tests/run.py 运行来源拒绝检查、当前三SP纯规则、冷却、指针和缓存恢复及DLL加载核对；旧双SP控制器/HUD/选装模拟不适用，未纳入当前通过范围。
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
