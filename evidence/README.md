# 精选证据索引

## Su-30SM 固定挂载独立研究

- [当前基线快照](fixed-mount-base-snapshot.json)：正式 1.0.1 加未提交兼容修订，复制时的逐文件哈希；主工作区保持原状。
- [对齐正式 1.0.2](su30sm-fixed-mount-base-1.0.2.json)：将 `a114616` 合入研究分支，运行文件与正式提交一致；主工作区仍在修改的文件不纳入。
- [四翼下两机腹方案](su30sm-fixed-mount-plan.json)：6AAM 六槽映射四处、LAGM 六槽映射两处；旧快照变换候选与资源比较，不是原型验收。
- [本阶段保护核对](su30sm-fixed-mount-stage1.json)：主工作区分支与 HEAD、正式保护核心、11 个已安装 Lua/DLL 文件；明确记录快照后外部继续修改 main.lua 的事实，不将状态清单相同误认为文件内容相同。
- [当前六阶段现场](su30sm-fixed-mount-live.json)：初始 LAGM、6AAM 切换/单发、LAGM 切换/单发/装填；模型身份、活动路由、弹体及延迟恢复队列，未改游戏内存。
- [原生路由分析](su30sm-fixed-mount-routing.json)：实际发射变换、父链选池、单枚显隐和延迟恢复；明确区分静态指令路径与现场实际观察。
- [本轮保护核对](su30sm-fixed-mount-stage2.json)：组件连续身份、语法解析、保护核心与已安装文件核对；不作为原型验收。
- [历史资源摘要](su30sm-mount-assets.json)、[基类摘要](su30sm-base-pawn-assets.json)、[历史四阶段现场](su30sm-mount-live.json)、[历史原生入口](su30sm-native-mount-findings.json)：从 `c44e3b9` 引入的 0.1.9 证据，不冒充当前 1.0.1 实测。
- 当前实施范围、工具和未确认项见[固定挂载研究](../docs/research/su30sm-fixed-mount.md)。


## 当前仓库 1.0.2

- [版本与兼容摘要](../docs/version-1.0.2.txt)：分批验证成果、合装范围与待验证时序修订。
- [用户验证](1.0.2-user-validation.json)：J-20 恢复双选/出击，以及用户后续反馈的偶发单 SP。
- [仓库准备记录](1.0.2-prepared.json)：版本、语法、UI 构建与保护核验；本轮保持游戏运行，未部署新版本。

## 历史 1.0.1

- [用户验证](1.0.1-user-validation.json)：用户反馈一切正常；日志补充双 SP 补给成功且继续切换。记录验证边界，不外推所有任务均已覆盖。
- [正式安装回执](1.0.1-installed.json)：独立正式核心与正式标题 UI 成套安装；本轮不重启游戏、不制作 ZIP。

## 1.0.1 候选

以下为历史过程，待验证与失败修正状态不覆盖当前正式版结论。

- [管理参数第 2 版](1.0.1-candidate1-management-revision2.json)：用户确认多人屏蔽和英文正常；记录 F8 精简、语言/档位迁移，以及补给 NO WEAPON 日志与用户复测决定。
- [菜单继承与英文启动](1.0.1-candidate1-menu-inheritance.json)：前一次修正仍失败，现场确认 UIGameMode 经 LobbyGameMode 继承 MultiPlayerGameMode。当前已重装并启动，日志确认单人菜单启用、语言 en；F8 实际操作仍待用户确认。
- [共享菜单误禁用](1.0.1-candidate1-mode-regression.json)：用户启动后全部失效；LobbyGameMode 为共享菜单类型，已移除其多人专用判定并重新安装，实际恢复待验证。[首次安装回执](1.0.1-candidate.1-initial-installed.json) 单独保留。
- [修订依据](1.0.1-candidate1-findings.json)：多人识别、英文提示及补给 HUD 的源码/反射依据，实玩待用户验证。
- [安装回执](1.0.1-candidate.1-installed.json)：独立核心、UI、9 个 Lua、音频与版本的安装核对及旧配置/存档保护。

## 历史 1.0.0

- [补给起飞后右下 HUD 短暂闪回](1.0.0-post-resupply-hud-flash.json)：2026-10-15 用户反馈，约几百毫秒后自行恢复；仅记录、暂不修复，原因未确认。
- [用户验证](1.0.0-user-validation.json)：性能候选实测没有问题，整理为 AC7DualSP 1.0.0。
- [安装核验](1.0.0-installed.json)：当前版本的文件、接口、语法、配置与存档保护记录。
- [发布核验](1.0.0-release.json)：独立 ZIP 的成员及哈希、运行库签名，不代表实机功能测试。

## 历史记录

以下按各阶段保留；“最新/当前/尚待测试”均指当时，不覆盖上方 1.0.0 结论。

性能与显示候选 1：[`0.1.9-performance-candidate.1-installed.json`](0.1.9-performance-candidate.1-installed.json) 记录语法、哈希、安装回读及保护文件核对；[`performance-candidate1-findings.json`](performance-candidate1-findings.json) 区分用户反馈、代码依据、标题页检查和未验证项；说明见[性能调整](../docs/performance-polish.md)。

