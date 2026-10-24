# 工作区与成果索引

本页是任务登记入口。当前正式版本见[README](../../README.md)，分支/提交规范见[开发规范](workflow.md)。已封存路径不要继续编辑，恢复从main新建任务；重要半成品见[研究索引](../research/README.md)。

## 正式入口与明确保留的工作

| 任务/状态 | 分支与责任 | 路径或来源 | 范围与下一步 |
|---|---|---|---|
| 正式主工作区 | main；接收/整合任务 | E:/AC7_MultiSP | 正式源码及统一文档；不要让研究线程直接并行写入 |
| Mod机体兼容性续研，CFA-50舱门源码候选待接收/待验证 | 本聊天；续接01a15f12-100b-7640-9d2b-2c54ef825a45；基线main/85ab3a0；本轮核对main/c1092c0，相关挂载头相同 | codex/research-mod-aircraft-compat；C:/Users/83717/.codex/worktrees/research-mod-aircraft-compat/AC7_MultiSP | 独占[续研正文](../research/mod-aircraft-compatibility.md)、研究回执及原分组工具；新增src/cfa50_bay_animation.hpp，src/su30sm_fixed_mount.hpp仅接Parts回调，src/weapon_bridge.cpp仅接事务绑定/退出，挂载设计同步CFA-50专节。运行2.25.2已含长按修复，用户确认其他Mod长按效果正常；EML炮体存在，顶部旧域冻结已定位并完成限定源码收尾，ESM暂缓。10包及XFA-33组已入库，当前A不切换；十字键稍后实测。本轮只读取现场、静态审阅源码和文档，不编译/测试/安装、不操作游戏或推送 |
| 文档治理，中文基本认可、英文待补齐 | 本管理聊天；四份中文完成锚archive/20261023/player-docs；安装说明基于79a90e4重写，codex/docs-universal-install完成后关闭 | C:/Users/83717/.codex/worktrees/project-governance/AC7_MultiSP；完成后detached保留 | 安装说明仅按未来通用ZIP及配套EXE编写，三种安装方式共用；根目录人工修改的功能说明、更新日志保留，不混入提交。双语及程序接入继续待用户安排；[文档复用](player-docs.md)记录边界 |
| 本机资产与GitHub整理，已接收 | 本管理聊天；基线6c62f53；完成锚archive/20261023/local-assets；codex/chore-local-assets完成后关闭 | 同整理工作树，完成后detached保留；正式入口仍为根目录main | 733项原名归组、其余按依赖原位索引；[清单](../../evidence/local-assets-20261023.json)保存1,206项位置及22个研究提交恢复锚。37个其他工作树未改，双语及界面接入由用户稍后安排 |
| 安装器，待接收/待验证 | codex/delivery-installation；“设计 Mod 下载与安装方案” | C:/Users/83717/.codex/worktrees/delivery-installation/AC7_MultiSP；a99a404 | 用户原交互文档及0.2.0源码/成品留此；本轮未合代码，后续交付重做由用户安排 |
| 原根目录资料，封存待甄别 | codex/aircraft-mod-static-plan；原兼容资料工作 | E:/AC7_MultiSP/Temp/preserved-local-20261023；0b75b21 | 原27项未提交变动逐文件保留；核对后按需接收独有结论，不整套覆盖main |
| 旧更新实现，封存待续 | codex/update-feature | C:/Users/83717/.codex/worktrees/update-feature/AC7_MultiSP | 11项未提交变动，已做快照；与现行更新页比较后再接收 |
| 独立更新器半成品，封存待续 | codex/update-preview | C:/Users/83717/.codex/worktrees/update-preview/AC7_MultiSP | 16项未提交变动，源码/说明/缓存保留；尚未成为正式安装方案 |

本轮管理聊天为“评估项目健康并规划整理”。上述“封存待续”是保留状态，不等于正在执行研究；不因聊天空闲而丢弃源码。

## 其他保留分支

下面分支尚未证明可以关闭，可能是未合祖先、差异接收或旧研究；本轮保留，不按名称猜测完成。实际责任聊天与新任务范围由继续工作的用户指定，开工后更新相应行。

