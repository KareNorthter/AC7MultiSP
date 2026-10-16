# 主线与文档整理记录

日期：2026-10-16。目标是维护性和 AI 检索效率；不改功能、不操作游戏、不编译或运行测试。

## 当前入口

- 主工作区恢复为 `E:/AC7_MultiSP`，`main` 从旧位置快进到已发布 `v1.1.0`，再承载本次文档整理。
- 1.1.0 功能提交为 `d2f2221`；正式发布标签未改变。
- 原发布 ZIP 已复制到主工作区 Release，按原发布回执核验，不重新打包。
- 活动分支从 16 个收敛到两个：`main` 与 `codex/pending-ui-polish`。

## 未合入成果

| 成果 | 保存位置与后续处理 |
|---|---|
| UI 三个提交 | `codex/pending-ui-polish`，原 `codex/ui-headings-hud-fixed`；f800518 / 07c81c0 / 7e2fe4a。涉及 SP 标题、中央文字位置/开关和弹药高亮，待逐项整合，不能用整分支覆盖主线 |
| 根工作区原未提交的 36 个文件 | 快照提交 `0637abc`，标签 `archive/20261016/three-sp-research`；含三 SP 草稿、布局、FOV 键盘模块等独有资料。只保存，不冒充已验证成果 |
| 早期持久 SP 研究 | `archive/20261016/persistent-sp-research`，保留 b0f4acc 独有研究 |
| 早期 Su-30SM 研究 | `archive/20261016/su30sm-dual-mount-research`，保留 c44e3b9 独有研究 |
| 已等价合入 UI | `archive/20261016/three-sp-ui-polish`；7b8ce4c 的等价补丁已合入，无需重复合并 |

其余旧分支也都建立 `archive/20261016/<去掉 codex/ 的原分支名>` 恢复标签后才移除分支指针；旧 main 单独保留 `archive/20261016/main`。完整映射见[整理回执](../evidence/repository-consolidation-20261016.json)。

如确需恢复历史，在主工作区从对应标签新建 `codex/` 分支；不要移动 main 或覆盖当前文件来恢复。未合入 UI 与根快照可能存在交叠，整合前先看差异和用户目标。

## 旧工作区与备份

五个旧工作区保留原目录、忽略文件和构建/采集资料，切到归档引用的分离 HEAD，并加 Git 工作区锁防止被意外清理。
它们的 AGENTS.md 已改为本地迁移提示；这是唯一预期的未提交改动，原 AGENTS 已保存在标签。锁不等于文件系统只读，不应在旧目录继续修改。

- `Temp/su30sm-fixed-mount-prototype`：1.1.0 原发布、构建和采集资料。
- `Temp/persistent-sp-research`、`Temp/su30sm-dual-mount-research`：旧研究。
- `C:/Users/83717/.codex/worktrees/release-1-0-2-f6/AC7_MultiSP`：旧 F6 发布。
- `C:/Users/83717/.codex/worktrees/three-sp-ui-polish/AC7_MultiSP`：未合入 UI 的原工作区。

`backup/maintenance-20261016/` 保存逐文件原字节、原引用和工作区清单，以及已核验的 `repository-before.bundle`。备份与 Temp 都由 .gitignore/.tfignore 排除；没有清空旧工作区或永久删除资产。

## 文档整理规则

55 份旧说明按原结构归档到 docs/archive，修正相对链接（包括原历史入口中失效的 100 处），再提炼当前状态、架构、开发、管理和挂载说明。
旧证据索引另存归档；原证据文件路径和字节保持，因为当前安装器仍依赖部分旧回执。
`docs/research/three-sp-candidate.txt` 仅保留明确退役的路径提示，以免旧准备器文档依赖悬空；不是当前入口。

`.ignore` 让日常检索跳过历史文档、证据、参考与第三方源码；它不影响 Git 保存。查历史时直接读索引目标或显式 rg --no-ignore。
当前文档直接修订正文，避免一轮一条追加时间线；过去的运行状态、PID 和操作授权不自动延续。

## 后续工程债务

构建来源摘要覆盖不足、研究/正式入口尚未统一、旧测试协议过时，已在[开发说明](development.md)列明。本轮不修这些代码，也不把文档整理宣称为稳定性修复。