2026-10-14 音质对照：[`0.1.9-audio-comparison-uninstalled.json`](0.1.9-audio-comparison-uninstalled.json) 记录完整卸载；用户反馈卸载后听感相同，随后按要求恢复，见[`0.1.9-audio-comparison-restored.json`](0.1.9-audio-comparison-restored.json)。源码排查和未验证假设见[音质排查](../docs/research/audio-quality-audit.md)。

0.1.9 已安装，见[`0.1.9-installed.json`](0.1.9-installed.json)。本地版本：[`0.1.9-user-validation.json`](0.1.9-user-validation.json) 为用户确认范围；[`0.1.9-runtime-manifest.json`](0.1.9-runtime-manifest.json) 记录 Git 核心与本地 WAV 的来源和哈希。

体验候选 9：[`experience-candidate9-findings.json`](experience-candidate9-findings.json) 记录候选 8 用户通过、已安装音量版本核对和新增音量/名称留白调整。

体验候选 8：[`experience-candidate8-findings.json`](experience-candidate8-findings.json) 记录 TLS 实际类型、候选 7 抛错及修正依据，新增实玩结论独立记录。

体验候选 7：[`experience-candidate7-findings.json`](experience-candidate7-findings.json) 区分用户反馈、只读原生研究及未实测的新修订。已于 15:26 安装，见[`experience-candidate.7-installed.json`](experience-candidate.7-installed.json)。

> 最新：用户提供 AC7 原生 UI 截图，确认弹药预览计入部件并保留比例，重新授权完成后安装。候选 6 一并纳入候选 5 的 SP2 WAV；见[研究与编译记录](experience-candidate6-findings.json)、[安装回执](experience-candidate.6-installed.json)。新界面、部件 A/B 和新音色尚待用户实玩；候选 5 未安装记录保留为历史。

> 最新：[候选 2 用户确认全部功能正常](experience-candidate2-user-validation.json)；[候选 3 金色选中与英文文案安装回执](experience-candidate.3-installed.json)、[机库外观现场记录](experience-candidate3-ui.json)。安装核验与外观实测分别记录，候选 3 代理未出击。

> 本轮新增[体验优化只读研究](experience-polish-findings.json)：STANDBY 与金色光标结构、机库原生动作链、START MISSION 前后 33 秒接入间隔、选择音事件与音库对象。候选已编译安装，见[候选 2 安装回执](experience-candidate.2-installed.json)和[候选 1 复测及修订](experience-candidate2-revision.json)；只读研究和安装核验均不是实玩通过的证明。

> 此前初步实玩通过的版本为 0.1.8：[用户初步验证](0.1.8-user-validation.json)、[运行文件清单](0.1.8-runtime-manifest.json)、[安装回执](0.1.8-installed.json)。下文候选“当前/尚未实玩”为当时记录；用户现已确认补给及所有功能初步可用，大范围和持久测试未完成。

返回[项目入口](../AGENTS.md)，相关结论见[架构](../docs/architecture.md)、[核心研究](../docs/research/core-findings.md)、[模型研究](../docs/research/weapon-models.md)。

