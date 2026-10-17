# AC7MultiSP

主工作区 `E:/AC7_MultiSP`；主线正式基线为用户实测通过的 **AC7MultiSP 2.4.0**，定位标签 `v2.4.0`。后续功能从 main 新建分支开发。开工先看 Git 状态。当前整合分支为`codex/menu-diagnostics-controls`；源码2.7.2、游戏安装2.7.1，二者必须区分。
按需读[架构](docs/architecture.md)或[开发说明](docs/development.md)；研究机体挂载先读[挂载设计经验](docs/multi-sp-mount-design.md)，无需通读证据和历史。用户操作见[安装说明](docs/安装说明.txt)，交付范围见[版本说明](docs/release.txt)。

新主管先读[主管接手入口](docs/development.md#主管接手入口)，任务分工、已验证范围与尚未整合的支线均在该节；不另建重复交接文档。

## 当前事实

- Steam build 9855922，单人战役/自由任务；main正式基线v2.4.0。已安装2.7.1修复测试版，成套产物Temp/build-2.7.1，16项运行文件、32成员挂架PAK与131项保护文件核验通过。用户本轮确认改动有效：F-2A切换/发射正常，Rafale M、Typhoon正常，ADF-11F反馈正常；未逐项列举全部场景。详见evidence/2.7.1-user-validation.json。
- 静态多挂共13机：原Su-30SM、F-35C、F-15E，以及F-2A、F-15C、F-15J、F-15 S/MTD、F-16C、XFA-27、Mirage 2000-5、ADFX-01、Rafale M、Typhoon。F-15 S/MTD、XFA-27因用户无DLC未测；不能宣称每机七组合全部实测。ADF-11F只增加TLS/QAAM/UAV功能共存，仍显示当前SP原生模型。0c13178的F-22A、Su-47、Su-57 Top Gun共21种动画布局与三个控制头已合入源码；产品Host、当帧Socket取位和调度尚未接入，新增运行准入仍为0。用户已授权本轮整合并安装测试，但研究提交本身不能直接生成这三机的可玩适配。
- F-2A性能未解决：用户2.7.1反馈机库、过场动画卡；任务单SP不掉帧，全部双/三SP掉帧，关闭多SP模型后不卡。可定位到多挂外观路径，尚不能认定仅由RKT或某一具体CPU/GPU环节引起。本轮只记录，之后专门研究。2.7.2源码已将LASM+HVAA双挂改为大LASM内侧、小HVAA外侧，并补齐HVAA外侧资源配方及三LOD编码往返；未编译、运行测试模块或安装。三挂外侧留给RKT，保留分侧内翼。后续同类方案优先遵循大弹内侧、小弹外侧，并核对支撑和离架净空。
- 用户确认 2.0“实测各个功能都没问题”，已合入 main。2.0.0完成配置/协议分离、页面具名接口、状态清理、挂载边界及文件写入失败检查；2026-10-16用户确认“测试没问题，可以提交了”，标签v2.0.0保留历史基线，当前正式基线为v2.4.0。
- 用户报告机库转镜头及评分页卡顿，实验多挂开启/关闭均出现；2026-10-17自行关闭总开关保存重启后，明确确认两处均完全不卡，已核对runtime_mod_enabled=false。应优先定位总开关启用的功能路径，不能只归因游戏或单凭此归因模型。此前menu回调平均9.416ms、峰值102ms（非总帧时间，未逐窗口标记页面）；发现评分页可能仍维护机库预览、周期配弹/页面搜索及重复VirtualQuery。总开关关闭仍运行的菜单许可/管理及加载器单独开销下调优先级，交互影响未排除。2.5.0分段在09:19:45明确记录CampaignDebriefing仍刷新后台机库：回调平均9.090ms，preview_scene平均5.712ms，pages平均2.972ms（嵌套、非总帧时间）。2.6.0仅在机库刷新模型/扫描选装，切页隐藏保留0.35秒；是否修复箭头回退感待实测。详情见开发说明及evidence/2.4.0-menu-performance-investigation.json。
- 用户2026-10-17评分页按F12后卡死无声已定位为取证CDB漏配80000003系统调试断点，现场栈DbgBreakPoint→DbgUiRemoteBreakin；保存转储并设置bpe日志+gh自动继续后恢复，用户确认画面声音和操作正常。当前会话与启动脚本均已补齐规则，未修改/编译/安装Mod；此事不能当作Mod崩溃或死锁证据。评分页原有卡顿独立待查，单次主线程样本在挂载预览身份校验，不能定因；细节见开发说明。
- 偶发退出持续关注：用户报告2026-10-16约23:50 ADF-11F进入SP选择页突发退出，重开同机型正常；首次退出没有对应异常栈，根因仍未定位。用户于2026-10-17确认23:51为重开测试正常后的手动关闭；该WER中的UE4SS+0x2f5957是关闭后清理时Input::Handler析构空指针；不能用它解释机库首次退出或把全部WER记录计成游戏内崩溃。调查摘要见evidence/2.4.0-hangar-exit-investigation-20261016.json，原始转储仅存Temp。本机已准备桌面“AC7 崩溃取证启动”及根目录启动入口，使用src/research/start_crash_capture.ps1和本机CDB按PID捕获首次异常、退出请求栈与日志，详见开发/安装说明；独立进程验证通过，真实游戏取证待反馈，未改运行代码、加载器或安装。
- 2.4.0仅对已核对的ADFX-01原生IEWS开放多挂并实验保留IEWS/TLS跨SP效果；其他ECM与UAV仍仅单挂，其他TLS跨SP仍结束；机库/补给动态预览已接入，本版已有用户基本正常反馈；完整绿/黄性能条未完成。后续先补原生流程/稳定性证据，再扩机型。
- 2.4.0已整合codex/mount-layout-research的三机型布局修订与codex/ecm-tls-persistence的ADFX-01原生IEWS/TLS持续效果，连同挂载设计经验提交main。研究工作区保留原样，用户已确认两支线成果正常；不扩大为全机型、所有任务或故障注入均验证。
- 用户要求禁用`proportional guided missile_P.pak`，已移至backup/disabled-guidance-20261016-231440，原PAK哈希和恢复路径见其中disable-receipt.json；其他模组保留。
- 2.4.0已作为本地正式版提交main并标记v2.4.0，未制作ZIP或远端发布；2.0/2.0.0均未制作ZIP；本地旧包 `Release/AC7DualSP-1.1.0.zip` 保持原发布字节。验证凭据按需查[证据索引](evidence/README.md)。

- 2.5.0支持主菜单/单人任务选择等安全入口切换总开关，任务/补给/机库保存后等待返回入口自动应用，无需重启；新增上方快捷结果约4.5秒、新错误/模型回退约6.5秒的通知，平时不绘制；管理通信协议7，配置管理版本仍6。15项运行文件及123项保护文件核对通过，安装时个人配置原字节和存档保留（该记录不代表之后用户开关状态），未自动启动游戏；见evidence/2.5.0-diagnostics.json。
- 用户要求暂时禁用锁定图标PAK，已移至backup/disabled-lockon-20261017-090543，恢复路径及原哈希见disable-receipt.json，其他14项PAK核验不变；比例引导PAK保持禁用。

- 2.6.0按77e9ffc吸收十机70条研究方案并接入共用运行层，16个副本仅保留Temp/游戏PAK，不提交游戏资源。安装回执evidence/2.6.0-installed.json，备份backup/AC7MultiSP-20261017-094359-167557。XFA-27原生ECM按真实类/虚表/冷却契约尝试跨SP保有；其他ECM/UAV限制保留。管理和提示26号。来源/检查/交付见evidence/2.6.0-integration.json。e35ff09加载修复已由2.7.0成套安装；上述修复已随2.7.1编译、检查并安装：F-2A可恢复切换/减重复检查、Rafale按Socket名绑定、Typhoon双挂补位；实机效果待复测，见evidence/2.6.0-mount-loader-repair.json。

- 2.7.0已整合e35ff09加载修复、codex/adf11f-uav-research的ADF-11F原生TLS/QAAM/UAV共存候选，保留XFA-27 ECM；动画e519d02设计/策略/证据已合并，但新机型运行准入仍0，不能宣称已适配。成套产物Temp/build-2.7.0，升级基线2.6.0，完整构建、检查与安装核验通过；备份backup/AC7MultiSP-20261017-102057-803131，具体状态见evidence/2.7.0-integration.json。

- ADF-11F原2.7.0两架UAV切TLS容量守卫失败已在2.7.1修复并安装；用户本轮反馈正常，具体边界仍按实测范围记录。保留公开FMemory/游戏GMalloc、先扩容再补引用、重读data与容量日志；不引入长期root。备份backup/AC7MultiSP-20261017-110205-136925。

## 工作规则

1. 中文交流。遇到资料或环境阻塞及时报告。
2. 2.4.0已获用户完整功能及两支线实测通过反馈，授权作为正式版提交main。默认不自动编译、新增或运行测试、安装、操作游戏；开发阶段的构建安装须在用户授权范围内。正式化沿用已实测字节，不重新构建；不自动推送或上传。
3. 新分支从 main 创建，前缀 `codex/`；旧工作区仅供恢复，不继续开发。未合入成果的定位见开发说明。
4. Lua唯一源`dist/DualSP/Scripts/`，当前源码2.7.2未构建；安装2.7.1成套产物在Temp/build-2.7.1。2.7.2新资源输入为Temp/mount-assets-2.7.2/source，仍16副本但名称/PAK将变化，须重新成套编译核心/UI/Lua/PAK，升级基线evidence/2.7.1-installed.json；不要把本候选Lua与dist保留的正式旧UI混装。2.4.0正式核心为`dist/DualSP/cores/2.4.0/DualSP.dll`，UI为`dist/DualSP/bin/DualSPUI.dll`，与Temp/build-2.4.0及已实测安装一致。2.0.0旧核心保留，仅与v2.0.0源码和当时UI配套，旧UI从Git标签恢复。旧`dist/DualSP/bin/DualSP.dll`不得改，SHA256为`D7D7055CA7909021EEACA15E63FF5FF253D2EB7E60A3FE3B79DCEEAB231DE339`。
5. 功能变化更新对应正文，不追加流水账或另建重复交接文档。默认只维护本入口、架构、开发、用户安装及版本说明；确有独立用途才新增文档。
6. pwsh7 优先，复杂文件处理用 Python；UTF-8，Windows 启动脚本沿用 BOM。临时文件 Temp，备份 backup；无 Git 文件修改前备份，清理送回收站。
7. 不提交个人状态、游戏资源、音频、第三方 PAK、工具、转储及 ZIP；遵守同一范围与文件所有权。
8. 用户要求克制使用subagent，优先已有具体工作线程。主管协调接入前先核对线程/工作树最新状态；主工作区核心、Lua与公共文档由主管统筹，支线不要整套覆盖旧版本。

默认检索由 `.ignore` 排除 evidence、references、vendor；需要时定向读取或用 `rg --no-ignore`。历史说明只留 Git，不作为日常预读材料。