| 分支 | 提交 | 工作树 | 状态 |
|---|---|---|---|
| codex/2.15-hud-mission14-localization | 83dbc0c | E:/AC7_MultiSP/Temp/hud-mission14-localization | 保留待甄别 |
| codex/camera-controls-options | c3102d4 | 无工作树；从提交恢复 | 保留待甄别 |
| codex/camera-hold-research | 579215a | 无工作树；从提交恢复 | 保留待甄别 |
| codex/camera-input-coverage | b4290e2 | 无工作树；从提交恢复 | 保留待甄别 |
| codex/camera-r3-short-press-fix | 4ba961a | 无工作树；从提交恢复 | 保留待甄别 |
| codex/cfa44-10ms | d7baecd | C:/Users/83717/.codex/worktrees/cfa44-10ms/AC7_MultiSP | 保留待甄别 |
| codex/cfa44-startup-review | 002ef58 | C:/Users/83717/.codex/worktrees/cfa44-startup-review/AC7_MultiSP | 保留待甄别 |
| codex/controller-layout-refresh | 064dccd | 无工作树；从提交恢复 | 保留待甄别 |
| codex/f14ia-ircm-compat | 9cc7f36 | 无工作树；从提交恢复 | 保留待甄别 |
| codex/f18e-mount-layout-feedback | 1260a6c | 无工作树；从提交恢复 | 保留待甄别 |
| codex/f18etg-socket-reference-fix | 7b89fc8 | C:/Users/83717/.codex/worktrees/native-sp-mount-takeover/AC7_MultiSP | 保留待甄别 |
| codex/five-aircraft-mount-review | 690e36b | C:/Users/83717/.codex/worktrees/five-aircraft-mount-review/AC7_MultiSP | 保留待甄别 |
| codex/hud-weapon-label-options | 0bb86dd | 无工作树；从提交恢复 | 保留待甄别 |
| codex/mod-aircraft-compat | e77a2cf | 无工作树；从提交恢复 | 保留待甄别 |
| codex/native-sp-mount-feedback-r3 | ba20e79 | 无工作树；从提交恢复 | 保留待甄别 |
| codex/native-sp-mount-takeover | 9364838 | 无工作树；从提交恢复 | 保留待甄别 |
| codex/target-selection-research | 21f2074 | C:/Users/83717/.codex/worktrees/target-selection-research/AC7_MultiSP | 保留待甄别 |
| codex/ui-interaction | f5aeb84 | C:/Users/83717/.codex/worktrees/ui-interaction/AC7_MultiSP | 保留待甄别 |

## 已完成分支与原位封存

本次移除52个已被main包含的活动分支名，tip均保存在archive/20261023/下；[治理回执](../../evidence/project-governance-20261023.json)逐项列出原名、提交、标签和处置。

- 34个原本无工作树的已合分支关闭。
- 18个干净且已合的旧工作树解除原分支：其中17个有ignored文件，原位detached并锁定；1个无任何ignored或未提交内容的cfa44-muzzle-notes副本送回收站。
- 原main所在Temp/integration-2.22.2-alpha1另外原位封存，main回到根目录。冻结产物、原料和旧路径依赖均未迁走。
- 4个原本缺目录的锁定登记已备份元数据后移出；恢复锚和缺原料风险留在研究/归档索引，不冒称恢复成功。
- 其余历史detached研究树、脏树和未证明已接收分支继续保留。Temp散落文件、旧检查和空暂存已归组；大原料、转储与个人备份保留，见[本机资产索引](../archive/local-assets.md)。Release旧产物见[归档索引](../archive/README.md#历史核心与发布包)。

完整前后盘点及9处未提交快照见E:/AC7_MultiSP/backup/project-governance-20261023/；恢复方法见[归档索引](../archive/README.md)。

## 后续任务登记格式

在适合的表中更新一行：任务目的｜责任聊天｜基线main提交｜分支/工作树｜独占修改范围｜状态｜来源/验收链接。状态用进行中、待接收、待验证、已接收、封存待续、已关闭。不要只写“最新”或用版本目录名代替提交。

首次正文整理以d2653ce合入main并关闭分支。整理工作树因当前置顶聊天受应用保护，自动归档被拒绝；本管理聊天核对干净后从main复用于上表文档任务，完成后回到detached状态。不为清理而更改用户置顶设置，其他任务仍须独立工作树。

本次收尾后保留23个分支和39条有效工作树登记；其中多数为封存原料或旧研究，并非39个活动开发任务。日后新增任务不得从已封存整合目录直接继续，也不要把文档维护再拆成多个无人负责的版本。
