# 归档与恢复

这里维护恢复位置和覆盖范围；关键结论见[工程经验](../engineering-lessons.md)，未完成方案见[研究索引](../research/README.md)，当前工作入口见[工作区索引](../workspaces.md)。不把历史全文再次复制成另一套可编辑正文。

## 文本与源码恢复锚

| 恢复锚 | 内容与用途 |
|---|---|
| archive/20261023/pre-governance-main / 39141fd | 本次整理前正式2.24.1全部源码、文档和证据，含三个before-2.16全文 |
| archive/20261023/pre-governance-root / 0b75b21 | 原根目录已提交2.21.1基线；未提交内容另见下表 |
| archive/project-before-2.16-release / 92690a7 | 更早整理前完整主树 |
| archive/20261016/*、archive/20261017/* | 早期研究tip与文档归档；名称映射可从上述标签中的旧archive/README查看 |
| archive/20261023/* | 本次关闭的52个已被main包含的分支tip；逐项映射见[治理回执](../../evidence/project-governance-20261023.json) |
| archive/full-bay-mount-2.8.1、archive/f22-belly-bones-2.11、archive/x02s-eml-2.12、archive/cfa-animation-2.15 | 弹舱/动画/布局等重要未采用或撤回方案，继续条件见研究索引 |

读取历史用 `git show 标签:仓库相对路径`，例如 `git show archive/20261023/pre-governance-main:docs/development.md`。恢复研究从当前main新建分支后取精确差异，不整支合回旧核心/Lua/配置。

本次移出日常树的三个长篇旧正文为docs/archive/architecture-before-2.16.txt、development-before-2.16.txt、multi-sp-mount-design-before-2.16.txt；还有已退役的docs/research/three-sp-candidate.txt占位说明。旧原文在上述标签和回收站可恢复；原研究源码、原始证据和未用动画文件没有删除。退役脚本若仍引用旧说明，未来恢复时从标签取回，不为它保留日常重复全文。

## 未提交内容与本机备份

| 本机位置 | 保存内容及边界 |
|---|---|
| E:/AC7_MultiSP/backup/project-governance-20261023/ | 整理前all-refs.bundle、refs-before.txt、inventory-before.json；9个脏工作树的HEAD、工作区/暂存二进制差异、未跟踪文件ZIP及摘要 |
| E:/AC7_MultiSP/Temp/preserved-local-20261023 | 原根目录27项变动的可编辑保留副本，原分支codex/aircraft-mod-static-plan；独立ZIP同时保留。根目录ignored资产仍在原位置，没有搬进该副本 |
| 同备份目录worktree-metadata-*.zip | 4条缺目录登记及1个干净旧工作树的Git元数据；回读核对后原登记送回收站。记录缺失不证明其原料已找回 |
| E:/AC7_MultiSP/backup/project-archive-20261017/ | 早期Git bundle、工作树ZIP、git-archive-manifest.json、historical-evidence-and-references.zip、historical-builds.zip及materials-archive-manifest.json；覆盖范围按旧清单，不凭名字推断 |
| E:/AC7_MultiSP/backup/fov-editor-archive-20261019-081507.zip | 原版/仅键盘FOV尝试、诊断脚本与日志等；功能未通过，旧说明曾记录70成员核验，本轮未重复解包验收 |

FOV卸载件在backup/compatibility-pwmki-standalone-20261019-081643/removed/；对应本机回执随preserved-local-20261023保留。恢复前先备份后续个人资料，不拿旧多SP核心覆盖当前版本。

## 原料与冻结目录

普通Git和本轮差异ZIP均不包含ignored模型、游戏资源、工具、完整构建或转储。17个已接收旧工作树及旧main工作树本轮原位封存，保留路径依赖，名单见工作区索引和治理回执；锁定只防误清理，不代表文件权限只读。

本轮没有清理Temp大资产、backup个人资料或旧发布包。原完整内舱、动画、旧TGM资源、未接收更新器继续保留。后续恢复依次核对提交、未提交快照、ignored原料、工具和摘要；缺少任一层就明确缺项。

旧Vortex链接备份中的部分链接目标已不存在，不能凭备份目录存在宣称可恢复。先区分链接本身和实际字节，必要时按原包或物化备份恢复，不自动跟随覆盖游戏目录。
