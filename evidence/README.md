# 按需证据

- [2.12用户实测](2.12.0-user-validation.json)：F22无肉眼卡顿，ADF01停光取舍、三机布局与X02撤销要求；[2.13布局](static-expansion-2.13.0.json)已获追加任务重开换机及编译安装授权，已成套安装，见[本版整合](2.13.0-integration.json)和[安装回执](2.13.0-installed.json)，真实效果待用户实测。

- [2.12.0交付](2.12.0-integration.json)：本轮反馈修订、Gripen原生腹挂及X02S EML保留，完整编译/检查/安装；[本轮用户反馈](2.11.0-user-validation.json)、[静态覆盖源](static-expansion-2.12.0.json)。CFA首发双侧蓝效仍待专项，实机待复测。

- [2.11.0交付](2.11.0-integration.json)：新增十机静态70组合，完整构建/检查/安装通过，39资源/78成员；[源数据与47机分类](static-expansion-2.11.0.json)保留23资源回读及16项后置，实机待测。

- [2.10.0交付](2.10.0-integration.json)：F22切换查询优化与四机轻量外观，完整构建/检查/安装通过，真实观感与性能待测；[2.9.0用户反馈](2.9.0-user-validation.json)。

- 2.9.0机外保留候选：[范围/旧方案归档](2.9.0-integration.json)、[构建](2.9.0-build.json)、[检查](2.9.0-tests.json)、[安装](2.9.0-installed.json)。16运行文件、32成员PAK、141保护文件核验，已安装，效果待测。Su47/TGM原生，普通Su57 PLSL/F22侧8AAM保留，F35保持。

- [2.8.1用户实测与方向调整](2.8.1-user-validation.json)：TG/Su-47仍严重掉帧，F-22A偶发低帧及切换卡顿/重复开舱；改为机外连续、舱内原生。普通Su-57 PLSL与F-22A侧8AAM优先，CFA-44专项，X-02S退出外观路线。只评估未实施。

- 2.8.1性能/同舱切换候选：[范围与限制](2.8.1-integration.json)、[构建](2.8.1-build.json)、[检查](2.8.1-tests.json)、[安装](2.8.1-installed.json)。已安装，16运行文件、32成员PAK、138保护文件核验；帧率和舱门效果待实测。F-22A分舱、Su-47左右新布局未完成。

- [Su-47真实全开端点几何](su47-native-endpoint-geometry.json)：全LOD静态复核及严格零偏差参照，仅覆盖记录的全部38根Body骨姿态，不证明全姿态或运行调度。
- [产品反射与发布依据](sp-animation-product-access.json)：Tick、Socket反射、原生发布与加载器导出依据；具体产品改动以源码及交付回执为准。

日常状态见 [AGENTS](../AGENTS.md)，无需通读本目录。

- [2.8.0用户实测](2.8.0-user-validation.json)：三机任务1/7fps，F-22A/TG切SP反复开合；Su-47左右分区建议及轻微穿模偏好。记录观察范围，不称完整根因或修复通过。
- [F-22A/TG舱域与F-35C对照](sp-bay-domains.json)：66d4f3d的逐骨通道、TG静态点共舱及完整切换补还边界；F-22A专用表路由仍缺独立输出写端与资源，不能当作已分舱。

- [三机接入的F-22A运行采样](animated-runtime-f22a-capture.json)：用户配合单SP8AAM暂停态与45秒只读连续采样，记录实际14槽／8窗口、三组持久池、反射参数及骨骼更新字段。修正NoSkeletonUpdate／Pause位和全开请求语义；原始内存与函数片段仅在Temp；三机接线与本轮交付见2.8.0回执。

