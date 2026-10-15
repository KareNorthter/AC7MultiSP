# AC7DualSP 项目入口

正式名称 **AC7DualSP**，当前正式版本 **1.0.1**。面向 Steam 版《皇牌空战 7》build **9855922**：保留普通导弹，同时选择和切换两种特殊兵装，分别保存弹药与冷却。现有 ZIP 仍为 1.0.0，本轮按用户要求不打包。

## 当前状态

**本工作区是独立固定挂载研究分支，不是主线安装入口。** 2026-10-15 从 1.0.1 及当时未提交兼容修订建立 `codex/su30sm-fixed-mount-prototype`，基线快照为 `cadacde`。研究目标为 Su-30SM 四处 6AAM＋两处 LAGM，允许重复出弹。已准备具体映射与只读采集工具，尚未修改运行核心或安装原型；用户稍后配合自由飞行。后续见[固定挂载研究](docs/research/su30sm-fixed-mount.md)。以下兼容批次是复制基线时的主工作区背景，不在本分支继续安装其他模组。

2026-10-15 14:10 Adler 抽样批次准备中：用户要求重新加入 Adler，并抽样高赞的不同类型模组。已停止游戏、备份配置/日志/存档，换下独立 PW-MKI，安装 Adler 主包及其原配本地化/数据表。静态表为 54 条，仅新增 Adler A/B（148/149），不安装全局兼容表；148 号 PW 方案已备份清除，其他方案保留。拟新增 All Part In CampaignMode（623）、Lock-On Crosshair UI（649）、TLS Damage and Range increase（190）；浏览器下载超时，三个文件尚未取得，整批尚未就绪。见[准备回执](evidence/compatibility-adler-samples-prepared.json)、[抽样安排](docs/research/mod-compatibility.txt)。本轮未改功能代码、编译或运行测试，未启动游戏。下文“跳过 Adler”已被用户本次请求更新。

2026-10-15 小批次用户验证通过：用户确认 Su-47 与独立 PW-MKI 双 SP、换机、出击、补给均正常。当前目录实际为 53 条，日志确认 PW-MKI（148）补给换装成功，本次会话无 Lua 错误。见[小批次用户验证](evidence/compatibility-small-addon-user-validation.json)。用户倾向不继续测 Adler，按未验证保留，不重新安装、不将 PW-MKI 结果扩大到 Adler；现有已验证组合保持不变。本轮仅更新记录，未改变游戏运行状态、未编译或运行测试、未发布或推送。

2026-10-15 13:51 缩减独立机体批次、未启动：用户反馈大量空模型机体且 DualSP 失效。日志确认统一 PPDT 加载 195 条机体，先后在 zx01 蓝图未安装、su47 默认兵装组件缺失处停止选装；错误未按机体结束，后续 PW-MKI 也无法接入。已撤至备份：Adler 与 2154/2274 共 5 包，仅保留 PW-MKI 独立包、FOV+S、比例制导以及原飞控。PW 单独表为原 52 条加 pw01（148）一条，不再用统一表的 192 号。Lua 改为读取 HangarManager.LoadedPlayerPlane／任务玩家实例兵装组件，按类和世界核对身份；换机后记录并清理旧机体页面错误。恢复选装页 closedRoot.owner 身份字段，STANDBY/PART 不隐藏根的修正仍保留。语法核验通过，核心/UI/外部文件不变，游戏已停止，按此前要求没有启动，实测待用户。见[小批次回执](evidence/compatibility-small-addon-prepared.json)。下方大批次已被取代，不能按旧清单重新装回。

2026-10-15 12:30 新增机体批次已准备、禁止自动启动：用户确认 DarkStar 上鼠标飞控、比例制导、FOV 正常，要求关闭游戏并准备下一批，晚些自行启动。已停止进程、备份配置/日志/存档，把 PW-MKI 替换版 4 个 PAK 移入备份，安装 Adler Standalone、PW-MKI Standalone、2154 配套 PPDT/PWDT、2274 名称/展示表与 NON-ASS 皮肤表。保留 FOV+S、比例制导、飞控和本 mod F6 入口。仅备份清除 CFA-44（140）的替换版旧方案，避免恢复原机后沿用不同武器；其他配置不变。文件核验通过，未编译/运行测试模块、未启动，新增机体实测待用户。见[新增批次回执](evidence/compatibility-addons-prepared.json)、[DarkStar 反馈](evidence/compatibility-flight-user-validation.json)。当前新增测试仅 PW-MKI、Adler A/B，统一表其他未安装项不可选择。

