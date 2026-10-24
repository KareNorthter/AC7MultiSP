# 快速进入测试任务设计

状态：alpha2已完整构建多SP与F6键盘修订；本轮接收bbf0984/4fb9fd0的皮肤功能，保留alpha2扁平三SP预设，规范写回schema3。另修订补给兵装说明的翻译多返回值报错，已从2acf8de完成[2.25.3 alpha3完整构建与封装](../../evidence/2.25.3-alpha3-exe-delivery.json)，未安装或实机验证。来源与边界见[本轮记录](../../evidence/2.25.3-alpha3-skin-resupply-source.json)，正式状态见[项目入口](../../README.md)。

## 目标与入口

用户希望参考任务内换机，指定机体、SP和任务，一键进入M1—M20及三个DLC特殊任务，跳过选任务、选机、选装交互和CG，用于快速测试。优先游戏启动后直接进入；若原生启动条件不允许，至少支持主菜单F6管理页启动。

从main `85ab3a0` 建立 `codex/feat-quick-mission-launch`，工作树为 `C:/Users/83717/.codex/worktrees/quick-mission-launch/AC7_MultiSP`。开发前已合入main `5a33e3a`，合并锚 `ab02cc4`。本任务维护快速Lua、F6接入、独立预设、脚本清单、对应双语资源和本文，不改版本号或安装器，不覆盖旧核心。接收前仍须比较届时main。

## 当前源码交互

在F6“调试 / Debug”页增加独立的“快速进入任务 / Quick mission launch”区域：

| 控件 | 设计 |
|---|---|
| 任务 / Mission | M1—M20、DLC SP Mission 1—3；名称、任务ID、地图、DLC标记从原生任务表读取，不按编号规律猜测 |
| 机体 / Aircraft | 复用换机候选目录及搜索；核对完整兵装资料和真实类引用，原生退让机体不强行接管 |
| 特殊兵装 / Special weapons | 勾选目标机体1至3种SP，按原生配置顺序连续填入逻辑槽；空选不能保存或出击，真实多挂资格在任务玩家就绪后重新核对 |
| 难度 / Difficulty | 简单、普通、困难、王牌，默认普通；原生难度setter已确认 |
| 跳过片头Logo / Skip startup logos | 默认开启，独立预设保存；F6更改后下次启动生效，Mod总开关需开启 |
| 进入任务 / Launch mission | 一次点击提交完整快照，关闭F6并启动；选择控件、普通“保存”均不出击 |
| 启动后自动进入 / Auto-launch on startup | 默认关闭；明确保存后启用。仅在原生单人初始化已完成的安全入口发出一次请求，不随返回主菜单反复启动 |
| 状态 / Status | 区分准备、已请求加载、等待玩家、兵装初始化、可操控、失败；不能在写入GI编号时显示成功 |

自动启动优先探索原生初始化完成后的直入路径；若只能在主菜单就绪后触发，界面必须如实说明，不能称已跳过标题、存档选择或所有启动交互。必须保留取消/停用入口，自动启动失败不无限重试。

所有新增按钮、说明、禁用原因、状态和错误均接入 `docs/user/interface/game.*.json`，由 `src/user_texts.py` 生成C++/Lua文字。玩家功能说明、更新日志中英已同步“开发中/未发布”范围。页面和语言切换尚未实测。

## 原生边界与待核实依据