- 2.8.0保守测试候选：[来源/限制](2.8.0-integration.json)、[构建](2.8.0-build.json)、[检查](2.8.0-tests.json)、[安装](2.8.0-installed.json)。已安装，交付时未自动启动游戏；用户随后反馈新三机任务严重掉帧，见上方用户实测；Su-47旧方案仅精确已核全开姿态可视。
- [2.7.2布局源码修订](2.7.2-layout.json)：F-2A LASM内侧/HVAA外侧，新增挂架三LOD编码往返；该记录保留源码准备时状态；修订已随2.8.0成套安装，主管接手见开发说明。
- [2.7.1用户实测](2.7.1-user-validation.json)：切换/发射、Rafale/Typhoon与ADF-11F反馈正常；F-2A所有多挂外观组合仍卡，关闭模型恢复，待专门研究。
- 2.7.1两线修复：[来源/范围](2.7.1-integration.json)、[构建](2.7.1-build.json)、[检查](2.7.1-tests.json)、[安装](2.7.1-installed.json)。F-2A切换/重复巡检、Rafale名称匹配、Typhoon双挂及UAV引用扩容已安装，用户反馈见上条，不能外推全部场景通过。
- [2.7.0挂载实测与源码修订](2.7.0-mount-feedback-repair.json)：F-2A忙时等待及性能候选、Rafale名称绑定、Typhoon双挂补位；该回执保留研究时状态，最新成套安装见2.7.1回执。
- 2.7.0三线整合：[来源/范围](2.7.0-integration.json)、[构建](2.7.0-build.json)、[检查](2.7.0-tests.json)、[安装](2.7.0-installed.json)。四机挂架加载修复与ADF-11F UAV/TLS运行候选纳入，XFA-27保留；实机加载/持续/回收待用户验证。
- [ADF-11F研究](adf11f-uav-research.json)：上游源码候选和原生UAV采样依据，未编译/安装字段属于研究时状态。当前交付看2.7.1回执；不能把原生切MSL采样当跨SP新代码通过。
- [SP切换动画研究](sp-switch-animation-research.json)：e519d02的47机筛查、16机29个活动挂架、49序列及15机105组合关联。策略runtime_enabled=false，新增运行准入0，未在本版启用。


- 2.6.0挂载扩展：[来源/用户反馈/评分分段](2.6.0-integration.json)、[构建](2.6.0-build.json)、[检查](2.6.0-tests.json)、[安装](2.6.0-installed.json)。整合77e9ffc十机70条方案，16独立挂架副本32成员回读及纯布局/队列检查通过；已收到三机正常、四机加载回退、两机DLC未测反馈；Typhoon未反馈，评分优化另待实机。下列研究JSON保留研究当时的未编译/未安装状态，不作为当前交付状态。

- [2.6.0挂架加载修复](2.6.0-mount-loader-repair.json)：四机第一份独立资源均未加载，结合e35ff09接入登记表诊断和原生AssetData按路径加载候选；记录用户三机正常、四机失败、两机DLC未测及Typhoon未反馈。源码候选未编译、运行测试或安装，不改写原安装回执。

- [动画机体布局覆盖层](mount-expansion-animated.json)：深入核对8机，F-22A／Su-47／Su-57TG三机各七组合，共21条完整离线布局。Su-47采用严格全开端点ammo显隐，其余五机保留整机暂缓依据。含25份骨架网格49个LOD的结构核对、101键几何摘要与源指纹；不启用运行白名单。
- [原生SP动画研究](sp-switch-animation-research.json)、[限定共舱控制依据](sp-common-bay-control.json)：引用动画线程的原字节数据与三个控制头；控制profile不代替整机几何准入，产品Host及运行调度待接入。