2026-10-15 11:54 飞控批次：用户确认 ADMM+ECM、50% 配弹并装增弹部件、检查点重来补测无问题，要求下一阶段优先 FOV/飞控/比例制导。已保留原 6 个 PAK，原样安装 AC7MouseAim 0.6.0-beta.1，不安装 AC7Weapons；管理入口新增配置 management_key，本机 F6（117），默认 F8，手柄 Back+Y 不变。Lua 语法和 UI 构建通过，核心/加载器/个人机体方案保留；见[飞控回执](evidence/compatibility-flight-batch-installed.json)与[当前测试顺序](docs/research/mod-compatibility.txt)。新增机体批次推迟，飞控实玩待用户，不将第一批通过扩大到本批。

2026-10-15 第一批实测更新：用户确认 STANDBY 换机 UI 消失已解决；PW-MKI 替换版出击、双 SP、降落补给与切换正常。日志对应机体 140、任务 ms06、100% 配弹，出击槽 0+1、补给后槽 2+1，当前会话无 Lua 错误。旧名称 ADMM/EML/ECM 来自替换版沿用 CFA-44 表项，实际武器类分别为 admm/railgun/eufm，不能把旧 ECM 简称当作实际 ECM。见[用户验证记录](evidence/compatibility-batch1-user-validation.json)。尚未完成 50% 配弹、增弹部件、检查点及新增机体/飞控批次；本次未更改游戏文件或停止当前任务。下方“菜单复测待用户”是本次反馈前的安装时状态。

2026-10-15 11:30 第二次换机修订：用户按 Su-57（TGM）双选→STANDBY→F-14A 再次复现 UI 消失。现场仍为 Review 根节点 Collapsed，第一次恢复钩子未触发。现取消 STANDBY/PART 的整页根节点隐藏及补救钩子，只还原本模组修改的兵装区域，由原生控制页面退出与复用。Lua 语法核对通过，已备份、安装并重启，外部 PAK 与个人配置未变。见[第二次修订回执](evidence/compatibility-standby-fix2-installed.json)，菜单复测待用户；不得把第一次尝试记为已解决，也不得据此认定外部模组冲突。

2026-10-15 11:21（未解决）：用户报告 ADF-11F 的 UAV 只能单挂（符合限制），并在 STANDBY 更换 Su-57（TGM）后失去全部菜单。首次尝试在 STANDBY/PART 重新激活时归还根节点，安装并重启；第三方 PAK、配置、核心/UI 均保持不变。见[第一次安装回执](evidence/compatibility-standby-fix-installed.json)。此尝试未通过随后用户复测。

2026-10-15 10:57：用户授权组织第三方模组分批安装与测试，并启动游戏。已在 1.0.1 基线上部署仅修改 Lua 的兼容候选：原生兵装名称、实际武器类编号驱动的弹药预览/行为判定。第一批为 PW-MKI 替换版及配套武器/第 1 皮肤/发光模块、FOV +S、比例制导；新增机体及飞控尚未安装。启动日志确认加载与单人菜单启用，实玩待用户。后续以[测试安排](docs/research/mod-compatibility.txt)和[第一批安装回执](evidence/compatibility-batch1-installed.json)为准；下方“本轮不重新启动”是此前正式整理阶段的边界，已被本次用户启动授权更新。正式版本与 ZIP 不变，不推送远程。

2026-10-15，用户反馈“一切正常”，要求停止游戏、更新版本、提交 Git，无需发布包。已验证候选整理为 1.0.1，功能逻辑不变，只统一版本标识与 UI 标题。最新日志记录双 SP 补给成功并继续切换。范围见[1.0.1 说明](docs/version-1.0.1.md)、[用户验证](evidence/1.0.1-user-validation.json)，部署见[安装回执](evidence/1.0.1-installed.json)。不重新启动游戏、不推送远程。

以下候选阶段记录属于历史，旧“待验证”和失败判断不覆盖当前版本结论：

2026-10-15 10:13：用户确认多人屏蔽与英文游戏正常。F8 改为管理参数第 2 版，默认 Game Language、标签“语言选择/Select Language”；移除未购买 SP 试用，右下 HUD/变淡与直接选择固定启用，中央简称保留开关，配弹范围 50%–100%、5% 一档（11 档）。已编译、安装并重启，日志确认协议 2、语言 auto/en；配置迁移后机体方案/键位/中央开关未变。补给日志是 NO WEAPON 已成功提交，用户表示可能记错并重新测试，本轮不改补给逻辑，见[管理修订记录](evidence/1.0.1-candidate1-management-revision2.json)。