现有换机在 `dist/DualSP/Scripts/debug_aircraft.lua` 通过 `SetPlaneAndSWP` 选择机体、`PlayerController:RestartLevel` 重开当前任务。它会核对新世界、本地受控Pawn、实际类和兵装初始化；临时方案不覆盖个人方案。这些事务约束可复用，但RestartLevel不能负责选择另一任务。现有机制见[架构](architecture.md#选装与调试换机)。

前期静态核对使用本机 `E:/AC7_MultiSP/Temp/experience-object-index.json`。索引是单行数组，可按 `outer/cls` 重建函数和属性；记录参数名称、类型，不包含完整启动调用链、参数偏移或任务表行数据。下表数组位置为零基，不能把子项排列顺序当函数参数顺序或ABI。本次实时读取另确认了GI函数参数偏移，见下节。

| 已核对候选 | 参数资料 | 能说明什么，尚缺什么 |
|---|---|---|
| `/Script/Nimbus.NimbusGameInstance:SetMissionID`；位置25633 | `MissionID:int` | 有任务选择setter；不能证明调用后任务系统已初始化 |
| `/Script/Nimbus.NimbusBlueprintLibrary:NimbusOpenLevel`；位置23992 | `WorldContextObject:UObject`、`LevelName:FName` | 有地图加载入口；调用顺序及出击前置状态待核对 |
| `/Script/Nimbus.NimbusPlayerController:SetIsSkipIGC`；位置26505 | `bSkipIGC:bool` | 有过场跳过候选；何时设置、哪些过场使用它、如何恢复尚不明确 |
| `/Script/Nimbus.Mission:SkipMiniGame`；位置8070 | 无参数 | 小游戏相关候选；不能据此声称可跳过全部CG，也不应默认跳过任务玩法 |
| `/Script/Nimbus.NimbusCheatManager:ForceMapChange`；位置24634 | `MapName:FString` | 调试地图切换候选；不直接采用未经核实的作弊启动路径 |
| `/Script/Nimbus.NimbusCheatManager:ToggleForceSkipIGC` | 无参数 | 开关型候选；当前值与恢复办法未知，不能重复调用当作“设置开启” |

该索引中的原生 `CampaignHangarSortieWidget`、`CampaignMissionLoadWidget` 类没有反射函数，`CampaignFreeMissionWidget` 只有属性；这些原生类的清单不能代替其蓝图流程。前期未确认完整出击入口；后续原生模式及最终加载请求的依据见下文“原生链与当前实现”，没有采用 `SetMissionID + OpenLevel` 拼接。

任务表实际对象为 `/Game/Blueprint/Information/MissionDataTable.MissionDataTable`，位置47365；结构为 `/Script/Nimbus.MissionDataTable`，已见 `MissionID`、`MissionStringID`、`StageNo`、`Category`、`Rule`、`DLCID`、`FreeFlightMissionID`、`NextMissionID`、`HangarPersistentLongName` 等字段。旧索引本身没有真实行值；2026-10-24已从当前进程取得72行，23个目标任务的实值见下节，不再以编号猜测作为依据。

现有 `runtime_context.lua` 的 `ms13=113`、同图自由飞行 `213` 与本次表行一致。任务和自由飞行必须分别识别，不能以地图相同混用规则；原生“自由任务”选择普通任务不等于“自由飞行”。DLC许可查询 `NimbusDLCInformation:IsDLCOwned(DLCID:string)->bool` 已有本机资料 `E:/AC7_MultiSP/Temp/plane-switch-native-research/ownership-query-supplement.json`；任务许可须用任务表自己的DLCID，不能套用机体DLC编号。

## 2026-10-24只读观察结论

用户自行操作普通任务M6和DLC特殊任务2。当前安装记录及加载日志为2.24.2；用户在普通任务出击前关闭了Mod总开关，因此本轮不是多SP初始化、快速启动或自动跳过CG的验收。[观察摘要](../../evidence/quick-mission-observation-20261024.json)保存目标任务行、关键日志、观测边界和本机原件摘要；完整日志、任务表、反射和代码片段仅留 `E:/AC7_MultiSP/Temp/tasks/20261024-quick-mission-observation/`，不入Git。

从本轮实际加载的任务表逐行核对：

| 目标 | MissionID | MissionStringID | DLCID | 对应自由飞行ID |
|---|---|---|---|---|
| M1—M20 | 101—120，每关对应关系已逐行读取 | ms01—ms20 | Base | 201—220 |
| DLC特殊任务1 | 2101 | ex01 | MISSION_C_001 | 2201 |
| DLC特殊任务2 | 2102 | ex02 | MISSION_C_002 | 2202 |
| DLC特殊任务3 | 2103 | ex03 | MISSION_C_003 | 2203 |

现场时间为北京时间：M6于07:28:49进入 `/Game/Maps/Transition/Transition`，07:29:07切至 `/Game/Maps/Mission/ms06/ms06`，画面随后显示START MISSION及飞行；现有鼠标飞控日志确认其实际机体为 `AcePlayerPawn_fa50_s00_C`、编号241。返回主菜单后，07:31:04再次进入过渡世界，07:32:38才切至 `/Game/Maps/Mission/ex02/ex02`。DLC2稳定飞行快照确认GI选择任务2102、机体235、SP配置0；真实机体 `AcePlayerPawn_wuav_s00_C` 的 `PlaneTypeID=235`、`bEnableWeaponInput=1`，并持有当前任务的 `NimbusPlayerControllerBP_C`。画面确认已实际飞行。

有两项直接影响启动事务：

- 过渡世界被现有运行许可标记为 `multiplayer_mode` 并关闭核心；这是本轮单人出击的实际过渡，不是用户进入多人。快速启动须在“本次已接受请求＋已核对单机过渡世界”下保留等待数据，同时保持核心禁用；不能直接复用换机中遇到该reason就取消的规则，也不能因此全局放开过渡/多人许可。
- 07:31:32已能只读看到ex02的MissionActor，07:32:38才有当前任务许可与实际玩家。加载中的目标对象存在不能作为世界交接完成依据，仍须核对GI当前世界、受控玩家和飞控输入。窗口最小化期间不采信旧画面；本轮耗时不能作为正常加载性能结论。

实时反射确认 `SetMissionID` 的int参数偏移0；`SetPlaneAndSWP` 的机体int参数偏移0、SP int参数偏移4。当前进程getter原码确认GI任务ID、机体ID、SP分别读 `+0x4d8/+0x4dc/+0x4e0`；观察脚本逐条核对getter签名后只读这些值，正式功能仍应使用反射接口，不能直接照抄偏移写入。

本轮只读轮询没有记录函数调用事件，因此未证实完整出击调用次序、单人/自由任务模式与难度的设置入口、完整地图选择规则或CG跳过契约。没有调用 `OpenLevel`、`SetIsSkipIGC`、`SkipMiniGame` 或其他游戏函数；不能把用户手动完成出击当成快速入口已可用。场景瞬时对象变化导致一段补定位停止，随后在稳定飞行状态取得新快照；只采信核对身份成功的记录。

## 启动事务设计

1. **预检。** 游戏线程重新确认本地单人、安全菜单、Mod实际启用、无补给/检查点/换机事务；核对任务行、DLC许可、目标机体和SP。F6暂借菜单输入时核对实际持有的页面，不把隐藏菜单当作可用入口。
2. **准备。** 保存本次请求快照和原生选择，仅在原世界尚存且未交接时允许回滚。归还F6输入、暂停和模型所有权，清理旧武器、检查点与调试身份；不修改存档购买记录，不把测试请求写成战役进度。
3. **启动。** 调用经核实的原生出击链，一次请求只发出一次加载。跳过选任务/选机页面应通过原生已确认的准备状态实现，不模拟持续按确认键，也不靠硬编码延时连续点击。
4. **过场。** 仅对本次启动范围内、已有明确跳过契约的CG应用原生跳过。不能全局停用剧情事件、时间轴或小游戏，防止漏掉生成玩家、阶段触发和任务目标；所有暂借状态有明确归还时机。
5. **确认。** 新世界和任务ID吻合，实际机体类与本地控制权吻合，再按真实兵装组件验证并应用临时SP方案。原生入口生成的选择不符就失败，不能以GI编号冒充实际机体/兵装。实际可操控且兵装就绪才报告成功。
6. **失败。** 超时、多人进入、游戏实例改变、其他任务/机体或原生拒绝均终止请求并说明原因，不自动重复旅行。只在原身份下回滚仍属本事务的选择；已换世界不能写回旧对象。失败不能把错误方案保存为个人方案。

渲染线程只展示状态并提交动作，游戏线程操作原生对象；快速启动使用独立子协议和请求号，避免与换机、普通管理保存、SP调试回执混用。若最终增加Lua模块，唯一源码仍放 `dist/DualSP/Scripts/`，并同步 `src/product.json` 的脚本清单；正式版本由整合任务确定。

## 特殊任务与限制

- **第13关：** 现行Mod使用任务指定SP并旁路多SP接管。用户目标包含M13，但“任意指定SP进入M13”目前没有依据，不能把保留任务兵装当成已实现指定SP。需先确认是否有保持任务脚本正常的方案；若最终仅支持任务指定SP，界面禁用自选SP并明确说明，仍标出该项与原目标的差距。
- **DLC三关：** 真实任务ID与DLC标记已取得，已实际观察ex02地图交接；ex01/ex03完整地图交接仍待核对。分别核对任务DLC许可，不购买、不伪造许可；未拥有或许可无法确认时明确禁用。
- **开场及阶段CG：** 跳过视觉播放不能跳掉任务初始化或必要事件；不能因接口名含Skip就保证覆盖全部CG。起飞、任务阶段、结尾需分别确认范围。
- **第三方机体：** 目录完整不是运行保证，沿用现有原生退让和真实类核对。所选SP的持续效果/多挂限制保持，不为快速入口绕过。
- **进度与方案：** 以独立测试出击为设计方向，优先复用自由任务规则；源码采用已核对的自由任务模式；存档/战役进度保持仍待实际验收。个人方案与旧检查点不跨任务复用。

## 原生链与当前实现

第二轮只读获取了当前构建的原生代码副本，摘要见[实现依据](../../evidence/quick-mission-native-entry-20261024.json)。原件只留 `E:/AC7_MultiSP/Temp/tasks/20261024-quick-mission-implementation/`，不提交完整游戏代码、配置或蓝图。

原生机库Sortie最终helper `0x6aed40` 先使用任务地图解析器 `0x4f6130`，把结果转FName，再调用GI加载器 `0x3d0320`，参数为使用过渡世界、空回调、允许加载。新入口复用这条最终请求；没有使用Shipping中为空实现的SuperF5，也没有直接调用OpenLevel。

自由任务模式由主菜单的原生选择证实为2，setter `0x3cfce0`；难度setter `0x3cfc40` 使用0—3，casual=false。先设难度，再调任务setter `0x3cfd90`，避免原难度4把目标转换成自由飞行。阶段4依据原生Sortie激活的赋值。核心逐项核对构建、函数字节、对象身份、真实主菜单、GI世界、NetDriver为空及任务表归属，布局不符拒绝执行。任务ID和DLCID始终读当前任务表；地图由原生解析器生成。

代码职责：

- `src/quick_mission.hpp`：任务表读取、原生自由任务设置/最终加载请求、Boot片头视觉结束状态；由weapon_bridge包含。
- `dist/DualSP/Scripts/quick_mission.lua`：独立预设、许可、幂等请求、跨世界等待、入口过场跳过和临时1至3SP配装。main仅负责接入已有调度与初始化，不另造武器初始化链。
- `src/quick_mission_panel.hpp`：F6调试页独立卡片。只提交动作，不在渲染线程调用游戏函数。UI/Lua子协议3与现有管理/换机协议分别核对；原生出击ABI为v2，接收首槽和第10项skin_no；独立能力门禁拒绝混装旧UI/核心。
- `runtime_context.quickMissionMenu`：只返回当前世界可见、已启用输入的MainMenuWidget，或F6实际暂借输入的该页面。

预设为DualSP目录下`quick-mission.json`，schema=3，字段mission_id/plane_id/a/b/c/skin_no/difficulty/auto_launch/skip_logos；a/b/c是原生配置号0至2，尾部空槽为-1，必须连续、不重复且至少一槽。默认M1、F-16C、单SP配置0、普通、自动出击关闭、Logo跳过开启。旧schema1的swp在内存转换为单槽；alpha2的扁平schema2保留全部a/b/c并补基础皮肤0，同时兼容研究嵌套schema2，启动读取不改原件；提交保存或出击时才写schema3，仍先写pending并备份旧文件，失败保留原件。配置和副本均忽略，不入Git。

出击请求冻结完整方案及当时双/三SP配弹比例；原生选机仅用首槽，新任务真实玩家就绪后返回完整临时方案，沿现有共用Profile与configureFlight初始化。保留调试允许未购买兵装、DLC许可与真实持续效果多挂准入；不以菜单目录猜测运行资格。不合格组合在HUD/武器重建前停止测试方案并提示返回主菜单调整，不向全局错误分支抛错或隔离整个机体，不保存为个人配装。M13继续原生任务SP。

自动入口在主菜单真实就绪2秒后发出一次请求；打开F6时延后，可取消本次。仅消费游戏启动时保存的auto_launch，当前保存开启不会立即自动启动。返回主菜单不重复出击。尚未绕过标题确认或存档选择，因此不能称全启动流程无人交互。

加载等待上限180秒；Transition中只保留请求数据，不放开核心。只有GI当前世界、任务ID、本地控制器、受控Pawn真实类及飞控输入均吻合才初始化测试兵装，初始化完成回调才显示成功。M13走任务指定兵装。入口期间使用SetIsSkipIGC，并在飞控就绪或失败时归还；只对起飞/开场补油小游戏申请SkipMiniGame，不取消战斗玩法。当前无法保证所有关卡的所有CG均覆盖，必须实测确认。

## 片头Logo跳过

从用户自有资源离线核对，启动地图为 `/Game/UI/Menu/Boot`；BNE、BNS/ACES、UE、CRI等开场Logo由 `CopyrightWidget` 的原生tick `0x656fc0` 分阶段播放。通用MoviePlayer的StartupMovies不足以说明这些Logo会被跳过，当前没有采用未经证实的NoMovies参数，也不修改或移除游戏视频、PAK、Engine.ini或Steam个人设置。

新代码只在Mod开启、当前为Boot世界、该页面归属当前GI且已激活时，复用原生视觉序列自然结束的赋值（`0x65724d`，计时基准和结束等待标记）。不再通过未能调用的SetRenderOpacity隐藏页面；视觉结束后的交接由原生处理。之后仍由原生tick核对加载器状态，调用原生完成流程进入标题；没有直接换图、跳过存档或屏蔽后续初始化。失败停止本次处理并保留原生启动。加载器早期显示、最初一帧闪现及全部Logo覆盖仍待候选实机验证。

## 2.25.0用户失败与源码修复

[日志证据](../../evidence/quick-mission-failure-20261024.json)记录本次PID17200、安装载荷摘要及最小错误摘录。北京时间08:19:58，F6已关闭，但快速入口prepare调用releaseWeaponInput，主菜单没有PlayerWeaponActivator而触发断言；本次没有原生出击接受日志，没有加载目标地图。因此不能归因于所选任务或机体不支持，也不能以已有任务表读取成功认定出击链通过。

快速入口现在只归还F6输入、清理通用运行所有权/HUD/旧检查点，不调用任务武器输入释放。没有修改releaseWeaponInput本身，正常任务重建仍要求有效武器组件。请求日志增加preparing、native_request、loading和weapons阶段，区分准备失败与原生请求失败。

同次启动08:15:29的Logo报错来自菜单GI归属校验。MenuBaseWidget没有名为GameInstance的反射字段；原生代码使用其+0x290内部缓存，Copyright激活0x656a17直接读取并以GI调用原生方法。新增读取指纹与已存副本逐字吻合，源码统一通过菜单基类身份与该缓存核对GI，替换Logo和MainMenu两处错误反射查询。世界、GI当前世界、网络隔离与UObject身份检查保留。该后续出击校验在本次尚未执行，是静态发现的同源阻断。

本轮只读日志/旧原生副本并修改源码，没有调用游戏函数、写进程内存、编译、运行测试或替换游戏文件。原2.25.0交付回执不修改；修复是否成功仍待新候选实机验证。

## 2.25.1快速出击成功后三SP失效

用户确认可以快速进入任务，但本项目三SP功能失效。[本轮日志依据](../../evidence/quick-mission-trisp-failure-20261024.json)显示PID29576在北京时间08:40:12提交M6/机体210并获原生接受，08:40:30在开场跳过中调用默认对象GetCurrentMission()报“expected 2 parameters, received 0”。异常冒泡至主调度，随后产生QUICK_MISSION_FAILED及机体210/Arkbird的adapter_error退让，未完成兵装初始化。这一现场不能当成三SP兵装切换本身失败。

源码不猜测该函数参数契约，改为查找当前世界唯一、有效且非CDO的Mission实例。开场跳过独立捕获失败，归还本次SkipIGC、停止本次跳过并提供双语提示，继续等待玩家飞控与兵装；不再由辅助错误触发机体隔离。片头同样隔离异常，移除现场报空UObject的SetRenderOpacity调用，保留原生视觉结束状态。任务、玩家类或兵装真正不符时的保护保持。快速测试仍按单SP预设进入；三SP组合可用任务内调试切换，本次不擅自覆盖预设或个人配装。

F6调试页顺序为任务配装、任务内换机、快速测试任务、日志；快速卡片固定在日志上方。该修复已随2.25.2完整构建及封装，未运行测试、安装或执行新游戏调用；待用户安装并重新启动游戏验证。旧进程已有的隔离状态不会因改源码自动解除。

## 待验证及授权边界

研究线程完成源码、双语资源和静态核对；本次整合另获编译与EXE交付授权，已完成完整构建和封装。研究观察、编译与游戏验收分别记录，没有执行新原生调用、安装或写进程内存。

原2.25.0候选已由用户安装，快速任务与Logo验收失败；修复源码已按用户新授权接收并编译为2.25.1，未安装或操作游戏。后续验收包括：M6和DLC2实际快速出击、23关映射、ex01/ex03、真实机体/SP、M13、DLC缺许可、开场CG/起飞及必要脚本、180秒失败退出、重复点击、自动启动一次/取消/返回菜单、片头全部Logo与存档加载、中英文及个人配装保持。保护旧dist核心，候选必须独立输出，不覆盖冻结产物。后续继续由当前主代理直接处理，尽量少用子代理。

## 快速任务皮肤与PART边界

接收bbf0984及其同步main的4fb9fd0，当前集成使用预设/界面协议3避免与alpha2同号不同结构混用。皮肤目录读取真实SkinNo、SkinID、PlaneStringID、PlaneReference及DLCID；引用错误、编号冲突或DLC许可未确认的选项禁用，基础0号保留原生基础类核对。切换机体重置皮肤0；选择同一机体不清掉当前配装/皮肤。出击前设置并回读GI皮肤，原生ABI v2核对同一数值；新任务再核对GI选择与实际机体/皮肤引用，完整配装仍冻结出击时比例。皮肤列表并不代表已逐项实测外观或所有战役解锁条件，不改购买/解锁记录。

研究预设的嵌套profile与alpha2扁平a/b/c都可读取，规范写回schema3。configured回调同步本次补给或调试后的临时方案，不反写快速预设或个人方案。PART尚未实现自选，沿用原生既有配件配置；完整装备、容量和冲突契约仍待研究，不能把已有数值预览当成装备功能。

核查时游戏安装为alpha2，不包含皮肤原生v2导出；本轮alpha3已完整构建核心/UI与18份Lua，并核对内嵌ZIP和双语正文，未安装或替换游戏文件。来源、迁移与故障边界见[本轮记录](../../evidence/2.25.3-alpha3-skin-resupply-source.json)，产物和用户待测项见[交付回执](../../evidence/2.25.3-alpha3-exe-delivery.json)。
