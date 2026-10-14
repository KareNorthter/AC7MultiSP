# 开发与发布

> 当前 [0.1.8](version-0.1.8.md) 为用户初步实玩通过的本地版本。`--management` 从 VERSION 读取版本，使用已纳入 Git 的 `dist/DualSP/cores/0.1.8/DualSP.dll`；不依赖 build-core。旧正式载荷、Install.cmd 和旧 ZIP 保留为 0.1.7 恢复入口；打包脚本阻止误标旧载荷，本轮不打包、不编译、不运行测试。后文为候选开发历史。

> 候选 7 仅修改 Lua 的补给阶段身份校验，沿用候选 6 已授权构建的核心和 UI，哈希一致。已于 2026-10-14T08:50:13.678319+08:00 安装；本轮未编译 DLL、未新增或运行测试模块。安装语法核验只 load 脚本，不执行。详见[修订](resupply-completion-revision.md)。

返回[项目入口](../AGENTS.md)。代码结构见[架构](architecture.md)，资产整理经过见[迁移记录](migration.md)。

根目录仅保留 `AGENTS.md`，统一承载使用说明和开发导航；发布脚本也只复制这个总入口。各子目录的 `README.md` 是局部索引或第三方说明，不作为第二个项目总入口。

## 当前基线

候选 5 源码在 `codex/resupply-native-flow` 分支修复补给 YES 的原生调用链，以及 Lua/核心切换后的锁定组件重绑。它新增 `dualsp_resupply_takeoff` 导出，安装器会拒绝旧核心混装；需要单独构建 `build-core/DualSP.dll`，不重编译 UI、不覆盖正式核心。用户明确授权后已于 23:44 构建并成套安装，未运行测试模块。候选 5 的 Lua 语法及十运行文件核验通过；依据及边界见[本轮修订](native-flow-revision.md)。

项目根目录为 `E:\AC7_MultiSP`，正式发布基线仍为 **0.1.7**。2026-10-13 已按用户安装要求构建独立补给候选核心，并将候选 Lua 和该核心安装到游戏副本，标记为 `0.1.7-resupply-candidate.1`；未运行测试模块或启动游戏。项目正式 DLL、发行包和正式恢复载荷保持原基线，不能把 `dist` 中的正式 DLL 与当前 Lua 直接混装。目录整理本身没有重新编译。

随后按用户要求在 `codex/hud-merged-candidate` 分支合并两个 HUD 功能，已构建并成套安装 **0.1.7-merged-candidate.1**。本轮包含十个运行文件，八个 Lua 文件只编译语法，安装核验通过，未执行测试模块或启动游戏。当前来源和恢复位置见[合并候选说明](hud-candidate.md)。

用户实玩发现出击选择初始化错误后，2026-10-13 12:39 已成套安装 **0.1.7-merged-candidate.2**。此次只修复 Lua 中的旧引擎接口误用，未重新编译核心或 UI。安装增加声明 API 名称的游戏映像核对；它不能代替真实反射绑定及用户实玩验收，记录见[诊断](../evidence/native-ui-api-failure.json)和[当前安装核验](../evidence/merged-candidate.2-installed.json)。

工具位置见[工具清单](../tools/README.md)。LLVM-MinGW 默认从项目 `tools/llvm-mingw-20261006-ucrt-x86_64` 读取，也可通过 `DUALSP_TOOLCHAIN` 指定。Python、PowerShell 7、Git 沿用本机安装，游戏仍在 Steam 库中，不随项目移动。

本机随后更新为 **0.1.7-merged-candidate.3**：根据用户复测修复准备页槽类型识别，并将右下箭头及中央简称的更新放到逻辑选择完成后。沿用现有两枚 DLL，不重新编译或运行测试；补给仍待用户实玩。诊断见[槽与切换记录](../evidence/native-ui-slot-switch-findings.json)，最新安装见[候选 3 安装核验](../evidence/merged-candidate.3-installed.json)。

2026-10-13 14:30 已按用户要求构建新版 F8 并统一安装 **0.1.7-management-candidate.1**：取消 F9 和 F8 兵装编辑，提供直接选择、中英文管理、独立中央开关及日志。首轮编译发现遗漏状态同步函数，已恢复后成功构建；候选武器核心未重新编译。八个 Lua 文件仅编译语法，十个运行文件及安装保护核验通过，未运行测试模块或启动游戏。见[管理说明](mod-management.md)及[安装证据](../evidence/management-candidate-installed.json)。

2026-10-13 15:52 已统一安装 **0.1.7-management-candidate.2**：补给双栏信息与 YES/NO 起飞重选；只修改 Lua，两枚 DLL 未重新编译。资源/截图与无参数原生入口已只读核对，八 Lua 语法和十个运行文件安装核验通过，未运行测试或启动游戏。见[AC8 对照](resupply-ac8-comparison.md)及[安装证据](../evidence/management-candidate.2-installed.json)。