2026-10-15 09:42：09:31 的修正仍误判，现场确认 AceUIGameMode_C → UIGameMode → LobbyGameMode → MultiPlayerGameMode。已改为先区分已知菜单世界中的 UIGameMode，再按当前页面识别多人；不能把 MultiPlayerGameMode 的继承判断当作多人对局专用证据。重新安装并按用户要求启动，日志已确认 singleplayer_menu 启用、语言 en、管理状态无错误；F8 实际操作与多人/任务待用户验证，见[现场继承与重启记录](evidence/1.0.1-candidate1-menu-inheritance.json)。

2026-10-15 09:31：用户反馈候选全部功能失效，日志为 disabled multiplayer_mode。根因是把单人菜单也使用的 LobbyGameMode 当作多人专用类型，已修正并重新安装；多人对局类型、联网状态和多人页面禁用仍保留。语法及安装核验通过，实际启动待复测，见[误判记录](evidence/1.0.1-candidate1-mode-regression.json)。后续不得再次以 LobbyGameMode 单独判定多人。

2026-10-15：按用户要求实现[1.0.1 候选 1](docs/version-1.0.1-candidate.1.md)，包含多人功能禁用、原生页面英文提示、F8 跟随游戏语言，以及补给起飞 HUD 连续接管。独立核心/UI 构建及 Lua 语法核对通过；用户负责实玩验证，不能宣称多人或英文实测通过。1.0.0 ZIP 保持原样，当前安装状态见[候选回执](evidence/1.0.1-candidate.1-installed.json)。

2026-10-14，用户确认性能与显示候选测试没有问题，要求整理为 1.0.0。1.0.0 沿用该运行逻辑，只统一名称、版本和管理窗口标题，整理文档与独立发布包。用户实测可用不等于所有机型、任务和检查点均已覆盖。

当前发布与验证范围见[1.0.0 版本说明](docs/version-1.0.0.md)，用户原话见[验证记录](evidence/1.0.0-user-validation.json)。性能修订依据见[性能与显示](docs/performance-polish.md)。此前候选状态属于历史，不是当前安装指引。

## 安装与使用

现有 1.0.0 ZIP 的[TXT 安装说明](docs/AC7DualSP-安装说明.txt)无需命令行。`Release/AC7DualSP-1.0.0.zip` 提供游戏文件、专用加载器、SP2 音频和运行库；它不是 1.0.1，本轮没有新 ZIP。本机 1.0.1 通过受控升级部署。

退出游戏并备份既有模组，将包内 `GameFiles` 的内容复制到含 `Ace7Game.exe` 的游戏根目录。已有其他加载器时不要直接覆盖。内部目录及启用行仍是 `DualSP` / `DualSP : 1`，确保旧配置兼容。

准备页依次选择 SP1、SP2，每步确认一次后进入 STANDBY。SP2 可选 NO WEAPON。中途补给双选后进入 YES/NO，默认 NO；NO 从 SP1 重选，返回回 SP2，YES 确认起飞。补给成功后才保存和应用。

键盘默认 1/2/3、手柄十字键左/上/右直接选择普通导弹/SP1/SP2。原切换键循环三者并跳过耗尽 SP。F8（手柄 Back + Y）只管理功能、语言、键位、比例和日志，修改后点击保存。任务中打开 F8 会暂停。

双 SP 默认各用完整载量的 50%，先计入已装部件；SP2 为空时 SP1 使用完整载量。当前候选滑块允许 50% 至 100%、5% 一档。比例只作用于下一份新草稿，不重算当前任务弹药。名称未就绪时亮度为正常的 35%；中央简称固定绿色、字号略放大且可关闭；右下 HUD 固定启用，两者服从全局显隐。

游戏目录：`E:\SteamLibrary\steamapps\common\ACE COMBAT 7`。配置与日志：`ue4ss/Mods/DualSP/` 下的 `profiles.json`、`mod.log`、`native.log`。本包不提供个人配置或存档。

## 已知限制

- 1.0.1 已调整补给 HUD 接管时序，用户最终反馈正常，但不代表所有补给边界都已覆盖。原反馈保留于[问题记录](evidence/1.0.0-post-resupply-hud-flash.json)。
- 单人战役和自由任务是目标；VR、多人、第三方机体及其他武器修改不保证兼容。
- 1.0.1 候选在多人禁用功能，但加载器仍存在，不代表联网或反作弊兼容保证。中文/英文操作提示跟随游戏，F8 默认 Game Language，仍可手动选择；管理参数第 2 版首次迁移按用户要求设为跟随游戏，之后保留手动选择。
- UAV、ECM 不支持双挂。当前只显示一种 SP 模型，普通导弹状态保留上一 SP 挂载。
- 切换离开发射中的 TLS 会结束光束。鼠标选择未接入，请用键盘或手柄。
- 长时间、多次补给、检查点及特殊任务仍需更广覆盖。性能优化通过用户复测，但没有可报告的统一帧率提升数字。
- 音质卸载对照未证实模组导致下降，保留[音质排查](docs/research/audio-quality-audit.md)，不把猜测写成结论。

