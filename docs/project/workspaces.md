# 工作区与成果索引

本页是任务登记入口。当前正式版本见[README](../../README.md)，分支/提交规范见[开发规范](workflow.md)。已封存路径不要继续编辑，恢复从main新建任务；重要半成品见[研究索引](../research/README.md)。

## 正式入口与明确保留的工作

| 任务/状态 | 分支与责任 | 路径或来源 | 范围与下一步 |
|---|---|---|---|
| F-22队列集成交付，进行中 | 本集成聊天；基线main/98d7e6a；来源研究树未提交差异 | C:/Users/83717/.codex/worktrees/integrate-bay-beta5/AC7_MultiSP | 接收双挂六点和XSDB四点队列候选，保留34机、92资源；按作者要求不修改更新日志，待构建及实机验证。 |
| 内舱支架集成交付，已出包待验证 | 本集成聊天；基线main/bda3af2；来源789c6fd及研究树未提交差异 | C:/Users/83717/.codex/worktrees/integrate-bay-beta4/AC7_MultiSP | 接收F-22及普通Su-57支架收拢与会话保护，保留34机、92资源；作者要求不修改更新日志，已完成构建及签名封装，待作者实机验证。 |
| 内舱反馈集成交付，已出包待验证 | 本集成聊天；基线main/0ea42f2；来源a00e6d7 | C:/Users/83717/.codex/worktrees/integrate-bay-beta3/AC7_MultiSP | 接收F-22及Su-57反馈修订和崩溃记录，保留34机与92资源；beta3日志已获作者逐条确认，完整构建及签名封装完成，待作者实机验证。 |
| 内舱成果集成，已出包待验证 | 本集成交付聊天；基线 main/158f2bd；来源研究分支 c43f0e9 | C:/Users/83717/.codex/worktrees/integrate-2-26-2-alpha3/AC7_MultiSP | 接收F-22与Su-57后续候选，合并现有四机及92资源；beta2编号及两条中文日志已获作者确认；已完成完整构建及签名封装，待作者实机验收；源码、构建及验收分别记录。 |
| 集成交付：2.26.2 alpha2，已出包待测试 | codex/integrate-2.26.2-alpha2；基线 main/37b36a8，延续alpha1/53305aa | C:/Users/83717/.codex/worktrees/integrate-2-26-2-alpha2/AC7_MultiSP | 接收外挂模型6e3581f及故障恢复733aa35，中文alpha2日志逐条获准；已构建常规/开发两包，提供默认关闭的故障入口和离线控制工具；已完成测试前alpha1快照及PAK摘要，单面板快速五轮自动归档；T6漏触发已修正并独立出开发包，作者确认五轮及四机模型、炮类HUD亮度通过；视角待测；本轮已修Super Kai的IEWS源码，接收1b56361与f59e81f，合并为33机/231决策/92资源。另提供当前四分页中文设计稿及提示清单，待作者审核；当时未编译；随后按作者授权启用XFA-36A/F-17V/Gleipnir真实冲突测试组，提供保存日志后恢复B组入口；现已按作者批准接收XFA-36修复、手柄及更新通知调整并编译整合修订EXE，七项设计稿中文仍待审核。环境/流程见[故障测试指导](../engineering/aircraft-fault-testing.md)，不自动启动游戏或执行实机用例 |
| 机体故障隔离与启动检查，源码待验证 | 本聊天；codex/feat-aircraft-fault-recovery；基线main/9075581 | C:/Users/83717/.codex/worktrees/aircraft-fault-recovery/AC7_MultiSP | 已编写机体级退让、F6诊断、换机恢复及严重错误提示；启动加载与更新结果分开提示，兵装异常在第一页查看。十二项中文词条经作者两次确认，同步英文与生成表；边界见[架构](../engineering/architecture.md#机体故障退让与f6诊断alpha2候选)。补充[测试指导](../engineering/aircraft-fault-testing.md)及本机TXT/原始日志归因核对；人工注入入口未实现。仅静态审阅，未编译、测试、安装或操作游戏，未合main |

| 官方内舱与活动机构多SP续研，进行中 | 本聊天；codex/research-internal-bay-multisp；基线main/37b36a8，正式2.26.1；源码候选074fdc6 | C:/Users/83717/.codex/worktrees/internal-bay-multisp/AC7_MultiSP | F35收尾已迁入共用模块候选，九机七组合路线见挂载正文。作者启动的实际2.26.2 alpha1只读现场确认MSL下swc1/swc2/swcc残留全开；同实例开启显示并再次切换后全部收闭、姿态更新，见[记录](../../evidence/f35-bay-live-observation-20261028.json)。普通Su-57作者确认关舱正常，两种显示状态均有切换日志，逐次采样仅覆盖开启末轮；见[Su-57记录](../../evidence/su57-bay-live-observation-20261028.json)。随后已制作Su-57完整七组合源码候选，29机/203组合、77资源不变；新外观开关限定连续性，关显示沿既有正常行为；F35候选效果未验证。F22单SWP3七组合接线草案已登记，尚未注册，等待原生现场。范围：舱门模块、核心分类、局部机构池参数、生成表与Lua资源清单及对应工程/研究/资产索引；不改版本、玩家中文、冻结DLL或其他线程；未编译/测试/安装，未写游戏内存或调用游戏函数 |
| X-02R与三个J-20启用，用户已验收 | codex/chore-enable-x02r-j20；基线 main/a0f4073 | C:/Users/83717/.codex/worktrees/install-x02r-j20/AC7_MultiSP | 由A切至B，四个Mod六份PAK与原包一致。作者2026-10-28确认X-02R A/B、J-20A/B/S及RH-172功能均正常，见[用户验收](../../evidence/mod-aircraft-user-acceptance-20261028.json)；功能与未知项维护在[机体正文](../research/mod-aircraft-compatibility.md#2026-10-28启用x-02r与三个j-20)，原启用回执保持；本轮仅记录反馈，未启动游戏、编译或运行测试 |
| 机体兼容反馈与资料提醒，已接收 | 本聊天；codex/docs-mod-compatibility-observations；基线main/41993d7 | C:/Users/83717/.codex/worktrees/mod-compatibility-observations/AC7_MultiSP | 记录RH-172补给冲突反馈和启动时兵装资料提醒待办；仅文档静态核对 |
| Gunplay一键启停，待用户验证 | codex/feat-gunplay-toggle；基线 main/052b8bc | C:/Users/83717/.codex/worktrees/gunplay-toggle/AC7_MultiSP；入口 E:/AC7_MOD/管理入口/一键切换Gunplay.cmd | 为现有A5主文件提供低优先级启停入口，保护备份和其他Mod；范围与使用说明见[其他 Mod 管理](../archive/other-mod-library.md)，未实际运行启停或游戏 |
| Mod机体测试前查询，已接收 | 本聊天；codex/docs-aircraft-nexus-precheck；基线main/e78672b | C:/Users/83717/.codex/worktrees/aircraft-nexus-requirement/AC7_MultiSP | 在AGENTS登记测试前查询作者Nexus页面、武器效果与皮肤差异的要求；仅文档静态核对 |
| 其他 Mod 目录简化，已接收 | codex/chore-simplify-mod-library；基线 main/b5bac08 | C:/Users/83717/.codex/worktrees/simplify-ac7-mod-library/AC7_MultiSP；本机入口 E:/AC7_MOD | 机体大全单独存放，其余原包统一归入一个目录；重复副本经摘要复核后送回收站，详见[其他 Mod 管理](../archive/other-mod-library.md) |
| 其他 Mod 资料与管理入口整理，已接收 | codex/chore-consolidate-ac7-mods；基线 main/800f858 | C:/Users/83717/.codex/worktrees/consolidate-other-mods/AC7_MultiSP；本机入口 E:/AC7_MOD | 整理其他 AC7 Mod 原包及管理快捷方式，保护现有游戏部署、Vortex 和历史依赖；位置与保留原因见[其他 Mod 管理](../archive/other-mod-library.md) |
| 集成交付：2.26.2 alpha1，待验证 | 本集成交付聊天；main/800f858；codex/integrate-2.26.2-alpha1 | C:/Users/83717/.codex/worktrees/integrate-20261025/AC7_MultiSP | 接收视角线程5cd8df0及ede049a、HUD a3f591a、IEWS 30b167d，统一公共weapon_names.lua、版本和登记正文；中文更新日志标题及四条变化已获作者逐条审核批准，原功能说明及NEXT帮助按作者既有决定保留。[源码接收](../../evidence/2.26.2-alpha1-source-integration.json)和[EXE交付](../../evidence/2.26.2-alpha1-exe-delivery.json)分别记录源码与构建。已核对RH-172此前安装，待作者安装候选包及实机验收；未运行测试、安装候选、操作游戏、推送或上传；正式版仍为2.26.1 |
| 视角平滑与连续环绕，已接入alpha1待验证 | 本目标切换聊天；基线main/800f858（2.26.1）；codex/research-camera-orbit-smoothing | C:/Users/83717/.codex/worktrees/camera-orbit-smoothing/AC7_MultiSP | 独占src/camera_hold.hpp、唯一研究正文及本行/文档索引/研究入口/资产索引本任务行；用户2026-10-26明确开始编码、不编译。已实现右杆输入连续交接、水平跨后方及普通座舱输出，机制与待验证范围见唯一研究正文；来源ede049a已与VR5°来源一并接入2.26.2 alpha1；机制保持独立，不改玩家中文文案。只做静态审阅，不编译/测试/安装，不替用户操作游戏或联系其他聊天 |
| VR模式5°条件生效，已接入alpha1待验证 | 本目标切换聊天；基线main/800f858（2.26.1）；codex/feat-vr-target-center-gate；功能提交1c9f68b，来源5cd8df0已纳入2.26.2 alpha1 | C:/Users/83717/.codex/worktrees/vr-target-center-gate/AC7_MultiSP | 独占src/view_target.hpp的NEXT角度门槛与白点共用判断，维护架构对应专节及本行/研究入口；已做人工静态审阅、差异格式及保护DLL哈希核对。作者逐项决定功能说明和F6帮助均保留原文，故NEXT相关两种语言词条保留；集成更新日志另在本聊天获准后同步，不再等待此帮助文案审核。不改版本、协议、配置或其他公共模块；本次始终不编译/安装，不写/运行测试，不操作游戏或推送 |
| HUD炮类名称亮度，已接入alpha1/待验证 | 本HUD优化聊天；基线main/800f858（正式2.26.1）；codex/feat-hud-dpad-brightness | C:/Users/83717/.codex/worktrees/hud-dpad-brightness/AC7_MultiSP | 当前仅修改native_hud.lua、weapon_presentation.lua、weapon_names.lua，使PLSL/MGP/HGP两处名称保持正常亮度；唯一设计见[架构HUD正文](../engineering/architecture.md#任务hud与中央武器简称)。十字键图标已按作者要求撤回，原实现锚af1b01b及本树Temp/tasks/20261026-hud-dpad/示意原料保留，只有作者重新提出需求时再恢复。已静态审阅，未编译/测试/安装/实机验证/推送；版本、协议及玩家中文源未改 |
| 原版IEWS名称修复，已接入alpha1/待验证 | 本CFA-44/IEWS问题聊天；基线main/800f858；codex/fix-iews-display | C:/Users/83717/.codex/worktrees/iews-display/AC7_MultiSP | 已统一CFA-44、Morgan、XFA-27的选装/HUD/F6及快速任务基础目录简称；按作者选择仅显示IEWS，不新增中文全称。独占weapon_names.lua、main.lua、loadout_flow.lua、native_loadout.lua、quick_mission.lua、overlay.cpp、quick_mission_panel.hpp及[架构名称规则](../engineering/architecture.md#兵装显示代号与内部代号)；内部代号、效果、版本及中文资源未改。静态差异核对完成，未编译/测试/安装/推送；不能视为完整电子战实战效果已验证 |
| 集成交付：2.26.1正式发布材料，已关闭 | 本集成交付聊天；基线main/19b1528；codex/integrate-20261025 | C:/Users/83717/.codex/worktrees/integrate-20261025/AC7_MultiSP | alpha3已验收；作者四份中文原字节接收、英文全部重译，中文逐条审核规则已登记。已完整构建正式EXE与通用ZIP并静态回读实际嵌入正文；Pages历史升序至2.26.0（含2.25.X），最新2.26.1和R2单入口词条获作者逐条批复。材料在Release/2.26.1，见[EXE/ZIP](../../evidence/2.26.1-exe-delivery.json)、[网页](../../evidence/2.26.1-pages-delivery.json)和[正文同步](../../evidence/2.26.1-document-sync.json)。作者已确认发布，见[发布确认](../../evidence/2.26.1-publication-confirmation-20261025.json)；本地main同步正式清单及当前规则，发布包不入Git。未独立联网核验，不推送/编译/测试/安装/操作游戏；原包与重复赞助素材保留 |
| F6状态发布性能优化，已接收 | codex/fix-f6-publish-performance；基线main/19b1528；来源56a6ba8 | C:/Users/83717/.codex/worktrees/f6-publish-performance/AC7_MultiSP | 轻量飞行状态与独立管理列表已由性能分支带入正式2.26.1；原alpha1交付保留。alpha3获作者总体正常反馈，后续整合由上方集成交付行统一负责，不修改来源树 |
| 任务内与原生选装性能，已接收 | codex/perf-runtime-preview；main/19b1528；最新73366ce | C:/Users/83717/.codex/worktrees/runtime-preview-performance/AC7_MultiSP | 包含F6来源、671b664及EML/公共视角/对象与模型读取优化；全部tip经alpha3纳入正式2.26.1，来源树保持干净。alpha2反馈、采样与后续验证只在[性能评估](../research/runtime-performance.md)维护；作者已确认CFA EML及F-2A普通飞行优化有效，具体观察与边界见该正文 |
| 本体/DLC剩余外挂机体多SP适配，待验证 | 本聊天；codex/feat-base-dlc-multi-sp；基线main/37b36a8（2.26.1） | C:/Users/83717/.codex/worktrees/base-dlc-multi-sp/AC7_MultiSP | 本批独占布局来源/资源生成、挂载准入与开场组合查询、资源打包/来源冻结衔接及对应工程正文。A-10C/Super Kai、F-104C及MiG-21bis三种双SP已有源码；新增15资源，完整输入92资源/184成员。MiG-21单MGP将原生两槽取位归至左炮口，保Gunplay实际伤害、弹道和双路扣弹；已只读采集双吊舱现场，未验证新单吊舱实效。两机三SP原生退让。详见[挂载设计](../engineering/multi-sp-mount-design.md#剩余外挂机体候选未发布待验证)。跳过内置弹舱，未编译/运行测试模块/安装/注入游戏输入/推送，未改玩家中文或版本号 |
| 正式主工作区 | main；接收/整合任务 | E:/AC7_MultiSP | 正式源码及统一文档；不要让研究线程直接并行写入 |
| 通用兼容流程及CFA-44机腹舱，已接收 | codex/refactor-compatibility-pipeline；基线main/19b1528；985cfdc及b95afa2 | C:/Users/83717/.codex/worktrees/compatibility-pipeline/AC7_MultiSP | 共用契约、局部故障与Root责任、单路机腹舱策略已纳入正式2.26.1；唯一规则见[挂载设计](../engineering/multi-sp-mount-design.md#cfa切换与ecm当前策略)。作者确认alpha3两侧模型开关下弹舱正常、其他功能总体正常；异常故障路径未逐项验证；来源树和旧原料保留 |
| 原生选装/补给固定标签英文修订，已接收 | codex/fix-native-ui-language；基线main/19b1528；7cec8b6及3d9ceb9 | C:/Users/83717/.codex/worktrees/fix-native-ui-language/AC7_MultiSP | 指定标签明确取英文，描述/提示保留语言选择，同步双语用途边界；经alpha3完整构建及作者总体正常反馈，纳入正式2.26.1。来源树、main未提交修改与旧包保持 |
| 免费Mod保护A阶段，alpha5由集成接收 | codex/feat-author-signing；来源6ce288182c3d15e1b921e59e94365569b03689e6 | C:/Users/83717/.codex/worktrees/research-mod-protection/AC7_MultiSP | 签名/精简源码由集成交付任务接入，复用已保留的唯一密钥并登记公钥；研究树及恢复件原样保留。实际边界见[交付方案](delivery.md#a阶段作者签名)，B未实施 |
| CFA-50检索与F6退格隔离，源码已接收待验证 | 本安装器/F6反馈聊天；基线026dd43；codex/fix-cfa50-aircraft-name | C:/Users/83717/.codex/worktrees/cfa50-aircraft-list/AC7_MultiSP；现行源码main | debug_aircraft.lua读取游戏正式名称，overlay.cpp补齐预读/原始键盘退格隔离；[定位记录](../../evidence/f6-aircraft-input-source-20261024.json)确认旧修复已合入且已安装UI与2.25.2交付一致。cc924a5已纳入2.25.3 alpha1完整构建，未安装或实测，现成2.25.2产物不含修订；CFA-50 EML研究范围不变 |
| 双语主页与发布流程，已接收待作者上传 | 本管理聊天；基线c1092c0；codex/integrate-pages-workflow，18eea51已合main；工作树受置顶保护保留 | C:/Users/83717/.codex/worktrees/integrate-pages-workflow/AC7_MultiSP；材料E:/AC7_MultiSP/Temp/tasks/20261024-release-2.25.2/ | 接收“mod内置更新”的未提交主页，集中双语词条并保留preview；按用户正式2.25.2确认整理[完整发布流程](release-workflow.md)，工具准备EXE、Nexus通用ZIP和整站材料，四处上传由作者完成。原ZIP、来源树及冻结产物不改，见[接收回执](../../evidence/pages-workflow-intake-20261024.json)与[发布准备](../../evidence/2.25.2-publication-preparation.json) |
| 集成交付：2.26.0验收、main收尾与GitHub同步 | 本集成交付聊天；本轮基线main 2b79f59；codex/integrate-20261024 | C:/Users/83717/.codex/worktrees/integrate-20261024/AC7_MultiSP | 作者确认安装包与Mod功能正常，原样接收三份手改TXT并同步生成文字，[验收与文档记录](../../evidence/2.26.0-main-acceptance-20261025.json)维护边界。本轮获授权仅向私有GitHub推送main，推送结果以远端回读为准；不重编/重包/测试/安装，不改其他未提交文件或历史回执 |
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
