# AC7 MultiSP 项目入口

本文是项目唯一总入口，统一提供使用说明、安装方式、研究导航和开发规则。目标是在 Steam 版《皇牌空战 7》中保留普通导弹，同时选择、切换两种特殊兵装，分别保存弹药和冷却，逐步接近用户提供的 AC8 交互体验。

## 当前状态

- **2026-10-14 双 SP 实体挂载研究分支。** 本工作区从 0.1.9 建立 `codex/su30sm-dual-mount-research`，仅研究 Su-30SM 双挂，不改变主工作区或已安装模组。已解析该机三套挂点，优先研究 6AAM＋QAAM；完整双组构建及发射仍待验证，见[本轮报告](docs/research/su30sm-dual-mount.md)。下列安装状态继续作为可玩基线。

- **2026-10-14 17:08 已安装 DualSP 0.1.9。** 用户确认候选 9 除 SP2 音量外其他功能正常；本版仅把音量系数从 0.35 降至 0.175，并统一版本标记。独立核心编译、语法和安装回读通过，配置/存档保留；本轮整理本地 Git 快照，最终音量待试听。见[安装回执](evidence/0.1.9-installed.json)。见[版本说明](docs/version-0.1.9.md)、[用户验证](evidence/0.1.9-user-validation.json)。以下候选状态为历史记录。

- **2026-10-14 16:19 已安装体验候选 9。** 用户确认候选 8 的 TLS 及其他调整正常；本批将 SP2 增益从 0.70 降至 0.35，准备页为三行全名与标题留白，性能/类型整体下移。编译、语法和安装回读通过，配置/存档保留；新的音量与布局待复测，见[安装回执](evidence/experience-candidate.9-installed.json)。见[说明](docs/experience-polish.md)、[依据](evidence/experience-candidate9-findings.json)。

- **2026-10-14 15:42 已安装体验候选 8。** 候选 7 的 TLS 收尾错误限定了 LaserWeapon 的 Tick，实际玩家 TLS 继承 AttachedInstantWeapon，导致发射后切换抛错并停止模组调度。已改为验证共用的三个收尾接口，保留玩家归属与 TLS 编号检查；编译、语法和安装回读通过，实际发射后切换待用户复测，见[安装回执](evidence/experience-candidate.8-installed.json)。见[说明](docs/experience-polish.md)、[依据](evidence/experience-candidate8-findings.json)。

- **2026-10-14 15:26 已安装体验候选 7。** 用户确认候选 6 主要功能正常，并报告排版、PART 单 SP、TLS 残留及 MSL 挂载变化。本批调整未就绪系数 35%、SP2 音量降低 30%、准备/补给排版、PART 双 SP、MSL 保留挂载及 TLS 原生收尾。构建、语法及安装回读通过，游戏已按授权停止，配置/存档保留；新增行为等待实玩。见[说明](docs/experience-polish.md)、[依据](evidence/experience-candidate7-findings.json)、[回执](evidence/experience-candidate.7-installed.json)。

- **2026-10-14 14:34 已安装体验候选 6。** 用户重新授权本轮完成后安装，游戏已按授权停止。准备页保留右下弹药汇总、增加全名前原生彩色分类图标；补给页用英文标签、原生绿色泛光与完整中文介绍；三页载量计入部件并保留现有比例。一并包含用户指定的 SP2.wav。构建、离线核验及安装回读通过，配置、存档、正式核心和恢复载荷保留，见[候选 6 回执](evidence/experience-candidate.6-installed.json)。本轮新显示和音色等待用户实玩；见[弹药说明](docs/research/sp-ammunition.md)、[体验说明](docs/experience-polish.md)。下列“当前”均属各历史批次记录。

- **2026-10-14 12:47 当前安装 0.1.9-experience-candidate.4**：修正 CFA-44/ADMM 的动态槽池容量误判，补上机库 SP1 和补给双选确认音，未就绪名称亮度系数降至 25%，补给弹药标题去掉“预计”。编译和安装核验通过，修复后实玩待复核；见[修订说明](docs/experience-polish.md)、[研究证据](evidence/experience-candidate4-findings.json)、[安装回执](evidence/experience-candidate.4-installed.json)。用户已确认候选 3 外观正常；新的补给 UI 风格改版因缺 AC7 原生截图暂缓。