| 保留内容 | 说明与边界 |
|---|---|
| `0.1.7-*.json` | 迁移前的最近离线检查及安装核验；本轮没有重新执行这些检查 |
| `native-hud-layout.json` | 自有原生 HUD 资源的结构摘要 |
| `native-weapon-message-structure.json` | 2026-10-13 离线解析的中央武器提示控件、布局与页面动画/调用摘要；尚未确认运行时隐藏机制 |
| `native-loadout-slots.json` | 候选 2 加载失败后，只读核对标题、列表、名称及图标的槽/父容器；不证明活跃运行时类型 |
| `native-ui-slot-switch-findings.json` | 候选 2 的用户反馈、三次出击槽判断错误及候选 3 呈现事务修复方向；候选 3 未实玩 |
| `ac8-resupply-comparison.json` | 用户三张补给截图的观察、实现取舍、原生结束函数签名与未验证项；不证明真实跳转和时序 |
| `preparation-review-findings.json` | 准备页重选/退出/重进的源码缺陷、双栏资源坐标、交接取舍与编译安装摘要；未实玩 |
| `preparation-live-validation.json` | 授权键盘操作的准备、取消重进、修改、保存与出击结果，以及未测边界和显示遗留；不代表飞行或补给通过 |
| `resupply-live-findings.json` | 用户实玩补给双选及起飞卡住的日志证据、离线修复记录及候选 6 安装信息；最新实玩失败与修订另见 input-audio-revision.json |
| `experience-candidate.2-installed.json` | 当前候选 2 安装核验；独立核心及整套 Lua，任务内效果待复测 |
| `experience-candidate2-revision.json` | 候选 1 用户复测、两处错误日志、VR 音源核对、用户选定的新音效及候选 2 修订依据 |
| `experience-candidate.1-installed.json` | 历史候选 1 安装核验；随后发现机库、HUD 和 SP2 声音问题 |
| `management-candidate.7-installed.json` | 历史全套安装；Lua 分别校验补给阶段关卡与运行任务，DLL 沿用候选 6；安装核验通过，效果待实玩 |
| `resupply-completion-revision.json` | 候选 6 用户复测及暂停现场只读证据；阶段 World 误判原因与候选 7 修订边界 |
| `management-candidate.6-installed.json` | 历史安装；用户随后确认本次音效未出现、双选及起飞正常，起飞后停用另见候选 7 |
| `management-candidate.5-installed.json` | 历史候选 5；真实补给 YES 桥接、飞机切换锁定组件同步；独立核心构建，配置/存档/正式核心保护核验，尚未实玩 |
| `management-candidate.4-installed.json` | 历史全套安装；准备布局/字体/导航/保存、无限弹药 HUD 修复及文件保护核验；实机另记 |
| `management-candidate.3-installed.json` | 准备双栏、确认编辑、取消交接及重进处理的成套安装；界面重新编译、核心沿用，配置/存档保留；未实玩 |
| `management-candidate.2-installed.json` | Lua 补给双栏/YES-NO 更新的整套安装核验，DLL 均沿用、配置及存档保留；未实玩 |
| `management-candidate-installed.json` | 管理候选 1 的 UI 构建、配套导出、八 Lua 语法、十个运行文件、完整旧模组备份和保护核验；不代表实玩通过 |
| `merged-candidate.3-installed.json` | 候选 3 的成套安装核验、旧模组完整备份与保护文件；不代表加载或切换已实玩通过 |
| `native-loadout-structure.json` | 准备页类、控件与布局摘要 |
| `native-resupply-structure.json` | 2026-10-13 只读解析的补给页父类、控件、布局及蓝图调用摘要；不证明运行时事件顺序 |
| `resupply-candidate-installed.json` | 2026-10-13 候选核心接口、Lua 语法与安装文件核验；保留配置、存档、原模组备份及正式核心，不代表实玩通过 |
| `merged-candidate-installed.json` | 补给与 HUD 合并候选的十个运行文件、Lua 转发接口与安装保护核验；不代表实玩通过 |
| `native-ui-api-failure.json` | 用户实玩出击选择失败的日志摘要，以及共用补给/HUD 接口误用的离线诊断 |
| `merged-candidate.2-installed.json` | 移除旧引擎缺失接口后的合并候选 2 安装核验；运行时菜单和补给仍待复测 |
| `weapon-model-layout.json` | Su-35S 挂架、Socket 与武器装填数量摘要 |
| `dual-mount-prototype.json` | 仅几何分配原型，未安装游戏功能；内部旧版本字样属于当时结论 |
| `unusable-packed-disk-code.json` | 加壳磁盘代码不适合作为写入接口的依据 |
| `native/` | 精选历史函数体与反射接口，不能重用旧进程绝对地址 |
| `integration/` | 早期实机原型通过记录，保留为研究依据，不等同于最新版本通过 |

完整 PAK 资源副本、资源解析中间 JSON、早期日志、失败尝试和重复版本检查已回收；重要失败机制与未知项保留在[关键决策](../docs/decisions.md)。当前运行文件与原始发行包 hash 另见[基线记录](../Release/baseline-0.1.7.json)。

本次整理的范围与结果见[迁移概要](migration-summary.json)，逐文件复制审计留在被忽略的 `Temp/`，不作为游戏功能验证记录。
## 本轮原生流程复核

[native-flow-revision.json](native-flow-revision.json) 记录 Su-30SM QAAM＋LAGM 的候选 4 失败复测，以及用户停留主菜单时取得的原生 YES/武器切换调用链。候选 5 已构建并成套安装，尚未实玩验证。此前仅根据函数名称判断结束入口、根据日志判断控件树故障的结论已撤回，详见[修订说明](../docs/native-flow-revision.md)。

## 候选 5 复测与候选 6 修订

[input-audio-revision.json](input-audio-revision.json)记录本次用户复测、暂停现场的音效引用失配，以及两个原生补给输入入口。候选 6 已编译安装，尚未实玩，不能把只读函数分析当成实玩通过。说明见[输入与音效修订](../docs/input-audio-revision.md)。

## 候选 6 复测与候选 7 修订

[补给结束记录](resupply-completion-revision.json)记录正常 YES 后阶段世界身份误判。当前候选 7 只更新 Lua，两枚 DLL 沿用候选 6；配置、存档与正式载荷保护核验通过，未由代理实玩。上文“尚未实玩”属于对应候选安装当时状态。
## 2026-10-14 候选 5 准备记录

[experience-candidate5-prepared.json](experience-candidate5-prepared.json) 记录用户确认 CFA-44 双 SP 正常、自定义 WAV、原生部件调用链及离线核验。本轮暂不安装，不能把编译通过当成新音频游戏内试听通过。
