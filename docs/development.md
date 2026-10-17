# 开发与恢复

主工作区 E:/AC7_MultiSP，开发分支从 main 创建；授权、版本与待办以 [AGENTS](../AGENTS.md) 为准。

当前main正式基线为2.4.0，标签v2.4.0。用户已确认全部改动及两条研究支线成果正常，授权提交正式版；可配置快捷键、三机型挂载修订及ADFX-01持续效果已统一纳入。正式核心在dist/DualSP/cores/2.4.0/DualSP.dll，UI在dist/DualSP/bin/DualSPUI.dll，均从Temp/build-2.4.0按原字节归档，未重新构建或安装。有效布局及扩机型方法见[挂载设计经验](multi-sp-mount-design.md)。

当前源码与游戏均为2.16.0测试候选，已按用户授权合并a07dfe6并成套编译、通过适用检查、安装；正式基线仍为v2.4.0。用户确认CFA左侧多余EML蓝光消失、Su30M2正常；CFA选EML仅4fps/ECM发射无反应及ADF停光卡顿未通过，源码修订待验证。CFA/F22准备页SP2/SP3光标卡顿单列待排查，F2专项暂缓。2.12本轮F22已无肉眼卡顿，Su30M2/Su34/F14A TGM明确正常，其余未提按用户总体正常范围记录；不扩大为全47机逐组合验收。2.13源码：ADF01切走TLS（含MSL）立即停光；Su35 LAAM+6AAM借原LASM腹点补两枚6AAM；用户已确认原文Su37实际指Su30M2，改4AGM腹2/4AAM翼4；MiG29大LAGM内侧成对、小弹外侧，三挂外左HPAA1/右4AAM1；X02撤全部外观接管，保持原生。旧46资源字节/编号不改，新4副本至50资源/100成员，随本次完整构建打包。F22/F35及未点名方案保持。

2.16：自有挂架先按精确路径加载、登记表后备；同栈预览重入合并，保留显隐请求及末尾身份核验。CFA撤Parts快照/强收舱，恢复原生开合；与ECM双挂保整架左右和原生取位，只有ADMM+EML/三挂分侧。炮口元数据缓存、已隐藏不重复写，左EML蓝光仅在分侧组合抑制。ADF停光复用弱反射元数据，仍立即End+隐藏主束，首发仍需冷解析。2.15按组合加载资源、单SP旁路、统一阶段标题及换代等待保持。旧动画/停光实现保留于a4f7d1b；调查见evidence/2.16.0-performance-investigation.json，未宣称性能已实测修复。

调试页新增“换机并重开任务”：官方47机按当前原生目录核对编号，缺少DLC许可和未购买项也列出并允许尝试，按用户要求仅标注状态；不写购买/许可记录。先归还管理暂停、清旧兵装/检查点，再由原生RestartLevel重开；实际新受控Pawn类链、世界/控制器与兵装初始化全部匹配后才确认生效。已装2.13默认单SP0和基础涂装0；2.14改为默认三SP及允许未购买SP调试，多挂资格限制保留；临时方案不写个人配置，离开任务清理。不是即时换机，不承诺检查点换机；用户已确认2.13换机正常，未单独反馈的机体/缺DLC组合不扩大验收。

2.14：换机优先默认三SP；未登记持续效果等仅限单挂项仍保安全限制，按可组合最多种回退。任务调试和组合轮切允许未购买SP，正常机库/补给保购买校验，临时方案不保存。Su30M2三挂=腹4AGM2/翼内4AAM4/翼外HPAA2，复用27/46/47资源；旧50资源/100成员不改。ADF01切离TLS仍走原生End并立即隐藏MainLaser，消除尾段静止主束；其他TLS持续不改。2.15中CFA旧姿态快照和ECM强收方案已暴露发射门槛冲突，2.16已恢复原生动画；旧方案在a4f7d1b留档。左EML多余蓝光消失已获确认，2.16仍保分侧组合抑制；弹药和装填不改。

## 主管接手入口

用户要求新AI线程接替“核心功能演进”担任项目主管，统筹剩余机体适配、性能和稳定性跟进。优先复用下列具体线程，克制使用subagent；先核对各线程最新状态和工作树，再收取精确提交/文件差异。新机体须七种选择集合的呈现策略完整（允许舱内原生或整机原生旁路）、失败可自动回退；不能把离线研究当实机通过，不再要求各类舱内模型同时显示。

2.16成套交付见evidence/2.16.0-integration.json及evidence/2.16.0-installed.json，产物Temp/build-2.16.0-final；17运行文件、50资源/100成员及166保护文件核验通过。PAK与2.15完全相同，配置/存档/其他Mod保留；备份E:/AC7_MultiSP/backup/AC7MultiSP-20261017-222046-932756。游戏原已关闭，未启动；真实HUD、关卡旁路、英文/返回流程和CFA/ADF性能效果待用户测试。

本次2.15交付见evidence/2.15.0-integration.json及evidence/2.15.0-installed.json，产物Temp/build-2.15.0-final；17运行文件、50资源/100成员和164保护文件核验通过。PAK与2.14完全相同；配置/存档/其他Mod保留，备份E:/AC7_MultiSP/backup/AC7MultiSP-20261017-211723-960129。游戏原已关闭，未启动；首次必需模型同步加载仍可能停顿，实际性能和新预览行为待用户测试。

本次2.14交付见evidence/2.14.0-integration.json及evidence/2.14.0-installed.json，产物Temp/build-2.14.0-final；17运行文件、50资源/100成员和164保护文件核验通过。旧100资源成员摘要完全保持，配置/存档/其他Mod保留；备份E:/AC7_MultiSP/backup/AC7MultiSP-20261017-204021-003893。游戏安装前已关闭，未自动启动，真实动画/特效和挂载观感待用户测试。

### 循环切换、关卡旁路与界面修订

来源工作区`Temp/hud-mission14-localization`，分支`codex/2.15-hud-mission14-localization`的`a07dfe6`；本轮已合入2.16性能/挂载修订并成套构建安装。新核心、Lua和管理界面必须成套，来源目录继续保留，未推送或发布。

循环闪图来源是原生先SP→MSL、Lua随后选下个SP的跨调度间隔。现有研究`evidence/three-sp-switch-hud-research.json`确认输入调用点`0x823994→0x886c50`，新代码核指纹/回址后只把该输入变为逻辑请求；自动耗尽等其他调用保持原生。新增`dualsp_take_cycle_switch`与`dualsp_controlled_weapon_switch`，内部完整飞机调用用作用域标记旁路拦截，保留锁定组件、按下/释放和连射门控。2.16已通过编译与适用自动检查，仍无本轮实机证据。

第14关依据`NimbusGameInstance:GetMissionID`（已有反射记录`evidence/native/自由飞行-ui-interfaces.txt`）和任务/机库上下文旁路；本机自有MissionDataTable离线回读确认`ms14=114`、自由飞行`214`。资源来源为原版`pakchunk0-WindowsNoEditor.pak`，原uasset SHA256 `7a6aa945029531f3beb9ab57f26bab9c5affe9372bed9983e47cae4e5fc21fec`，uexp SHA256 `696445ad9365b16b70bcb07e961fd10b5113bb7d99993a1b7523b5c88b029d71`；提取副本及JSON只留此工作区Temp。管理许可与兵装许可分开，离关后重建会话，个人设置/方案不改；不能仅以残留任务编号禁用主菜单。

英文资源集中`src/ui_messages.hpp`；管理页所有动态状态、兵装说明、DLC/购买标记、错误及通知经过显示翻译，原生页面不受手动界面语言影响。未知中文底层异常显示对应功能英文摘要，日志保留完整原文。通知独立32号、管理26号，英文操作按钮加宽并限制在当前窗口内。

“原生机库拒绝了决定/返回”原来是`preparationTick`在交付边沿后看到跳转字段仍为0，就把本次动作当永久错误。该回执只能证明边沿未被接收，不能证明兵装故障。现返回`native_ignored`，Lua不保存草稿，恢复选择页/归还已关闭页，等待新输入；内部review先退回最后选装槽，避免空行绘制。结构、身份及三秒无回执仍保留错误。具体连按现场尚未复现，修订观感须后续用户测试。