- **2026-10-14 体验优化候选已安装**：已修改机库一步确认/返回、STANDBY 双 SP 信息、EMPTY 预览、HUD 提前接入、补给页接入时机、中央就绪透明度和 SP2 独立选择提示。**当前安装 0.1.9-experience-candidate.3，编译与安装核验通过，金色选中与英文布局已在 Su-30SM 机库现场核对，未出击**；详见[体验优化说明](docs/experience-polish.md)、[外观记录](evidence/experience-candidate3-ui.json)及[安装回执](evidence/experience-candidate.3-installed.json)。下文使用说明保留此前 0.1.8 的流程，新候选以体验优化说明为准。
- **候选 2 用户确认全部功能正常、未发现 BUG**，见[用户记录](evidence/experience-candidate2-user-validation.json)。候选 3 仅调整准备页金色选中、英文兵装信息与弹药标题，详见体验优化说明。
- **候选 1 用户复测**：中央装填透明度、武器切换/发射及补给双 SP/单 SP 正常；机库交接、STANDBY 退出闪回、首次开场 HUD 和 SP2 无声仍有问题。候选 2 已针对原因修订，并加入兵装光标预览联动；见[修订依据](evidence/experience-candidate2-revision.json)。
- **候选 2 机库现场通过**：Su-30SM 的 6AAM/LAGM 一次确认直接进入 STANDBY，重进兵装页后一次返回正常，已保存方案保留；SP1/SP2 背景模型特写随光标变化。代理当时停在 STANDBY；随后用户已确认全部功能正常。
- 此前初步实玩通过的保留版本：**DualSP 0.1.8**，支持已核对的 Steam build **9855922**，由管理候选 7 整理而来。
- **2026-10-14 用户确认：0.1.8 中途补给正常，该版本全部功能初步可用。** 最新日志包含补给结束确认、任务成功、双 SP 方案应用和恢复后的武器切换，本次运行未见错误。更大范围、长时间、多次补给及检查点覆盖仍待用户测试，不能据此宣称完整稳定。见[版本说明](docs/version-0.1.8.md)和[用户验证记录](evidence/0.1.8-user-validation.json)。
- 原生出击/补给双 SP 选择、YES/NO、HUD、直接选择及 F8 管理成套保留。持续锁定声本次未再出现，LAGM 锁距此前复测暂未再见异常。
- 0.1.8 沿用候选 7 的运行逻辑和候选 6 的两枚 DLL，仅更新版本标记。0.1.8 整理时未重新编译 DLL、未新增或运行测试模块；当时已按用户要求关闭游戏。
- 正式 0.1.7 核心、旧发行包及恢复载荷继续保留。0.1.8 实际使用的独立核心保存于 `dist/DualSP/cores/0.1.8/DualSP.dll` 并纳入 Git，避免提交后依赖被忽略的构建目录。
- 开场 HUD 提前接入和独立 SP2 选择音已由用户确认正常；双模型暂缓，未新增自制音频；暂停后退出曾出现 Fatal error，尚未获得可归因调用栈。其余边界见下文[已知限制](#已知限制)。

## 0.1.9 操作概览

键盘默认 1/2/3、手柄十字键左/上/右直接选择 MSL/SP1/SP2。原游戏切换键循环 MSL → SP1 → SP2，跳过耗尽 SP；切回 MSL 保持上一种 SP 挂载。F8 只管理设置，不选择兵装。详情见[管理说明](docs/mod-management.md)。

## 使用说明

准备页先选择 SP1，再选择 SP2；SP2 可选 NO WEAPON。每步一次确认，随后直接进入 STANDBY，不增加原生单 SP 页。完整名称、分类图标、性能和类型分区展示，弹药只在右下汇总。STANDBY、PART 显示已保存双 SP，PART 载量计入已装部件，不模拟仅悬停的未装部件。

中途补给先选 SP1、SP2，再进入起飞 YES/NO；初始选中 NO。NO 回 SP1 重选，返回键回 SP2，YES 一次确认起飞；只有补给成功后才应用和保存。补给保留绿色原生风格和中文介绍。

双 SP 默认各使用原生完整载量的 50%，先计入适用部件，再按比例取整且至少一发；SP2 为空时 SP1 保留完整载量。F8 可改下一份新草稿的全局比例，不重算当前任务弹药。准备、STANDBY、PART、补给使用同一预览规则，详见[弹药说明](docs/research/sp-ammunition.md)。

右下和中央名称未就绪时使用正常亮度的 35%，至少一发就绪后恢复。两个显示功能在 F8 中独立开关并服从全局 HUD 显隐。SP2 提示音使用本地 WAV，0.1.9 增益为游戏音效音量的 0.175 倍；MSL/SP1 保留原生音效。切换离开发射中的 TLS 会结束光束，用户已认可这一行为。

F8 支持中英文、功能开关、键盘单键设置、配弹比例和日志。任务中打开 F8 会暂停，手柄可用 Back + Y 打开；改动需点击保存。方案按机体保存，出击后锁定。

## 安装与恢复

本机仅支持已核对的 Steam build **9855922**。当前版本安装入口见下一段；[旧 0.1.7 安装包](Release/DualSP-0.1.7.zip)及根目录 `Install.cmd` 仅用于恢复旧版。卸载入口为 `Uninstall.cmd`，不会直接删除配置。

当前版本为 **0.1.9**。游戏退出后运行 `python -X utf8 src/install-resupply-candidate.py --experience`，从 Git 保留的 `dist/DualSP/cores/0.1.9/DualSP.dll` 安装整套 Lua、UI 和本地 SP2.wav，保留配置并备份旧模组。原音频不入库，恢复源码后需按 [Audio 说明](dist/DualSP/Audio/README.md)补齐；缺失时安装器会停止。版本细节见[0.1.9](docs/version-0.1.9.md)。

**禁止混用旧核心与当前 Lua。** 根目录 Install.cmd 和旧 ZIP 只恢复 0.1.7；恢复 0.1.8 须切回配套历史源码后使用 --management。本轮不生成或公开发布新安装包。

日志与配置位于游戏目录 `ue4ss/Mods/DualSP/`：`mod.log`、`native.log`、`profiles.json`。兵装错误会停止武器修改；HUD/准备页接入错误会明确显示，F8 可查看原因。开发资料整理不会修改已安装模组或存档。

从 Git 恢复 0.1.8 后使用上述管理安装器，不要把旧正式核心与当前 Lua 混装。正式发行载荷目前仍为 0.1.7；发布新安装包需另行整理，见[开发说明](docs/development.md)。

## 已知限制

- 一步交接、兵装光标模型特写联动已由用户实测确认。鼠标点选和说明视频未接入；原生选择请用键盘或手柄，F8 不选兵装。
- 检查点、自然补给及部分特殊武器的完整覆盖仍属实验性。UAV/ECM 不支持双挂。
- 当前飞机只显示一种 SP 挂载，MSL 状态保留上一 SP 模型。双模型目标[暂缓保留](docs/research/weapon-models.md)。
- 单机战役和自由任务是目标范围；VR、多人、第三方新增机体及其他武器修改未作兼容保证。52 行原生机体数据的支持不等于全部机体实测通过。

## 文档导航

| 需要了解什么 | 入口 |
|---|---|
| 当前 0.1.9 的范围、安装和验证边界 | [版本说明](docs/version-0.1.9.md) |
| 本轮体验优化的源码、研究依据和未验证项 | [体验优化候选](docs/experience-polish.md) |
| 0.1.8 的版本范围和验证边界 | [版本说明](docs/version-0.1.8.md) |
| 安装、使用、已知限制 | 本文的[使用说明](#使用说明)、[安装与恢复](#安装与恢复)、[已知限制](#已知限制) |
| 模块划分、数据流、关键约束 | [架构说明](docs/architecture.md) |
| F8 管理、中英文与直接选武器 | [管理说明](docs/mod-management.md) |
| 本机依赖、构建与发布方式、Git 规则 | [开发说明](docs/development.md) |
| 原生任务准备双选的流程与边界 | [原生选择页](docs/native-loadout.md) |
| 中途降落、双 SP 重选与补给候选 | [原生补给说明](docs/native-resupply.md) |
| 补给正常起飞后模组停用 | [候选 7 阶段身份修订](docs/resupply-completion-revision.md) |
| 补给跳过确认、模组停用与持续锁定音 | [候选 6 输入与音效修订](docs/input-audio-revision.md) |
| 补给黑屏与 LAGM 锁距的历史修订 | [候选 5 原生流程修订](docs/native-flow-revision.md) |
| AC8 补给截图、双栏与起飞 YES/NO 对照 | [补给对照](docs/resupply-ac8-comparison.md) |
| 弹药、冷却、生命周期研究结论 | [武器核心研究](docs/research/core-findings.md) |
| 武器装填或耗尽时名称半透明的可行性 | [HUD 就绪状态评估](docs/research/hud-readiness.md) |
| 中央当前武器简称正常亮度常驻的可行性 | [中央武器提示评估](docs/research/persistent-weapon-message.md) |
| MSL/SP1/SP2 选择音效与暂停的研究 | [音效研究](docs/research/weapon-switch-audio.md) |
| 补给重选与两个 HUD 新功能的合并候选 | [合并候选说明](docs/hud-candidate.md) |
| 双模型挂载可行性与后续目标 | [模型研究](docs/research/weapon-models.md) |
| Su-30SM 挂点、首个组合与持久分组门槛 | [Su-30SM 双挂研究](docs/research/su30sm-dual-mount.md) |
| 为什么采用当前方案 | [关键决策](docs/decisions.md) |
| 保留了哪些证据 | [证据目录](evidence/README.md) |
| AC8 参考图 | [参考资料](references/README.md) |
| 本轮迁移及清理记录 | [资产整理记录](docs/migration.md) |

## 目录与事实来源

- `src/`：当前 C++ 核心、F8 界面、资源研究与发布脚本。
- `dist/DualSP/`：当前 Lua、受保护的 0.1.7 核心、`cores/0.1.8/`、`cores/0.1.9/` 独立核心与当前界面 DLL，**运行时 Lua 的唯一源码位置**；不要另建一份同名调度脚本。
- `payload/`：安装所需的专用 UE4SS、许可证及模组载荷；模组副本由 `dist` 同步，不重复纳入 Git。
- `docs/`、`evidence/`、`references/`：说明、精简证据和参考图。
- `tests/`：既有回归检查，仅在用户要求时使用，不自动新增或运行。
- `tools/`：本机便携依赖，见[工具清单](tools/README.md)，工具二进制不进入 Git。
- `Release/`：保留旧 0.1.7 发行包与恢复基线说明；Git 从当前版本开始，不导入早期版本。
- `Temp/`、`backup/`：临时文件与备份，均被 `.gitignore/.tfignore` 排除。

游戏安装仍位于 `E:\SteamLibrary\steamapps\common\ACE COMBAT 7`。开发资料迁移不等于迁移游戏；存档、游戏内模组与配置不随开发目录移动。

## 后续协作规则

1. 交流、新增代码注释和文档使用简洁中文。函数、变量和关键逻辑需有必要注释，避免过度设计。
2. 修改功能时同步更新相关说明，并保持上述文档互相引用。结论必须区分用户实测、历史实验、离线推断和未验证内容。
3. **除非用户明确要求，不自动编译、编写测试模块或运行测试。** 默认不打开或游玩 AC7；2026-10-13 本轮用户明确授权代理启动并操作游戏，验证自由任务的机体、双 SP 选择及出击，不测试切换和发射。此授权仅用于本轮准备页调试。
   **2026-10-14 本阶段新授权：每次改动完成后可立即编译、进行安装器语法核对并安装，也允许停止游戏进程，无需再次确认。允许现场研究和菜单操作；开始任务仍由用户完成，不发射武器。未授权自动新增或运行测试模块。**
4. 不掩盖失败，不静默回退。F8 按用户最新要求只管理功能、参数、键位和日志，不代替原生兵装选择；原生接入失败必须报告，不自动换用另一套选择流程。
5. 保护正式武器核心：`dist/DualSP/bin/DualSP.dll` 的 SHA256 必须保持 `D7D7055CA7909021EEACA15E63FF5FF253D2EB7E60A3FE3B79DCEEAB231DE339`。重新编译的核心只能先作为独立候选。
6. 优先 PowerShell 7；复杂文件操作用 Python，临时文件放 `Temp/`，默认 UTF-8。Windows 启动脚本沿用带 BOM 的 UTF-8。
7. 使用 Git 管理变更。无 Git 保护的既有文件修改前放入 `backup/`；清理采用回收站，不永久删除。遇到资料不足或环境瓶颈及时求助，不反复盲试。
8. 不将原始游戏资源、个人存档、工具依赖及临时产物提交到 Git。版本库目前仅在本机，不自动推送或公开发布。
