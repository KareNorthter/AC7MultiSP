# 工作区与成果索引

本页是任务登记入口。当前正式版本见[README](../../README.md)，分支/提交规范见[开发规范](workflow.md)。已封存路径不要继续编辑，恢复从main新建任务；重要半成品见[研究索引](../research/README.md)。

## 正式入口与明确保留的工作

| 任务/状态 | 分支与责任 | 路径或来源 | 范围与下一步 |
|---|---|---|---|
| 正式主工作区 | main；接收/整合任务 | E:/AC7_MultiSP | 正式源码及统一文档；不要让研究线程直接并行写入 |
| CFA-50检索与F6退格隔离，源码已接收待验证 | 本安装器/F6反馈聊天；基线026dd43；codex/fix-cfa50-aircraft-name | C:/Users/83717/.codex/worktrees/cfa50-aircraft-list/AC7_MultiSP；现行源码main | debug_aircraft.lua读取游戏正式名称，overlay.cpp补齐预读/原始键盘退格隔离；[定位记录](../../evidence/f6-aircraft-input-source-20261024.json)确认旧修复已合入且已安装UI与2.25.2交付一致。cc924a5已纳入2.25.3 alpha1完整构建，未安装或实测，现成2.25.2产物不含修订；CFA-50 EML研究范围不变 |
| 双语主页与发布流程，已接收待作者上传 | 本管理聊天；基线c1092c0；codex/integrate-pages-workflow，18eea51已合main；工作树受置顶保护保留 | C:/Users/83717/.codex/worktrees/integrate-pages-workflow/AC7_MultiSP；材料E:/AC7_MultiSP/Temp/tasks/20261024-release-2.25.2/ | 接收“mod内置更新”的未提交主页，集中双语词条并保留preview；按用户正式2.25.2确认整理[完整发布流程](release-workflow.md)，工具准备EXE、Nexus通用ZIP和整站材料，四处上传由作者完成。原ZIP、来源树及冻结产物不改，见[接收回执](../../evidence/pages-workflow-intake-20261024.json)与[发布准备](../../evidence/2.25.2-publication-preparation.json) |
| 集成交付：alpha4补给预览/日志源码已修订 | 本集成交付聊天；本轮基线main 622e639；codex/integrate-20261024 | C:/Users/83717/.codex/worktrees/integrate-20261024/AC7_MultiSP；日志E:/AC7_MultiSP/Temp/tasks/20261024-alpha3-log-review/ | 本轮修订mount_mesh_override.hpp同组件任务/预览网格记录冲突、bounded_log.hpp及build.py的版本注入，统一alpha4源码候选与双语更新说明；[来源和验证边界](../../evidence/2.25.3-alpha4-preview-log-source.json)。用户授权修复1和3；不编译/测试/安装/操作游戏/上传，F6发布性能待办、两机公共舱继续暂缓。此前[alpha3 EXE](../../evidence/2.25.3-alpha3-exe-delivery.json)原样保留 |
| 快速进入测试任务，2.25.2初始化修复待实测 | 来源964507d及整合c9bc440 | C:/Users/83717/.codex/worktrees/quick-mission-launch/AC7_MultiSP；现行源码main | 原主菜单准备/归属修复保留；辅助异常隔离和日志上方布局已随2.25.2完整构建。原[失败定位](../../evidence/quick-mission-failure-20261024.json)、[三SP中止日志](../../evidence/quick-mission-trisp-failure-20261024.json)及[唯一设计](../engineering/quick-mission-launch.md)保留，尚未验证新游戏效果 |
| Mod机体兼容性续研，多机舱门源码候选待接收/验证 | 研究聊天01a1692d-b258-72e1-b585-88d0e1bf1fc7；续接01a15f12-100b-7640-9d2b-2c54ef825a45；同步main/09cdc82 | codex/research-mod-aircraft-compat；C:/Users/83717/.codex/worktrees/research-mod-aircraft-compat/AC7_MultiSP | 原名称/IRCM及CFA-50候选已在main。本轮独占通用舱门模块、weapon_bridge.cpp事务接点、su30sm_fixed_mount.hpp的Parts入口及Light中Dark收尾责任转移；同步挂载设计相关专节、续研正文和回执。最新7e255e1已由集成交付任务纳入2.25.3 alpha1完整构建；实现普通SU-57/TGM、DarkStar、XFA-33 A/B/C的限定候选；公共域只保重建连续性，挂架停用仍需现场区分。固定成本读取，同值不写，无新增逐帧扫描/反射/日志；未实测性能。用户逐机实测；本轮不自动编译/测试/安装、不操作游戏或推送 |
| F6启动更新选项，已接收待实测 | 来源7f213aa；codex/feat-installer-f6-usability | E:/AC7_MultiSP/Temp/tasks/20261024-installer-f6/worktree | 默认开启选项、配置19补缺失字段及管理协议22已纳入本轮2.25.0完整构建；旧源码阶段未构建记录保留，实际联网和保存行为待用户验收 |
| 安装器与F6易用性调整，已接收待人工验证 | 来源5a33e3a、DPI修订050dc3d；codex/feat-installer-f6-usability | E:/AC7_MultiSP/Temp/tasks/20261024-installer-f6/worktree | 新首页/F6/DLC许可/DPI已纳入本轮2.25.0整合。原2.24.3、DPI回执和成品保留；旧线程停止构建的指令只记录当时范围，本集成按用户新授权制作新EXE，未安装/操作游戏/上传 |
| 双语文档与通用交付，待游戏验证 | 本管理聊天；基线85ab3a0；codex/integrate-bilingual-delivery完成后关闭 | C:/Users/83717/.codex/worktrees/project-governance/AC7_MultiSP；完成后detached保留 | 接收根目录九份人工文档，生成英文并集中游戏/安装器词条；本地候选2.24.2、安装器0.3.0，独占docs/user、游戏文字调用、构建封装及对应正文。[交付回执](../../evidence/2.24.2-delivery.json)记录具体核对，不安装或操作游戏 |
| 本机资产与GitHub整理，已接收 | 本管理聊天；基线6c62f53；完成锚archive/20261023/local-assets；codex/chore-local-assets完成后关闭 | 同整理工作树，完成后detached保留；正式入口仍为根目录main | 733项原名归组、其余按依赖原位索引；[清单](../../evidence/local-assets-20261023.json)保存1,206项位置及22个研究提交恢复锚。37个其他工作树未改 |
| 安装器来源，已接收必要差异 | codex/delivery-installation；“设计 Mod 下载与安装方案” | C:/Users/83717/.codex/worktrees/delivery-installation/AC7_MultiSP；a99a404 | 原树及0.2.0成品保留不动；src/delivery及封装入口接收到上行整合任务，并按[最新交付方案](delivery.md)改造。自制Vortex扩展源码仅保留研究，当前通用包不分发 |
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
