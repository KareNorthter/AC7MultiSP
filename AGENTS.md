# AC7DualSP 项目入口

正式名称 **AC7DualSP**，当前版本 **1.0.0**。面向 Steam 版《皇牌空战 7》build **9855922**：保留普通导弹，同时选择和切换两种特殊兵装，分别保存弹药与冷却。

## 当前状态

2026-10-14，用户确认性能与显示候选测试没有问题，要求整理为 1.0.0。1.0.0 沿用该运行逻辑，只统一名称、版本和管理窗口标题，整理文档与独立发布包。用户实测可用不等于所有机型、任务和检查点均已覆盖。

当前发布与验证范围见[1.0.0 版本说明](docs/version-1.0.0.md)，用户原话见[验证记录](evidence/1.0.0-user-validation.json)。性能修订依据见[性能与显示](docs/performance-polish.md)。此前候选状态属于历史，不是当前安装指引。

## 安装与使用

新手请看发布包内的[TXT 安装说明](docs/AC7DualSP-安装说明.txt)，无需命令行。发布包为 `Release/AC7DualSP-1.0.0.zip`：提供可直接复制的游戏文件、专用加载器、SP2 音频、VC++ x64 和 DirectX 离线安装文件。

退出游戏并备份既有模组，将包内 `GameFiles` 的内容复制到含 `Ace7Game.exe` 的游戏根目录。已有其他加载器时不要直接覆盖。内部目录及启用行仍是 `DualSP` / `DualSP : 1`，确保旧配置兼容。

准备页依次选择 SP1、SP2，每步确认一次后进入 STANDBY。SP2 可选 NO WEAPON。中途补给双选后进入 YES/NO，默认 NO；NO 从 SP1 重选，返回回 SP2，YES 确认起飞。补给成功后才保存和应用。

键盘默认 1/2/3、手柄十字键左/上/右直接选择普通导弹/SP1/SP2。原切换键循环三者并跳过耗尽 SP。F8（手柄 Back + Y）只管理功能、语言、键位、比例和日志，修改后点击保存。任务中打开 F8 会暂停。

双 SP 默认各用完整载量的 50%，先计入已装部件；SP2 为空时 SP1 使用完整载量。比例只作用于下一份新草稿，不重算当前任务弹药。名称未就绪时亮度为正常的 35%；中央简称常驻、固定绿色且字号略放大，两个 HUD 功能各有开关并服从全局显隐。

游戏目录：`E:\SteamLibrary\steamapps\common\ACE COMBAT 7`。配置与日志：`ue4ss/Mods/DualSP/` 下的 `profiles.json`、`mod.log`、`native.log`。本包不提供个人配置或存档。

## 已知限制

- 单人战役和自由任务是目标；VR、多人、第三方机体及其他武器修改不保证兼容。
- UAV、ECM 不支持双挂。当前只显示一种 SP 模型，普通导弹状态保留上一 SP 挂载。
- 切换离开发射中的 TLS 会结束光束。鼠标选择未接入，请用键盘或手柄。
- 长时间、多次补给、检查点及特殊任务仍需更广覆盖。性能优化通过用户复测，但没有可报告的统一帧率提升数字。
- 音质卸载对照未证实模组导致下降，保留[音质排查](docs/research/audio-quality-audit.md)，不把猜测写成结论。

## 文档导航

| 内容 | 入口 |
|---|---|
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