2026-10-13 16:43 已按用户“修复并编译安装”要求安装 **0.1.7-management-candidate.3**：准备双栏、确认页分别编辑 SP1/SP2、明确继续/取消交接、原生页面激活清除关闭标记，以及可见父容器的高亮修复。运行逻辑为 Lua 修改；用 `src/build-ui.cmd` 重新编译既有管理 UI 成功，没有重新编译武器核心。八 Lua 语法、十运行文件、旧模组备份、配置/存档/正式载荷核验通过，未运行测试模块或启动游戏。其他补给聊天已完成，其功能随整套安装保留。说明见[准备页](native-loadout.md)，证据见[复核](../evidence/preparation-review-findings.json)及[当前安装](../evidence/management-candidate.3-installed.json)。

## 操作规则

只有用户明确要求构建、验证或发布时才执行相关命令。以下是现有入口的用途，**不是默认自动执行流程**：

| 入口 | 用途 |
|---|---|
| `src/build-ui.cmd` | 编译 F8 UI DLL |
| `src/build-core-portable.cmd` | 生成 `build-core/DualSP.dll` 独立候选，不覆盖正式核心 |
| `src/build.cmd` | 执行上述两个构建入口 |
| `src/prepare-payload.ps1` | 核对正式核心 hash，同步八个运行文件；当前补给源码需要候选协议，会明确停止旧核心混装 |
| `src/package.ps1` | 生成带文档和许可的发布目录、安装包及源码包；不覆盖现有快照 |
| `src/update-local.ps1` | 在游戏退出后更新本机安装，并核对配置、存档和运行文件 |
| `src/install-resupply-candidate.py` | 将 `build-core` 独立候选与当前 Lua 成套安装到已有游戏副本；完整备份原模组，核对配置和保护文件 |
| `src/install-resupply-candidate.py --management` | 安装当前 0.1.8；从 Git 管理的独立核心及 Lua 成套部署，沿用现有 DLL，不自动构建 |
| `src/install-resupply-candidate.py --merged-hud` | 合并安装补给重选、就绪名称变淡与中央简称常驻，核对十个运行文件及专用 Lua 转发接口 |
| `tests/` | 保留的当前回归源码；使用方式见[现有检查说明](../tests/README.md) |

资源研究工具 `src/inspect_*` 和 `decrypt_hud_for_inspection.py` 只针对用户自有游戏；原始资源副本和解析中间结果放 `Temp/`，不提交。现场读进程工具只在用户明确授权相应研究时调用，不能用当前旧证据中的绝对地址继续操作新进程。

补给候选的构建、安装与实玩边界见[原生补给说明](native-resupply.md)。`src/inspect_resupply_assets.py` 使用 `tools/dotnet8` 的本地运行库执行既有 UAssetGUI，只读解析页面并更新结构摘要。依赖来源和安装位置见[工具清单](../tools/README.md)。

## Git 与发布产物

- 仓库仅在本机，默认分支 `main`，初始标签为 `baseline-0.1.7`。不导入早期版本，也不自动推送。
- 当前源码、正式 DLL、专用加载器、第三方源码与许可、文档、精选证据和参考图受 Git 管理。
- `Temp/`、`backup/`、便携工具、Python 依赖、构建产物、安装载荷的重复模组副本与发行 ZIP 不入库。
- `Release/DualSP-0.1.7.zip` 是迁移前已经发布的原始安装包，按 hash 保留，不把其中旧文档当作当前开发入口。当前说明以 [AGENTS.md](../AGENTS.md) 为准。
- 变更功能时同步文档；准备下一版本时统一更新版本标记及说明，再按用户要求构建和发布。正式核心替换必须有独立实玩依据。

本机迁移后的安装载荷已保留，根目录安装入口可以直接使用。若以后从 Git 新建副本或修改 `dist`，需要在用户要求安装或发布时先运行 `src/prepare-payload.ps1`，从受版本管理的正式运行文件生成对应载荷，再安装或打包。便携工具不在 Git 中，应从本机现有工具目录补齐，来源见[工具清单](../tools/README.md)。

现有代码中保留部分历史英文注释以避免本轮扩大功能改动。后续新增或修改的注释、函数与关键变量说明应使用中文。

## 管理候选 4 更新

管理候选 4 于 2026-10-13T19:58:41.393239+08:00 成套安装。按用户授权重新构建 UI DLL（只读键盘导航）及独立候选核心（HUD 接受原生无限弹药 -1）；正式核心与恢复载荷未改。八 Lua 仅语法编译，十运行文件和保护核验通过。代理实际操作准备和出击，范围见[实机记录](../evidence/preparation-live-validation.json)。没有新增或运行测试模块。