2.16同时合并线程“修复切换与机库提示”的a07dfe6：原生顺序切SP不提交MSL中间态，改与直接选择共用逻辑流程；第14关真实任务ID114临时旁路兵装/模型/HUD接管，214自由飞行不受影响，离开后恢复且不写个人开关。管理动态状态和通知补英文，通知字号32；原生忽略单次返回时静默恢复，不保存未确认草稿，真正异常仍提示。

### 版本与本轮收尾

- 2.13.0已成套构建、检查并安装：Temp/build-2.13.0-final2，17运行文件、50资源/100成员PAK、158保护文件核验。旧92资源成员摘要完全保持；配置、存档与其他Mod保留。备份backup/AC7MultiSP-20261017-192334-141522，未自动启动游戏。见evidence/2.13.0-integration.json及2.13.0-installed.json；调试换机边界见2.13.0-debug-aircraft.json，真实新Pawn、任务/检查点和无DLC目标仍待用户实测。第一次Temp/build-2.13.0-final在空闲路径优化前构建，未交付。

- 2.13源码：ADF01切走TLS（含MSL）立即停光；Su35 LAAM+6AAM借原LASM腹点补两枚6AAM；用户已确认原文Su37实际指Su30M2，改4AGM腹2/4AAM翼4；MiG29大LAGM内侧成对、小弹外侧，三挂外左HPAA1/右4AAM1；X02撤全部外观接管，保持原生。旧46资源字节/编号不改，新4副本至50资源/100成员，随本次完整构建打包。F22/F35及未点名方案保持。本轮反馈见evidence/2.12.0-user-validation.json；设计正文新增默认审美准则与逐机摘要，不再一套三挂固定分侧套所有双挂。2.12.0交付回执和原构建保留；2.13.0已成套更新，换机及新布局仍待用户实测。

- 2.11.0完成用户睡前追加：Su-35S、Su-37、F-4E、F-14A TGM、Gripen E、Su-30M2、Su-34、MiG-29A、F-16XL、MiG-35D十机完整静态方案并成套安装。47架全覆盖分类为23静态/6机外保留/2原生旁路/16特殊后置；详细七组合与后置原因见挂载设计总表，源evidence/static-expansion-2.11.0.json冻结。原十机数据/原16资源字节不变，新23运行副本全LOD/编码回读通过；新分区绑定先只读核实际NoCollision，不写碰撞、不缩逻辑窗口。Gripen两份旧离线同名资源在关键来源/配方/编码摘要全相同时严格复用，旧工具执行记录不改写。下次构建以2.11.0-installed为安装基线。

- 2.10.0优化F22切换时同栈骨查询，新增ADF-01翼下共架、DarkStar PLSL常驻、ADF-11F左UAV右QAAM与CFA-44左ADMM右EML/隐藏IEWS候选。CFA原生x1类复用既有持续ECM框架，但仍须实际CDO/Actor虚表及装填门槛；ADF11窄导轨局部穿入UAV按用户轻穿模取舍保留。TLS机鼻/内部舱门原生，原13机和F35路径保持，详细组合及验证边界见挂载设计正文。当前批完成安装后，按用户睡前追加要求筛选其余飞机；含RKT/GRKT、挂点极少、仅内置弹舱、特殊动画及其他困难机体留待讨论，不冒然注册。

- 2.9.0已成套安装：Su-47/TGM任务及预览原生旁路，普通Su-57只保PLSL静态舱，F-22A只保侧8AAM，舱内/发射走原生；不启用旧共舱/全骨姿态模块。F22非8AAM配置借具名骨显隐去掉SWP3重复腹架，当前8AAM及Init前归还，运行资格不符独立退回视觉；F35既有函数/映射保持。16运行文件、32成员PAK（与2.8.1相同）、141保护文件核验通过，配置/存档保留；备份backup/AC7MultiSP-20261017-135954-441251，未启动游戏，见[本版交付](../evidence/2.9.0-integration.json)。

- 用户已授权实施机外保留路线并在完成后编译安装。完整弹舱多挂的2.8.1源码/21组合/证据和交付回执先留档到本整合分支提交与`archive/full-bay-mount-2.8.1`标签；这不是正式验收标签。用户认可F-22A、Su-47、Su-57系列现有布局思路可行，当前停用原因是性能和运行表现，不撤销布局研究结论。后续可从该标签及研究分支9cefa43/66d4f3d恢复继续研究；本机完整产物仍为Temp/build-2.8.1，不覆盖原字节。

- **最新方向覆盖旧动画待办。** 用户2.8.1反馈：TG 2–3fps、Su-47 6–7fps且空架；F-22A常见120fps、偶降80/30且切换卡顿，重复开舱仍在；F-35C约120fps且切换正常。见[最新实测](../evidence/2.8.1-user-validation.json)。后续优先机外外观连续：F-22A保侧8AAM、普通Su-57保翼下PLSL，舱内走当前SP原生；Su-47/TGM暂缓外观共存，X-02S退出多挂外观路线，CFA-44后续左ADMM右EML隐藏IEWS外观。保留多SP兵装功能，不再追求全部舱内模型共存或精确动画。七种选择集合仍须策略完整，允许内部原生/整机原生旁路。用户随后已授权实施并编译安装，2.9.0源码已完成机外路线接线；当前交付状态看本版回执。

- 2.8.1已按用户关闭游戏后的安装请求完整构建、通过适用自动检查并安装：16运行文件、32成员PAK（与2.8.0原字节相同）、138保护文件核验通过，配置及存档保留；备份backup/AC7MultiSP-20261017-131914-403446。交付见[2.8.1整合记录](../evidence/2.8.1-integration.json)。未启动游戏，实机帧率和开合效果待用户复测；F-22A独立分舱/Su-47新左右布局未交付。用户允许轻微肉眼难辨动画纰漏，后续不以全骨逐位准确为产品目标；本版Su-47旧门槛限制仍须明确。

- 用户已测2.8.0：Su-47三SP任务约1fps、关模型恢复且机库正常；F-22A/TG布局满意但任务约7fps，两机切SP反复开合。下一轮先修任务共用热路径与切换连续性；F-22A侧舱/腹舱分控须核真实骨骼/Socket，不按武器名猜测。Su-47左右分区重新研究，用户允许轻微不影响观感的接触。详见`evidence/2.8.0-user-validation.json`；该反馈后的修订已随2.8.1安装，当前实机效果待复测。

- 主工作区`E:/AC7_MultiSP`，整合分支`codex/menu-diagnostics-controls`。`dc4c2c1`归档源码与已安装2.7.1构建完全一致的阶段成果，以及本轮用户实测。main/v2.4.0保持原正式基线；本轮提交在整合分支，没有推送或发布。
- 当前源码升级为2.8.0，接入F-22A、Su-47、Su-57 Top Gun的七组合候选、动态取位和保守失败回退；Su-47仅对完整实采全开姿态提供零偏差可视资格，其他姿态隐藏内部弹体并保留原生发射。同步纳入2.7.2中已准备的F-2A修订：F-2A的LASM+HVAA双挂为大弹内侧、小弹外侧，配套新HVAA外侧副本；其他组合选位、弹药与冷却不变。三挂外侧为RKT，仍为分侧内翼导弹。新资源完成三LOD与AC7编码回读，已随2.8.0成套构建、现有检查并安装；新三机已有上述性能及舱门反馈；F-2A新布局仍待用户实测。详见[布局修订](../evidence/2.7.2-layout.json)。
- 源数据为`evidence/mount-expansion-batch1.json`，配方`src/research/static_mount_split_recipes.json`，离线回执`evidence/sp-combination-static-split-experiment.json`。运行表由`src/research/build_runtime_mount_data.py`生成，同时更新头文件与`mount_assets.lua`；禁止只改生成表。
- 本轮构建使用`Temp/mount-assets-2.7.2/source`（16副本、32资源文件，已按回执摘要核对），构建默认值已更新。原16资源中不再使用lasm_outer，增加hvaa_outer；索引与PAK已随2.8.0成套升级。不能沿用2.7.1旧PAK或把2.7.2 Lua拷进旧安装。游戏资源与工具全部留本机，不能提交。
- 2.7.1成套产物`Temp/build-2.7.1`，安装回执`evidence/2.7.1-installed.json`，备份`backup/AC7MultiSP-20261017-110205-136925`。2.8.0以该回执为升级基线，已通过正式build/tests/install流程交付Temp/build-2.8.0-final；16运行文件、32成员PAK、136保护文件核验通过，备份backup/AC7MultiSP-20261017-123525-791486。前一次Temp/build-2.8.0因Socket字段修正作废，不能安装。2.8.1已使用2.8.0-installed.json升级；后续版本改用2.8.1-installed.json为基线。