- [原版挂载扩展目录](mount-expansion-catalog.json)：47架有武装原版机型的真实代码、三SP类、组件/网格引用、来源与分批建议；排除无武装f104av和第三方机体。仅资源核对，不代表全部适配。
- [首批挂载方案](mount-expansion-batch1.json)：六机各七组合；吸收7ae97b6的静态分区依据后，F-16C、F-2A、Mirage、Rafale四机有完整七组合及精确载体表，共28条，含14条分区候选。F-104C缺第三站、Gripen的LACM支撑未明，继续整机暂缓。
- [第二批挂载方案](mount-expansion-batch2.json)：十五机各七组合；区分原生单挂参考、现成整架复用研究候选和暂缓组合，保留净空/支撑依据与不足窗口取舍。三批均为离线研究，未编译、运行测试或安装；可读表及整合要求统一见[挂载设计经验](../docs/multi-sp-mount-design.md#其他原版机体的分批研究)。

- [第三批挂载方案](mount-expansion-batch3.json)：剩余二十三机各七组合；69条单挂参考、22条现成资源候选、70条暂缓。保留静态／骨架挂点、原生通道、固定发射器／效果源／UAV角色和逐项动画／资源／支撑阻塞，包含CFA-44；仅F-15C、XFA-27、ADFX-01三机七组合齐全，其余整机暂缓。
- [全机型交接索引](mount-expansion-handoff.json)：47机研究覆盖；旧静态路线十机70条；animated_design_overlay另列三机21条动画布局，合计13机91条完整候选，其余31机暂缓。`integration_candidates`含40条多挂（26原资源＋14已核对静态分区），附精确分配、载体及前提。整机七组合门槛不变，未编译、测试或安装，无安装包。

- [静态分区往返依据](sp-combination-static-split-experiment.json)：技术线程7ae97b6的原字节回执，22副本／66个LOD完成资源及改名编码往返，未验证游戏加载、渲染或动作；本轮四机选用其中16副本／48个LOD。实验`integration_allowed=false`保持原样，不能直接安装。

- [F-15E补给模型观察](resupply-model-observation-20261016.json)：ms06补给展示任务玩家本体，保留入场前多挂状态；只读采样及实现约束，不代表动态预览已实现。

- 2.4.0正式版（main/v2.4.0）：[构建](2.4.0-build.json)、[检查](2.4.0-tests.json)、[安装](2.4.0-installed.json)；三个可配置快捷键及两支线最新候选，完整编译、检查和安装核验通过，比例引导PAK按用户要求移入备份禁用；三机型布局获用户认可，反馈范围见[挂载设计经验](../docs/multi-sp-mount-design.md#当前结论和验证范围)，用户随后确认全部改动及两支线成果正常，授权正式提交main，见[用户验证](2.4.0-user-validation.json)。
- [挂载最新修订](mount-layout-research-latest.json)、[ECM/TLS研究](ecm-tls-persistence-research.json)：2.4.0吸收的原研究证据；其中未编译/未安装状态描述属于研究当时，不可当作整合后的实时状态。
- 2.3.1整合候选：[构建](2.3.1-build.json)、[检查](2.3.1-tests.json)、[安装](2.3.1-installed.json)；扩展F7到机库/补给/暂停，合入挂载支线布局。完整构建、检查及安装核验通过；用户确认游戏已关闭后安装，未自动启动游戏，新场景和布局待实测。
- [挂载布局研究](mount-layout-research.json)：独立支线的原始研究记录，未编译/未安装描述属于采集当时；2.3.1按差异吸收候选，不能引用旧版回执当作新布局实测。
- 2.3.0开发测试版：[构建](2.3.0-build.json)、[检查](2.3.0-tests.json)、[安装](2.3.0-installed.json)；新增Ctrl+Alt+F6组合轮切及Ctrl+Alt+F7即时模型开关，完整编译、检查和安装核验通过，未自动启动游戏；用户已确认任务内两快捷键正常，机库F7限制在2.3.1扩展。
- 2.2.2开发测试版：[构建](2.2.2-build.json)、[检查](2.2.2-tests.json)、[安装](2.2.2-installed.json)；补齐页面激活即时刷新、同机体组件重建和开场玩家定位，保留失败回退；用户于2026-10-16实测反馈“基本没看到问题”，授权本地提交，未提供逐机型/任务明细。
- 2.2.1开发测试版：[构建](2.2.1-build.json)、[检查](2.2.1-tests.json)、[安装](2.2.1-installed.json)；修复机库跨页与补给/开场视觉交接，保留自动回退，待用户复测。
- 2.2.0开发测试版：[构建](2.2.0-build.json)、[检查](2.2.0-tests.json)、[安装](2.2.0-installed.json)；用户确认候选变化可见，但PART切页闪回、SKIN及起飞漏接；原回执保留。
- 2.1.1开发测试版：[构建](2.1.1-build.json)、[检查](2.1.1-tests.json)、[安装](2.1.1-installed.json)；完整编译、自动检查和安装核验通过，未自动启动游戏，实玩待用户验证。
- 2.1.0：[构建](2.1.0-build.json)、[检查](2.1.0-tests.json)、[安装](2.1.0-installed.json)；现场发现准备页弹药行层级误判，已在2.1.1修复，旧回执保留。
- 已验证2.0.0：[构建](2.0.0-build.json)、[检查](2.0.0-tests.json)、[安装](2.0.0-installed.json)；用户已确认[实测正常](2.0.0-user-validation.json)，为历史已验证基线；当前main正式基线为v2.4.0。
- 已验证2.0：[构建](2.0-build.json)、[检查](2.0-tests.json)、[安装](2.0-installed.json)；用户已确认[实测正常](2.0-user-validation.json)，已合入主线。
- [1.1.0 用户验证](1.1.0-user-validation.json)：实测反馈与稳定性边界。
- [1.1.0 构建](1.1.0-build.json)：来源/DLL/导出/语法核对，不代表实玩。
- [1.1.0 发布](1.1.0-release.json)：原 ZIP 摘要与成员。
- [仓库整理](repository-consolidation-20261016.json)：旧分支/工作区恢复映射；文档数量与路径是上一轮快照。

本次安装器以[2.3.1](2.3.1-installed.json)完整运行文件作为2.4.0升级基线，当前已安装2.4.0；安装保留个人配置原字节，首次启动迁至管理6。后续升级更新完整基线，不能重跑覆盖历史回执。旧记录保留原字节，研究工具需要时定向读取。
旧回执提及的说明可从 Git 历史读取，方法见[开发说明](../docs/development.md)。旧“当前”、PID、安装状态不能当作实时事实。
默认检索排除此目录，需要时按文件读或用 rg --no-ignore 定向查找。