## 文档导航

| 内容 | 入口 |
|---|---|
| 当前 1.0.1 功能与验证范围 | [当前版本说明](docs/version-1.0.1.md) |
| 新手复制安装、配置、卸载 | [TXT 说明](docs/AC7DualSP-安装说明.txt) |
| 1.0.0 发布与验证范围 | [版本说明](docs/version-1.0.0.md) |
| 模块、数据流与生命周期 | [架构](docs/architecture.md) |
| 构建、部署、打包与 Git | [开发](docs/development.md) |
| F8 与直接选择 | [管理说明](docs/mod-management.md) |
| 性能、中央颜色与补给接入 | [性能修订](docs/performance-polish.md) |
| 旧版本导航及本轮清理 | [历史与踩坑索引](docs/history-1.0.0.md) |
| 武器、载量、模型、HUD、音效研究 | [核心研究](docs/research/core-findings.md)、[弹药](docs/research/sp-ammunition.md)、[模型](docs/research/weapon-models.md)、[中央 HUD](docs/research/persistent-weapon-message.md)、[音效](docs/research/weapon-switch-audio.md) |
| 决策、证据与参考 | [关键决策](docs/decisions.md)、[证据目录](evidence/README.md)、[参考资料](references/README.md) |

## 目录与版本边界

- `src/`：C++、管理界面、部署/发布及研究工具。
- `dist/DualSP/Scripts/`：运行时 Lua 的唯一源码位置，不另建重复调度脚本。
- `dist/DualSP/cores/0.1.9/DualSP.dll`：1.0.0 复用的已实测核心，纳入 Git；路径表示来源版本，不是安装版本。
- `dist/DualSP/cores/1.0.1/DualSP.dll`：当前正式核心，与实测候选一致；candidate.1 目录保留历史，不与旧 UI/Lua 混装。
- `dist/DualSP/bin/DualSPUI.dll`：当前管理界面。该目录的 `DualSP.dll` 则是受保护的旧 0.1.7 核心，禁止直接与当前 Lua 混装。
- `payload/`：专用加载器、许可及旧恢复载荷。1.0.0 打包器只从中取加载器/配置/许可，不使用其旧 Lua/DLL 副本。
- `docs/`、`evidence/`、`references/`：当前说明、历史研究、精简证据与参考。
- `tests/`：历史回归源码，按对应版本理解，不自动新增或运行。
- `tools/`：本机工具和发布运行库；`Temp/`、`backup/`：临时产物与备份，均排除 Git。
- `Release/`：当前 ZIP 与旧 0.1.7 恢复包；ZIP 不入 Git，源码、DLL 与发布核验记录入 Git。

## 后续协作规则

1. 交流、文档、新增代码注释使用简洁中文。函数、变量和关键逻辑应有必要注释；注重可读性，避免过度设计。
2. 主动指出不合理要求与遗漏。资料不足或环境/网络阻塞时及时求助，不反复盲试；禁止吞错、静默回退和补丁式掩盖问题。
3. 修改功能同步更新说明，区分用户实测、历史实验、离线推断与未验证内容。
4. 默认不自动编译、编写测试模块或运行测试。2026-10-14 用户阶段授权：每次修改完成可编译、核对语法、安装并停止游戏，无需再确认；允许现场研究和菜单操作，但开始任务由用户完成、不发射武器。未授权自动新增或运行测试模块。
5. F8 仅管理功能/参数/键位/日志；原生接入失败明确报告，不切换到其他兵装选择流程。
6. 旧核心 `dist/DualSP/bin/DualSP.dll` 的 SHA256 必须保持 `D7D7055CA7909021EEACA15E63FF5FF253D2EB7E60A3FE3B79DCEEAB231DE339`。新核心只能先构建为独立候选。
7. 优先 PowerShell 7，复杂文件处理使用 Python，新文件 UTF-8，Windows 启动脚本沿用 UTF-8 BOM。临时文件放 Temp。
8. 使用 Git 保护变更；无 Git 的既有文件修改前备份至 backup。清理送回收站，不永久删除。临时和备份目录用 .gitignore/.tfignore 排除。
9. 不提交原始游戏资源、个人配置/存档、工具及临时产物；不自动推送或公开发布。本次 ZIP 仅整理本地交付，原 SP2 音频不入 Git。
10. 允许 subAgent；各代理必须遵守同一文件所有权与上述边界。