### 具体工作线程

| 线程 | 负责内容 | 接手时重点 |
|---|---|---|
| [各个战机多SP挂载方案](codex://threads/01a1441b-45a7-7d91-80f2-cf03b98647b1) | 剩余机体的完整七组合、挂架几何与布局 | 工作树`C:/Users/83717/.codex/worktrees/mount-expansion/AC7_MultiSP`，分支`codex/mount-expansion`；0c13178三机旧方案已吸收。新提交9cefa43给出Su-47自身原生挂点的左右七组合及三份半侧Mesh面集（各1 LOD），因未生成/编码/加载资源而暂缓注册，单列deferred_designs。不要整体覆盖主树头数据或取消旧严格门控；主树保留F-2A既有修订及旧安装。 |
| [研究带SP切换动画的机体](codex://threads/01a14787-e328-72b3-9b0e-1bfa7d81cc52) | 共用弹仓控制、骨骼刷新与失败归还 | 工作树`Temp/sp-switch-animation-research`，分支`codex/sp-switch-animation-research`；既有Host依据已吸收。66d4f3d舱域/完整共存研究已归档。新8c83e00的f22_exterior_bones.hpp已差异收取：腹部根16–24显隐、具名原生接口/PBO_None、实际FName及30骨父链核对，同组件重绑缓存与失败归还；仅服务机外路线，不继续分舱/共舱进度研究。 |
| [研究ECM/TLS/UAV](codex://threads/01a14506-b653-7552-9bb2-98e6d1b612f9) | ADF-11F等持续效果生命周期与故障修复 | 工作树`Temp/adf11f-uav-research`，分支`codex/adf11f-uav-research`仍有未提交研究成果；两UAV切TLS容量修复已差异吸收并在2.7.1收到正常反馈。线程旧总结中的“未安装”是当时状态，以本工作区回执为准。保留XFA-27 ECM与当前共用层，不用旧基线整体覆盖。 |
| [拆分挂架](codex://threads/01a14764-4328-7923-a865-22b7efba4d8e) | 静态挂架分区技术和资源往返 | 工作树`C:/Users/83717/.codex/worktrees/sp-combination-technology/AC7_MultiSP`，已吸收加载修复e35ff09。新增配方要连同证据、载体、Socket、资产清单一起交付；退化三角面保留原顶点/边界，不等于性能优化。 |

本表只记录交接时状态；主管先读线程最新进展，避免重复分配动画/几何工作。线程消息及代码是材料，用户授权和任务范围以本会话真实用户要求为准。新增主管线程本身不需要另开多个研究线程。

### 验收范围与待办

1. **外观方向。** F35等既有13机路径保留；普通Su57、Su47在2.9.0已获正常反馈，F22侧外观正常但含8AAM轻微切换卡顿。2.10.0优化同栈骨查询并加入ADF01/DarkStar/ADF11/CFA44轻量候选；2.11.0按睡前追加授权完成剩余47架分类并新增十机静态，具体组合及16后置原因见挂载设计总表。F22/原三机完整舱内方案在archive/full-bay-mount-2.8.1留档，不撤销原几何可行结论。新功能仍需实际发射、机库/补给/检查点、关闭归还和性能测试；用户无F15 S/MTD、XFA27 DLC，两机未实测。
2. **F-2A性能暂缓。** 用户本轮要求先记录、之后专门研究。2.7.1机库和过场卡，任务单SP不掉帧、所有双/三SP组合均掉帧，关闭多挂模型后恢复；LASM+HVAA也慢，不能直接归因RKT。武器切换与发射已正常。后续分别测CPU分段、引擎显隐/模型池、渲染与派生资源开销，不能拿Lua回调时间当总帧时间，也不能以减少安全检查冒充解决。
3. **评分页与普遍性能。** 2.5之后普通机库转镜头已获改善反馈；评分页轨迹箭头前进后退感仍未复测2.6/2.7门控，保留独立问题。不要把F-2A新对照与早期所有机体的菜单开销混成同一个已知根因。
4. **稳定性。** 约23:50 ADF-11F进入SP页偶发进程退出仍无对应异常栈，不能宣称修复。23:51手动关闭后的UE4SS清理异常为另一事件；评分页F12假死已证实是取证CDB漏配系统断点并恢复，勿误记Mod死锁。继续用桌面“AC7 崩溃取证启动”或根目录启动入口；原始dump/日志在Temp，不提交。发生后保存时间、机体/组合/页面与提示，按PID对齐日志和转储。

新版本安装需用户授权；此前开发阶段允许关闭游戏并编译安装修复版，本轮按用户明确编译安装及睡前继续授权执行，当前状态看最新交付回执，未推送或发布。默认不自动构建、写测试模块、运行测试、操作游戏、推送或发布。比例引导与锁定图标PAK保持禁用，配置和存档必须保留。现有副本资源、新增几何往返不证明游戏中的发射净空；布局大小优先级不能跳过原生父链、支撑或动画证据。

## 2.7.1修复整合

用户2.7.1反馈：F-2A切换/发射正常，但机库与过场仍卡；任务单SP正常，所有双/三SP组合启用多挂模型均掉帧，关闭模型后恢复。这是多挂外观路径对照，不能只归因RKT；具体开销未定，本轮仅记录，后续单独研究。Rafale M、Typhoon正常，ADF-11F反馈正常。这些反馈不代表全部任务/检查点均通过。详见[用户实测](../evidence/2.7.1-user-validation.json)。

2026-10-17用户实测2.7.0：初始三SP切换正常，依次发射TLS、QAAM、两架UAV后，游戏时间35.7367秒的请求104（UAV→TLS）触发“持续兵装：原生引用数组容量不足，旧效果将结束”，控制器回退原生MSL+单SP。两架UAV当时约5.95/5.62秒；这不是未购买、模组总开关关闭或游戏崩溃。原日志未记录两数组各自Num/Max，不能认定具体是哪一组不足。

源码已修正：保留对象前核对加载器FMemory/GMalloc接口及版本化容器就绪；原生重建后统计Active/Spawned缺项，用绑定游戏GMalloc的公开FMemory::Realloc按需扩大容量，保留原Num/元素。两组容量准备后重新读取data再补引用并提交Num，继续使用同步临时RootSet，不绕过身份/寿命/附着检查，也不引入长期root。新增CONTINUOUS_REFERENCE_PLAN和CONTINUOUS_REFERENCE_GROWN记录两数组数量、所需及扩容结果。容量修复已随2.7.1完整构建、通过适用检查并安装；合成检查覆盖扩容搬迁、分配失败及原Num/元素保留，真实分配器/GC与跨SP生命周期仍待用户复测。交付见evidence/2.7.1-integration.json。

本版基于实际安装2.6.0，合并当前工作区已吸收的e35ff09挂架加载修复、`Temp/adf11f-uav-research`未提交UAV候选，以及动画研究e519d02。运行文件没有从旧2.4.0分支整套覆盖，保留26号管理/提示、协议7、安全菜单总开关、分段日志、评分页门控及十机静态布局；XFA-27原生ECM契约继续保留。

UAV新增`raven_sp.hpp`与具名核心握手，ADF-11F按实际原生TLS/QAAM/UAV配置检查开放同选。持续对象从Active/Spawned并集去重发现，真实源槽独立登记；重建时临时保有，正常帧UAV由Active保有，退场仍执行完整原生父链。模型回收与战斗Actor分离，取消旧SP队列，切回后按缓存冷却重建回收进度。ADF-11F仍为当前SP原生模型，未新增三组外观同时显示。

挂架加载保留LoadAsset返回对象和registered/didLoad，未登记时经原生CreateAssetData/GetAsset按独立路径加载，详细诊断落日志。保留NoCollision和精确身份检查，加载失败仍独立回退；此修复尚待真实游戏确认。16副本PAK与2.6.0相同，成套重建程序并保留原PAK基线摘要。

动画支线只有离线设计：47架筛查、16架29个活动挂架、49份序列，15架105条组合关联；新增准入0。已合并三个研究文件和正文，未启用未完成的机型或强行复用F-35C偏移。

当前完整构建Temp/build-2.7.1，升级基线evidence/2.7.0-installed.json；适用检查及安装核验通过，16项运行文件、32成员PAK、131项保护文件。备份backup/AC7MultiSP-20261017-110205-136925，个人配置及存档保留，未自动启动游戏。来源冻结在Temp/integration-2.7.1-inputs，交付见[2.7.1整合记录](../evidence/2.7.1-integration.json)。本轮吸收挂载修复b1bc370及UAV容量修复；2.7.0三线来源见原回执，动画运行准入仍未扩大。

## 2.7.1挂载修复

用户2.7.0实测：F-16C、Mirage 2000-5正常且无卡顿；Typhoon正常但双SP空挂点较多。F-2A三挂模型正常、非对称可接受，但机库卡顿、任务约降40 FPS，RKT连射中切换会使多SP失效。Rafale M仍回退；F-15C、F-15J、ADFX-01沿用此前正常反馈，F-15 S/MTD、XFA-27因无DLC未测。反馈未逐项列明全部七组合和任务边界。

F-2A错误为原生暂拒切换触发Lua致命断言；新dualsp_switch_ready只读原生门控，Lua延迟请求在五游戏秒内重试并按上下文取消。Rafale采用Socket名称双射，保留原生池下标。Typhoon双挂数据、生成头和交接索引同步补位；单挂、三挂及PAK资源保留。性能候选减少重复读取与查页，继续保留序列号和接管权校验。具体布局、窄净空与验证边界统一见[挂载设计经验](multi-sp-mount-design.md#26.0运行接入范围)。

挂载修订已随2.7.1成套编译、通过适用检查并安装。dualsp_switch_ready已核对导出；Lua、核心和UI成套升级，Typhoon选位表随核心更新，原16个派生网格及PAK不变。性能收益、Rafale实际池匹配和新选位发射净空仍待实机复测。当前交付见[2.7.1整合记录](../evidence/2.7.1-integration.json)；下方研究回执保留修订时状态。 源码和现场证据见[修订回执](../evidence/2.7.0-mount-feedback-repair.json)。

## SP 切换动画研究的取用

独立分支 `codex/sp-switch-animation-research` 从main／v2.4.0建立；工作区 `E:/AC7_MultiSP/Temp/sp-switch-animation-research`。基于挂载总研究线程 `01a1441b-45a7-7d91-80f2-cf03b98647b1`，不修改其工作区或当前主工作区未提交成果。逐机结论及完整边界见[挂载设计经验](multi-sp-mount-design.md#sp-切换动画的静态研究)。

可取用三个文件：`src/research/analyze_sp_switch_animations.py`是离线分析入口；同目录`sp_switch_animation_policies.json`是16架机体的域划分、交接规则及未解条件；`evidence/sp-switch-animation-research.json`记录47架筛查、29个活动SWP、49份不同序列，以及15架新增机体的105条七组合关联。策略是研究数据，`runtime_enabled=false`，不能作为运行准入表。没有新增编译、测试模块、运行测试或安装。

复现时显式提供原挂载线程的官方资源清单、目录和第三批方案；分析器按实际Pawn参数引用读取，校验资源摘要，拒绝其他压缩格式，输出文件必须不存在。需要本地原件，不下载依赖、不读取游戏进程。参数示意如下，路径须指向本机对应原件，输出仅放Temp：

```powershell
python -X utf8 src/research/analyze_sp_switch_animations.py `
  --catalog E:/AC7_MultiSP/evidence/mount-expansion-catalog.json `
  --batch E:/AC7_MultiSP/evidence/mount-expansion-batch3.json `
  --manifest Temp/input-manifests.json `
  --manifest Temp/mount-assets-20261017-094711-176575/manifest.json `
  --manifest Temp/mount-assets-20261017-094909-555475/manifest.json `
  --manifest Temp/mount-assets-20261017-095002-268072/manifest.json `
  --native-manifest E:/AC7_MultiSP/Temp/su30sm-fixed-mount-prototype/Temp/f35c-cover-state-native/manifest.json `
  --output Temp/sp-animation-review.json
```

`Temp/input-manifests.json`列出参考线程的`Temp/batch3-manifests.json`实际输入清单，前三个本工作区提取批次补齐47架参数表、仅标志机体的Body和F-35C Body序列。原件均留本机Temp。上游目录与批次的SHA256、每个实际使用资源对的摘要写入回执；本轮回执不依赖上游尚未合并代码。接入线程可按需取此分支的三个文件；设计章节与证据索引按差异整合，避免覆盖其后续文档。

## 有效入口


## 动画布局覆盖层的取用

本分支以`evidence/mount-expansion-animated.json`记录八机深入评审与三机完整七组合，统一正文见[动画候选](multi-sp-mount-design.md#带动画机体的完整七组合候选)。`mount-expansion-handoff.json`的`animated_design_overlay`单独绑定这份数据；旧`eligible_aircraft`／`integration_candidates`仍只服务十机静态路线，旧三批记录不当作最新动画布局。不要直接把三机代码追加到旧运行生成器：本层显式区分owner/provider/呈现架、隐藏provider刷新与内部ammo门控，旧数据格式没有这些保证。

引用线程`01a14787-e328-72b3-9b0e-1bfa7d81cc52`的`analyze_sp_switch_animations.py`、`sp_switch_animation_policies.json`与`sp-switch-animation-research.json`，并从其提交`00bd24c`按原字节吸收`common_bay_animation.hpp`、`native_common_bay_parts.hpp`、`owned_bone_refresh.hpp`、`inspect_common_bay_control.py`与`sp-common-bay-control.json`。三个控制头没有自动注册／新钩子，不改变产品运行代码。几何准入来自本分支覆盖层，YF-23虽有控制profile仍被几何排除。

产品接入必须补齐：安全访问与反射Host；完整原生实例／场景身份校验；同步切换快照与原生Parts更新顺序；Pause/Tick及刷新模式最新原生请求观察和逐组件归还；provider当帧组件空间／世界空间Socket取位；Su-47的严格全开＋端点姿态见证。Parts原生引擎不是UObject；指纹／owner／配置必须通过，写入部分失败及归还失败不得误报成功。TG第三组静态不读骨架字段，第三原生配置的NativeTransition保留原生流程。姿态容差、坐标换算和运行池数量需有真实依据，不能填经验值或直接等于Socket数。

0c13178合并后尚不能直接交付三机可玩安装。用户已用F-22A单SP8AAM配合暂停态及45秒连续只读采样：实际8AAM逻辑14槽／8窗口，持久SWP1／2／3池6／6／14。现场元数据证明NoSkeletonUpdate为mask2、PauseAnims为mask4；原生7f0210在关闭Tick后写的是NoSkeletonUpdate，旧研究“直接写Pause”结论已修正。共舱控制请求现分别保存三项字段，仅更新本次原生确实改变的项。原生打开请求1可在phase2／progress1时保持全开，原生全开判据已接受请求1或2；关闭请求3立即不满足该判据。

采样摘要见[三机接入的F-22A运行依据](../evidence/animated-runtime-f22a-capture.json)。GetSocketTransform参数、枚举与Transform布局已取证；骨骼发布slot890→16e5de0和帧标记写入slot878→16fb660是时序接线依据，原件及连续记录只在Temp。连续轮询不是函数调用跟踪，不能把任意缓存变换当成当帧结果。2.8.0已接入三字段最新原生请求归还、发布后的取位及Su-47严格零偏差全骨门槛并成套安装；用户已反馈严重性能问题，原安装未替换。现场取证始终只读，没有调用游戏函数、写进程或附加调试器；候选运行时仅在自己的游戏线程绑定内管理已授权的模型状态。

产品接线现位于`mount_animated_layout.hpp`、`mount_animation_host.hpp`和`mount_animation_runtime.hpp`，三份实际反射/发布适配头一并包含核心。已通过核心语法编译、成套构建和现有适用检查，并安装2.8.0；具体哈希与保护范围见本版回执；新三机已有上述性能问题。刷新模式只允许原生已是AlwaysTickPoseAndRefreshBones的所需骨架，不写或恢复模式字段；原生改变模式即回退。共用Controller的Pause保护覆盖绑定、维持和restoreSwitch内部路径；真实暂停先记账再归还。NoSkeletonUpdate、Pause和Tick分别跟踪，原生初始化期间暂停强制维持，实际Init完成后、重新接管前读取最后三字段请求；不能把初始化循环的中间清位当最终原生意图。任务/预览独占一个动画绑定，世界、资源、组件或组合改变时先归还；归还失败保留身份拒绝覆盖。

2.8.0性能反馈后的源码修订：完整世界与机型类路径仅在绑定时解析，热路径复核已绑定的对象序列号、类/包FName与每条Outer/父类边；不靠长时间缓存“仍有效”的布尔值。MasterPose字段绑定后缓存元数据，发布时仍核对象、字段布局和实际弱引用。Controller单次维护先收齐所有真实Pause意图，再核模式和按需写入，Host不重复整组读取。原生NoSkeleton/Tick/Init入口仍同步观察，100ms巡检仅作兜底；Parts不再前后各维护一次，更不对F-22A/TG每帧重放整套显隐。Su-47等所需组件本帧全部发布后统一应用，收拢立即隐藏。公共舱快照先排队，完整Initialize、弹药恢复及keepArmed结束后核同request/场景/generation/目标配置并一次消费；失败只归还视觉，不回滚已完成弹药事务。F-35C现有独立通道及切换实现保留；其原生模式6不能复制到新三机的公共模式1。TG静态LACM点仍在共舱内，因此三配置均共享切换快照，但仅前两组管理骨架。F-22A分舱仍待逐骨驱动改造。新增`ANIMATED_WORK_COST`每约2秒归纳本模块Parts后处理、发布观察及周期ensure的均值/峰值/次数，发布原函数耗时排除；不是游戏总帧时间。上述运行源码已随2.8.1完整构建、通过适用检查并安装；尚无实机帧率收益结论。

Su-47两份UGB/QAAM全开只读快照证明Socket局部属性、实际CDO缩放和Socket骨组件单位与资源一致，但全架另三根骨的四元数存在约10^-7差异。已用实际发布的完整骨骼重核全部LOD，四种多挂的弹体—Body、弹体—弹体均无表面接触，弹体—架体保留与原生完全相同的挂耳基线。产品采用`su47_native_reference`的严格uint32逐分量相等门槛，需同帧发布的全部38根Body骨、所需6骨挂架、Socket局部及相对父链变换同时相符；不允许非零姿态偏差。其他飞控姿态隐藏内部ammo并旁路本次映射发射，不永久关闭会话或阻止原生发射。证据见`evidence/su47-native-endpoint-geometry.json`；这不是全姿态/全流程实机通过。

| 离线入口 | 作用 |
|---|---|
| `inspect_mount_assets.py --asset-path` | 可重复指定精确PAK相对.uasset路径；提取同名HUD图与真实模型时避免歧义，仍保留原basename重复保护，不改原游戏包 |
| `read_cooked_skeletal_mount.py` | 严格解析本版本骨架网格Extras、全部LOD/分区/权重/索引与参考骨架；不扫描偏移猜顶点，不支持的布局直接拒绝。格式参考UEViewer的UE4原始序列化实现 |
| `audit_animated_mount_geometry.py` | 核对25份机体／挂架、49LOD的完整读取与参考姿态蒙皮回构，仅输出摘要 |
| `review_animated_us_bays.py` | F-22A／YF-23及FB-22旧参考；真实Socket、骨架、原生接触基线及XSDB展开序列 |
| `review_animated_sukhoi.py` | Su-47／普通Su-57／Su-57TG七组合、完整架、Body及原生弹体核对 |
| `review_animated_dark_falken.py`、`review_animated_fb22.py` | DarkStar／ADF-01／FB-22整架与固定Body干涉，保留拒绝依据 |
| `build_animated_mount_candidates.py` | 只接收人工评审的整机七组合，校验真实owner类／自身弹体／provider点与更新集合，生成独立覆盖层；不启用机型 |
| `build_mount_handoff.py --animated-batch` | 校验覆盖层七组合完整性和来源指纹，将静态路线与动画路线分开索引 |

分析器只读显式manifest与资源，原始提取物／完整大采样在Temp；提交的证据保留摘要、少量接触见证和哈希，不带游戏资源、PAK或原始顶点数组。NumPy仅用于离线几何，未下载新依赖。数据整理与资源分析不属于游戏编译或运行测试，本轮均未执行后者。

复现须提供本机原生资源manifest（包含Pawn／三组SWP／参数表／Body／各序列与武器模型；F-22还需真实XSDB骨架、Skeleton及展开序列），分析器参数见各入口。所有输出必须新路径并放Temp，例如：

```powershell
python -X utf8 src/research/audit_animated_mount_geometry.py --manifest Temp/batch3-manifests.json --manifest Temp/body-manifest.json --batch evidence/mount-expansion-batch3.json --aircraft f22a --aircraft su47 --aircraft su57tg --output Temp/new-geometry-review.json
python -X utf8 src/research/build_animated_mount_candidates.py --batch evidence/mount-expansion-batch3.json --review Temp/animated-us-bays-review.json --review Temp/animated-sukhoi-review.json --review Temp/animated-dark-falken-review.json --review Temp/animated-fb22-review.json --geometry-audit Temp/animated-mount-geometry-audit-final.json --control-source src/common_bay_animation.hpp --control-source src/native_common_bay_parts.hpp --control-source src/owned_bone_refresh.hpp --admit f22a --admit su47 --admit su57tg --output Temp/new-animated-candidates.json
python -X utf8 src/research/build_mount_handoff.py --catalog evidence/mount-expansion-catalog.json --batch evidence/mount-expansion-batch1.json --batch evidence/mount-expansion-batch2.json --batch evidence/mount-expansion-batch3.json --animated-batch evidence/mount-expansion-animated.json --output Temp/new-mount-handoff.json
```

覆盖层中`reviews`的完整分析指纹、`analysis_sources`及`control_sources`限定取用版本；US／Sukhoi分析器入库仅统一行尾及末尾空行，原分析指纹保留，入库源码指纹另列，没有重新运行几何。后续修改控制头、解析器或资源需重核对相应依据。XSDB展开播放推导不能替代实际释放运动，Su-47显隐也不能证明闭舱实体净空。全部新增机体待产品接入和用户实玩，不编译、不安装、不操作当前游戏。

| 工作 | 工具 |
|---|---|
| 完整构建 | src/build.py，成套构建核心/UI，输出全新 Temp 子目录 |
| 自动检查 | tests/run.py --build-dir 指向上述目录，失败不会写通过回执 |
| 本机升级 | src/install.py，默认预检，显式 --apply 安装，不启动游戏 |
| 本地打包 | src/package-release.py，使用当前产品清单与完整构建回执，已有 ZIP 不覆盖 |

2.0.0已在 Temp/build-2.0.0 完整构建、检查并安装，用户实测通过，历史基线标签为v2.0.0。下列为操作顺序示例，现有目录和安装回执已存在，勿重复执行；后续新构建须使用全新目录并核对当时安装基线：

```powershell
python -X utf8 src/build.py --output-dir Temp/build-2.0.0
python -X utf8 tests/run.py --build-dir Temp/build-2.0.0
python -X utf8 src/install.py --build-dir Temp/build-2.0.0
python -X utf8 src/install.py --build-dir Temp/build-2.0.0 --apply
```

构建输出必须是全新目录；默认工具根为 tools，可指定 --tools-root。每次完整编译，不再接受旧 --reuse-core-build。自动检查不等于实玩。

build.cmd/build-core-portable.cmd/build-ui.cmd 和旧研究构建入口只转发 build.py，全部构建成套产物，不直接覆盖 dist。旧三SP安装入口只转发 install.py；单机型安装入口明确拒绝执行，原实现可从 Git 恢复。其余历史研究/补给安装器不用于当前版本。

## 来源与交付

产品名称与 Lua 清单在 src/product.json；版本在 VERSION。Lua 唯一源 dist/DualSP/Scripts，共十三个脚本。2.4.0正式核心位于dist/DualSP/cores/2.4.0/DualSP.dll，UI位于dist/DualSP/bin/DualSPUI.dll，构建副本在Temp/build-2.4.0，与已实测安装及源码一致；旧2.0.0核心保留，旧UI从v2.0.0标签恢复，仅与当时源码配套。旧 bin/DualSP.dll 仅作保护基线。
加载器/许可从 payload 显式取用；[工具清单](../tools/README.md)、音频、运行库、ZIP 是本地依赖，不入 Git。

构建回执包含 src 产品 C++/头文件、vendor 全树、所有 Lua、正式工具与参数，以及编译命令和工具链全树指纹；构建前后核对一致。安装/打包再次核对整个来源集合，不能只比对清单中少数文件。旧格式回执不接受。

本机升级基线由 product.json 的 upgrade_receipt 指定，本次为2.3.1→2.4.0，candidate=true为冻结构建时的候选标记；正式状态以main、v2.4.0和用户验证记录为准，保留来源及历史安装回执字节。已核对游戏退出后安装2.4.0。不能重跑旧安装覆盖历史回执。未来升级须选择新的完整runtime_files基线。安装器接受schema 2/管理参数5或6并保留个人配置原字节；2.4.0首次启动先独立备份，再将快捷键迁至管理参数6。现有脚本须与基线完整匹配，新模块必须登记，不能静默删除旧模块。
安装必须有匹配构建和测试源码的通过回执。退出游戏后整套暂存、核对、备份模组和存档，再交换目录；保留个人配置、其他模组及外部 PAK。第二步交换失败会尝试归还原目录，不启动游戏。

[2.4.0用户验证](../evidence/2.4.0-user-validation.json)记录正式化授权、范围及回执摘要；正式DLL与用户测试构建逐字节相同。源码清单保留原candidate字段以维持构建来源校验，未改动运行逻辑、构建或旧回执；正式版状态由Git标签与用户通过记录确定。本轮不制作ZIP、不推送、不操作游戏。

2.4.0的[构建](../evidence/2.4.0-build.json)、[检查](../evidence/2.4.0-tests.json)、[安装](../evidence/2.4.0-installed.json)已归档；备份backup/AC7MultiSP-20261016-231446-459750，113项受保护文件核验通过。真实配置安装时保留原字节，首次启动迁移；临时副本迁移已核对方案、旧管理键与其余字段不变。未自动启动游戏。

比例引导mod按用户要求禁用：原Game/Content/Paks/~mods/proportional guided missile_P.pak移到backup/disabled-guidance-20261016-231440，SHA256为41b49ad83d60509795db910a02fd24c159ab39c324b5d1b745906f4a95a094e5，其中disable-receipt.json记录恢复位置；需恢复时退出游戏再原样移回。未删除或改写PAK，其他模组由安装器核验。

2.3.1的[构建](../evidence/2.3.1-build.json)、[检查](../evidence/2.3.1-tests.json)已归档，已安装，见[安装回执](../evidence/2.3.1-installed.json)；最终产物Temp/build-2.3.1-final。完整模组/存档备份位于backup/AC7MultiSP-20261016-223134-297074，个人配置和其他模组保留，111项受保护文件核验通过，未自动启动游戏。先前Temp/build-2.3.1因构建期间源码收尾变更被来源校验拒绝，不能交付。

2.3.0的[构建](../evidence/2.3.0-build.json)、[检查](../evidence/2.3.0-tests.json)、[安装](../evidence/2.3.0-installed.json)已留存；备份为backup/AC7MultiSP-20261016-194543-836550，安装后108项受保护文件核验通过，个人配置及完整模组/存档备份核验通过。未启动游戏，自动检查不证明真实挂点、动画或补给效果。

[2.0构建](../evidence/2.0-build.json)、[检查](../evidence/2.0-tests.json)、[安装](../evidence/2.0-installed.json)、[用户实测](../evidence/2.0-user-validation.json)均保留原记录，对应旧基线3c00f3e。
2.0.0的[构建](../evidence/2.0.0-build.json)、[检查](../evidence/2.0.0-tests.json)、[安装](../evidence/2.0.0-installed.json)回执已保存，完整产物原样存入 dist。原模组与存档备份在 backup/AC7MultiSP-20261016-120724-927183；个人配置、存档、其他模组核验未变。安装时正常关闭游戏且未自动重启；用户随后确认[实测正常](../evidence/2.0.0-user-validation.json)，授权提交2.0.0并合入main。标签v2.0.0保留历史基线，后续从当前main/v2.4.0开发；现存1.1.0 ZIP不变。

## 偶发退出调查

用户报告2026-10-16约23:50 ADF-11F进入SP选择页后突发退出，重开同机型正常。源程序/已安装运行文件仍与v2.4.0回执逐项一致。原始转储、UE4SS/mod/native日志及本地CDB输出保存在Temp/crash-analysis-20261016-2351，不提交原始转储或个人状态；脱敏摘要见[调查记录](../evidence/2.4.0-hangar-exit-investigation-20261016.json)。本轮仅离线读取与静态审查，没有操作游戏、修改运行代码/加载器、编译、测试或安装。

必须区分两类事件：23:51:40和23:04:54的转储都在UE4SS!RC::Input::Handler::~Handler+0x1a7（DLL+0x2f5957）读取空RCX，调用栈包含LdrShutdownProcess→RtlExitUserProcess→ExitProcess，属于进程退出清理异常。用户于2026-10-17明确确认23:51:38为测试正常后手动关闭，故23:51:40转储对应手动关闭后的清理错误；此前23:49–23:50机库突发退出仍是独立未定位事件。2026-10-16的11条应用1000记录均为相同模块/异常/RVA，不能据此称11次游戏内崩溃。用户报告的首次退出前，日志最后为23:49:52菜单PERF_TICK；23:50:25重启后，23:51:29接入ADF-11F三SP页、23:51:31确认、23:51:32回到STANDBY，证明重试流程正常。首次退出没有匹配WER转储，且重开覆盖了当时UE4SS日志；退出清理空指针不能作为首次退出根因。

ADF-11F未接入三机型固定多模型或Morgan持续效果专用映射；通用页面事件、异步机体加载、控件构建，以及加载器和AC7MouseAim共存仍是待证方向，不能因无复现就排除。需要区分或排查共存时，按游戏+加载器、仅MultiSP、再加AC7MouseAim逐项对照，未经用户指定不更改当前模组配置。

用户已要求每次启动游戏自动取证。`src/research/start_crash_capture.ps1`现使用本机微软签名的WinDbg amd64/cdb，替代缺失的ProcDump依赖。桌面“AC7 崩溃取证启动”或根目录`启动游戏并取证.cmd`先启动隐藏监听，再通过Steam启动502500；检测到游戏已运行时直接附加，重复启动沿用已有监听。正常路径等待实际CAPTURE_READY后提示成功；环境检查可用`-CheckOnly`，仅监听可用`-WaitOnly`，不注册常驻服务、计划任务或全局调试器，不改游戏、加载器或Mod配置。

每轮写入Temp/crash-capture-时间/session-N-pid-PID。CDB记录访问违例、栈溢出、堆损坏、快速失败、非法指令和致命应用退出的首轮异常；未处理异常另存所有线程栈；在ntdll!RtlExitUserProcess和NtTerminateProcess入口记录调用者，区分退出请求与UE4SS清理阶段。异常用gn交还应用处理，退出断点用gc自动继续；系统调试断点bpe另设日志和gh自动继续，防止F12使无人值守采集永久暂停。每PID上限8份首次异常、2份未处理异常、2份退出请求转储（/mipu，含栈引用内存，不含完整进程内存），上限后仍继续记录栈。-pd及异常收尾发送qd用于安全脱离，不强杀调试器或游戏。符号仅查本地目录并忽略环境符号配置，采集不依赖在线符号下载。规则参考微软[CDB参数](https://learn.microsoft.com/en-us/windows-hardware/drivers/debugger/cdb-command-line-options)、[异常控制](https://learn.microsoft.com/en-us/windows-hardware/drivers/debuggercmds/sx--sxd--sxe--sxi--sxn--sxr--sx---set-exceptions-)及[转储命令](https://learn.microsoft.com/en-us/windows-hardware/drivers/debuggercmds/-dump--create-dump-file-)。

每5秒覆盖一份512KiB日志尾部快照，发现新转储另存快照，进程退出保存完整UE4SS/mod/native日志和配置、页面状态、安装回执；附带快照时间与文件偏移。运行文件清单存哈希，不复制二进制。退出后继续等待60秒供启动交接/快速重开，最多8个PID；初次未启动游戏等待10分钟，游戏运行时不限时，结束后保存本轮应用事件1000/1001/1002。正常关闭也会产生退出现场，必须结合用户操作和时间线判断，不能自动计为崩溃。外部强杀、断电、附加之前的退出或超出采集范围的错误不保证获得最初调用者；快速重开可能影响最终完整日志归属，因此优先比对独立PID目录内此前快照。

2026-10-17已核对微软签名、启动环境、脚本语法、后台等待和实际日志快照；用独立临时Python进程确认访问异常转储、退出前调用栈及安全自动继续，原始验证仅存Temp/capture-check-*。用户随后通过入口实际运行游戏，PID35172已持续附加；评分页按F12后卡死、无声，00:52:35采集器停在80000003断点，栈为ntdll!DbgBreakPoint→DbgUiRemoteBreakin。00:57:50保存线程栈及小转储后，向已有采集器控制管道发送bpe自动继续规则及gh，进程随即重新响应、日志恢复；用户确认画面和声音恢复、可以操作。因此本次卡死是采集器漏配系统断点的暂停，不能作为Mod死锁或此前偶发退出的证据。[微软说明](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-registerhotkey)明确F12为调试器保留键。本会话已即时修正，启动脚本已同步补齐；独立进程DebugBreak后自动继续并正常退出，退出请求转储仍有效，验证仅存Temp/capture-f12-check-*，没有编译或安装Mod。

本次原始现场在Temp/hang-20261017-005235-f12；非侵入第二调试器因0xC0000022未能附加，实际转储通过原有调试器取得。性能问题仍独立待查：暂停前一条menu周期统计average_ms=1.422、peak_ms=94只统计Mod回调，不能代表总帧时间；暂停现场主线程位于dualsp_mount_preview→mountIdentity→checkMemory→VirtualQuery，已用同版DLL完整COFF符号校正，不能按CDB附近导出误称图标或兵装目录调用。单个采样不能证明性能根因；恢复后315887ms峰值包含调试暂停，不应计为Mod运算开销。

若采集器未来已停在调试提示符，可在本轮目录control.txt写入snapshot（保存栈与小转储）、continue-breakpoint（设置bpe规则后gh继续）或detach（qd脱离）；工作进程仅接受这些固定操作，消费后移动为processed文件。此通道是已暂停现场的维护入口，不会主动打断正在运行的游戏；运行中的输入可能等到下次调试器接受命令时执行。真实偶发退出的根因仍待捕获。触发时记下时间、机体/页面、操作、实验开关状态，以及是突发退出还是手动关闭；保持本地目录即可分析。

## 机库与评分页性能排查

2026-10-17用户报告机库转镜头和任务总结评分画面卡顿，自行关闭总开关、保存重启后明确反馈“机库预览飞机时移动镜头完全不卡，评分页也完全不卡”。已核对重启后runtime_mod_enabled=false，因此当前配置中应优先调查总开关启用的功能路径；不能再只笼统怀疑游戏本身。加载器、管理及场景许可在禁用对照中仍运行且体验顺畅，其单独开销可下调优先级，但尚未排除与已启用功能的交互影响。具体热点仍需分项计量，不能把功能级对照直接归因为某个函数或仅归咎实验模型。

首轮只读源码、既有日志和先前暂停栈；反馈后准备分段诊断，随后用户明确要求编译安装并加入安全总开关/通知。现已按完整流程交付2.5.0，未自动启动游戏或改写个人配置。记录见[菜单性能调查](../evidence/2.4.0-menu-performance-investigation.json)，原始资料仅存Temp/menu-performance-review-20261017。事发后00:58:06–01:01:51共16个menu统计窗口、6779次Mod回调，按样本数加权平均9.416ms，最大峰值102ms；此前00:26:57–00:28:13的6个menu窗口平均7.791ms、峰值55ms。scene=menu未区分评分/机库/其他菜单，用户未逐窗口标记页面，不能冒称精准同场景帧率测试。计时仅覆盖Lua tick及其同步调用，不覆盖独立原生/渲染钩子，也不等于总帧时间或纯CPU时间；排除F12调试暂停的315887ms异常值。

源码确认四项开销路径，但各项占比尚未测量。第一，tick及模式事件调用runtime_context.allowed，每次遍历UIManager.MenuWidgetArray，核对对象、类型、显隐及输入；总开关禁用分支在其后，故关闭后仍有此工作。第二，maintainScenePreview以共享单人菜单许可及HangarManager.LoadedPlayerPlane为条件，没有限制为真正需要展示的机库页面；评分页可继续维护机库实例，先前暂停栈已确认dualsp_mount_preview→mountIdentity→checkMemory→VirtualQuery。Lua每tick进入预览；稳定方案在核心每100ms完整巡检，不能误称每tick都重做完整反射和重绑。第三，兜底场景预览每0.5秒重新optionsFor，涉及原生配弹及购买资料；非任务仍泵送三类机库页面，未命中页面时每类最多每0.5秒FindAllOf，激活事件缓存可减少搜索，不能把最坏频率当实测次数。第四，read/write逐次VirtualQuery，对象身份、布局、名称字符串中存在同一内存区域的重复校验；仅为开销候选，不能删除安全检查来换性能。

总开关对照仍不能称完全原版：UE4SS、AC7MouseAim、管理渲染/输入钩子、游戏线程调度及许可识别仍在。08:41已只读核对重启后mod_enabled及runtime_mod_enabled均为false，随后收到用户两处均不卡反馈；启动/菜单首批样本不能直接与此前评分事件后窗口计算改善比例。已排除一个错误方向：overlay.cpp已有面板关闭时跳过ImGui NewFrame/绘制及渲染目标切换，不能笼统归为管理面板每帧GPU绘制。优先量出预览、页面发现、部件配弹，再按需要细分原生identity/apply/rebind及回调外显隐钩子。优化应避免评分页刷新未展示机体、用身份/配置/部件变更驱动重新计算、合并单次操作内同页内存校验，同时保留SKIN/PART/起飞交接的立即覆盖和异常回退。

当前源码诊断候选在main.lua保留PERF_TICK，并每15秒累计输出PERF_STAGE：context（tick场景许可）、options（兵装资料和配弹）、pages（三类机库/补给页面pump）、preview_scene（兜底预览）、preview_native（实际原生预览调用）、publish（tick状态发布）。不逐次写日志，不更改武器事务、显示刷新频率或故障回退。父阶段包含子阶段，options/preview_native也可能由页面事件调用，不能将各项相加当总耗时；切场景丢弃未输出的分段窗口，首帧可能不计入分段。计时未覆盖所有事件和独立原生钩子，不能把缺失部分记为零。2.5.0已完整构建、通过适用检查并安装，真实性能细分待用户测试；先用分段结果锁定主要耗时，再实施优化。PERF_TICK新增最后观察到的可见且开放输入菜单类名，不能把它当作独占顶层页面证明。

## 2.6.0十机与独立挂架交付

用户确认2.5.0功能生效、机库转镜头已不卡，但评分页轨迹箭头前进后退；要求十架全部完成接入再安装。研究线程01a1441b-45a7-7d91-80f2-cf03b98647b1最新提交77e9ffc，含上游7ae97b6分区成果；按差异吸收研究数据/工具/设计正文，未覆盖核心诊断和管理改动，未写研究工作区。正式main/v2.4.0不变。

16副本从技术研究工作区Temp复制到本工作区`Temp/mount-assets-2.6.0/source`，逐文件匹配原回执AC7编码哈希。生成器`src/research/build_runtime_mount_data.py`同时产生C++规则和Lua资源清单；构建默认读取上述本地输入，也可显式`--mount-assets-dir`。`src/build_mount_assets.py`生成不覆盖原资源的PAK4，并用既有独立读取器回读32成员；不把游戏资产、密钥或PAK提交Git。资源材料来自本机自有游戏，源字节/副本仅在Temp和本机安装目录。

完整产物`Temp/build-2.6.0`；安装基线`evidence/2.5.0-installed.json`。安装器验证PAK构建哈希、旧包基线、完整程序和个人配置后，备份模组/存档并成套安装；失败归还程序目录和已装旧PAK，不删除文件。源码快照包含运行规则、生成器和研究输入，历史证据不改成游戏已通过。构建/检查/安装回执按2.6.0前缀存evidence；未自动启动游戏、制作ZIP或提交main。

评分页09:19:45分段确认可见CampaignDebriefing时，423次回调平均9.090ms；preview_scene平均5.712ms，pages平均2.972ms；options29次平均8.207ms，preview_native平均5.076ms。阶段嵌套不能相加。原日志保存`Temp/2.6.0-integration-inputs`，摘要/哈希见[整合回执](../evidence/2.6.0-integration.json)。本版停止非机库预览和页面扫描，保留机库切页0.35秒无页过渡；尚不证明箭头回退根因或改善幅度。

管理与顶部通知字体20→26，最大面板1180×920。新增十机单挂归还原生、多挂使用共同映射；F-2A76原生候选中仅外侧38口可参与多挂，队列身份扩为256、物理显隐64位。XFA-27 ECM按精确原生类/虚表/发射即装填契约尝试持续保有，不能推广其他ECM/UAV。资源纯网格固定保有不引用世界，原网格通过引擎接口归还。具体布局和未验证边界统一见挂载设计经验。

加载候选e35ff09已随2.7.0安装；2.6.0当时的加载失败记录保留在[evidence/2.6.0-mount-loader-repair.json](../evidence/2.6.0-mount-loader-repair.json)。最新实测和未构建候选见上方“2.7.0实测后的挂载修订候选”，不能把2.6.0旧状态当作当前安装情况。

## 2.5.0诊断测试版交付

用户授权整合分段诊断、安全菜单总开关、快捷结果/错误悬浮提示，并暂时禁用锁定图标Mod。基线evidence/2.4.0-installed.json，当前完整产物Temp/build-2.5.0，回执为[构建](../evidence/2.5.0-build.json)、[检查](../evidence/2.5.0-tests.json)、[安装](../evidence/2.5.0-installed.json)及[交付范围](../evidence/2.5.0-diagnostics.json)。15项运行文件和123项保护文件核对通过；模组/存档完整备份backup/AC7MultiSP-20261017-090610-898203，个人配置原字节保留，总开关仍false。没有自动启动游戏、提交main、制作ZIP或推送。

总开关仅在当前世界中的可见白名单入口应用；管理面板临时借用输入的真实页面可作为依据，隐藏/失效/其他世界的主菜单不算安全。实际函数检查覆盖任务拒绝、管理借用输入、成功清理后启停、清理失败保持停用且不反复重试。界面实际提示/焦点和引擎生命周期仍待用户实测。通知只在显示期间绘制，不抢输入；错误摘要去掉Lua路径/行号、截UTF-8边界、持续错误去重，完整内容保留原日志/管理页。

锁定图标文件~~!UI (Lock-on Crosshair) Hex!~~_P.pak已移至backup/disabled-lockon-20261017-090543，SHA256为5b9018904ba09135c29bd4e4dc18f40a6cb8518a119015fac672ca3e5ae65eb2。该目录disable-receipt.json含原位置和恢复方法；其他14项PAK核对未变，比例引导PAK仍禁用。用户测试时先从取证入口启动，在主菜单开启并保存总开关，待提示消失后分别在同一机体机库移动镜头、通关评分页停留30–60秒，反馈时间/页面/机体再分析分段。

## 本轮整理范围

- tests/run.py 运行来源拒绝检查、三SP纯规则、配置迁移/保存失败、协议应答、页面具名接口、冷却、指针、缓存恢复/挂载边界及DLL加载核对；旧双SP控制器/HUD/选装模拟不适用，未纳入当前通过范围。
- 2.2.2补齐页面激活、机库组件重建与开场定位；只向相同身份组件继承原生请求，空池等待，真实异常回退，保持既有兵装事务；检查覆盖21种组合、配弹边界、单多切换、失败隔离和补给交接，不能替代未列出的引擎和故障注入场景；2.4.0已获用户统一实测通过反馈。

## 支线整合与恢复

2.4.0按差异整合两工作区的最新未提交候选。挂载研究保留原分支最新网格替换头、纯布局和恢复处理，并保留核心整合分支的全场景F7模式5；持续效果研究带入continuous_sp.hpp和核心接入，保留核心协议4比例配弹。原工作区不写入、不提交、不清理；研究记录分别见evidence/mount-layout-research-latest.json、evidence/ecm-tls-persistence-research.json。已完整编译、通过适用检查并安装，产物Temp/build-2.4.0。

三个快捷键配置在shortcuts中保存；UI/设置协议升6，原管理键整数只保留主键镜像。检查使用既有模块扩展，覆盖全修饰键匹配、冲突、旧配置备份迁移，以及持续引用数组的定向移除；不把合成检查当真实效果验收。

2.3.1的早期候选布局已由2.4.0按用户反馈修订；旧研究记录保留原字节，不能作为当前布局说明。

待专项测试：用户在2.3.0报告机库预览转镜头明显卡顿，实验挂载开启和关闭均出现；记录为性能疑点，未证明根因。后续在同机型/涂装/分辨率/镜头路径下对照完全禁用mod、mod开启但模型关闭、模型开启的帧时间，分离页面扫描、文件发布及模型巡检耗时；本轮不改变性能策略。

`codex/pending-ui-polish` 保存 f800518、07c81c0、7e2fe4a 三个提交；当前开发分支已按改动点吸收标题、弹药高亮和中央提示，配置/UI逻辑按2.0.0模块结构实现，未整枝覆盖主线，也未发布。

机库与补给模型研究结论见[架构](architecture.md)对应章节。已只读采样F-15E机库和ms06补给，确认补给展示任务玩家本体，旧显隐钩子保留进补给前多挂状态。实现须加入明确的预览显示所有权，不能只停周期更新或直接复用任务兵装会话。后续定向采集候选前后、起飞交接、其他机型模型池和F-35C动画驱动证据；2.2.2已接入独立视觉上下文与页面生命周期并完成编译、检查、安装；2.2.2已有用户“基本没看到问题”的实测反馈，但没有逐机型/任务明细，不能扩大验证范围。
原根目录36个未提交文件存于提交 `0637abc` / 标签 `archive/20261016/three-sp-research`。其余旧分支有同前缀恢复标签；完整映射在[仓库整理回执](../evidence/repository-consolidation-20261016.json)。
五个旧工作区保留本地产物并锁定，只有 AGENTS 有封存提示；不在那里继续开发。备份位于 backup/maintenance-20261016，含已核验 bundle。

历史说明已从当前目录移除，完整整理前快照为 `archive/20261016/docs-before-prune`。需要某个旧结论时，用 `git show <标签>:<当时路径>` 定向读取；不要恢复整套历史文档。
恢复代码应从归档标签新建 codex/ 分支，不重置 main。通常无需查历史，先读当前架构和相关代码。

## 文档分工

AGENTS 只维护状态/规则；architecture 维护模块和实现约束；本页维护工具/恢复；两个 TXT 供用户及打包器。
按用户要求，独立的[挂载设计经验](multi-sp-mount-design.md)统一维护数量概念、三机型七组合、取舍依据及扩机型流程，吸收原架构中的布局说明。后续机体结论在同一文档扩展，不再另建相似挂载专题、状态页或交接流水账。docs/research/three-sp-candidate.txt仅保留旧工具所需的退役指路，不作为有效设计说明。必要回执按[证据索引](../evidence/README.md)读取。
