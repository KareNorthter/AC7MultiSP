# AC7DualSP 项目入口

正式名称 **AC7DualSP**，当前正式版本 **1.0.1**。面向 Steam 版《皇牌空战 7》build **9855922**：保留普通导弹，同时选择和切换两种特殊兵装，分别保存弹药与冷却。现有 ZIP 仍为 1.0.0，本轮按用户要求不打包。

## 当前状态

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
