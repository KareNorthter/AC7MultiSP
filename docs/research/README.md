# 研究与待继续成果

本页登记未采用、已撤回、局部采用及尚待接收的成果。**“分支提交已合main”不代表该工作树的未提交文件或ignored原料已合入。** 这里的保留也不等于准许恢复运行、构建、安装或继续调查；后续按用户指定研究，从[工作流程](../project/workflow.md)和当前main接续。

当前产品机制见[架构](../engineering/architecture.md)和[挂载设计](../engineering/multi-sp-mount-design.md)，可复用教训见[工程经验](../engineering/engineering-lessons.md)。历史原文见[归档索引](../archive/README.md)及`archive/20261023/pre-governance-main`标签。本机来源位置只提供恢复线索，实际续研前仍须核对文件与摘要。

Mod机体兼容性已按用户指定从main新开`codex/research-mod-aircraft-compat`续接，旧未提交现场继续封存。最新7e255e1及前阶段名称/IRCM修订已进入2.25.3 alpha1完整构建；[交付与待测清单](../../evidence/2.25.3-alpha1-exe-delivery.json)明确尚未安装或游戏验收。用户随后确认XFA-33 B、Su-57 TGM公共舱互切修订未生效，已按指令暂缓；其他已测项无问题，范围见[反馈记录](../../evidence/2.25.3-alpha1-feedback-alpha2-source.json)。当前分组安装与待办只在[兼容性续研正文](mod-aircraft-compatibility.md)维护；用户已确认S-44/Arkbird此前问题解决，不再当作未修复。

游戏启动后的兵装资料不完整提醒尚待实现，RH-172补给与ZSD/Gunplay组合的用户反馈也已记录；继续条件和边界见[兼容性续研正文](mod-aircraft-compatibility.md#启动时兵装资料提醒待实现)。

通用兼容流程第一版从正式2.26.0/main `19b1528`另建`codex/refactor-compatibility-pipeline`，接入能力独立的故障账本、持续效果契约表和舱门资源策略；唯一设计及待验证范围见[架构](../engineering/architecture.md#通用兼容适配流程)，责任与路径见[工作区索引](../project/workspaces.md)。源码经alpha3集成和作者总体正常反馈，正式接收为2.26.1，不恢复暂缓适配，不覆盖旧研究原件。

同分支随后按用户更正处理CFA-44机腹舱：限定资源使用共用单路独立输出策略，Light旧复制写入者撤出。具体规则、原料来源与待验证项只在[挂载设计CFA专节](../engineering/multi-sp-mount-design.md#cfa切换与ecm当前策略)维护；作者已确认alpha3机腹舱表现正常，正式2.26.1沿用。

## 任务内与原生选装性能

[性能评估](runtime-performance.md)维护main/19b1528与F6 alpha1的基线、实现优先级、alpha2范围及统一测试步骤。alpha2首轮反馈有效；EML修订45a8c43后，继续实现公共视角/对象表读取、稳定相机资格复用、轻量挂架/模型校验批次及非CFA切换分段日志，均已纳入正式2.26.1，CFA EML持续掉帧与F-2A普通飞行优化获作者确认，验收边界见同页。原生选择槽重复取位、F-2A完整重绑/初始化及渲染成本继续保留，不将一次性加载当目标。

## 免费Mod署名与资源保护

[唯一研究正文](mod-protection.md)基于main 7431855及2.25.3 alpha1原交付静态盘点，给出同源文字/图片、离线签名、安装与按需核验、符号精简及可选混淆的边界、成本和继续条件。研究锚5593ae3；用户已选择A，源码候选位于codex/feat-author-signing，现行设计见[交付方案](../project/delivery.md#a阶段作者签名)。源码6ce2881由集成交付线程接入alpha5，现有作者公钥已登记；制作与验证见alpha5交付回执。B未实施，研究旧扫描保持原阶段含义，不分配正式版本。

## 挂载、弹舱与动画

2026-10-28从main/37b36a8（正式2.26.1）新开`codex/research-internal-bay-multisp`：优先将F35C关闭外观后缺失的舱门收尾迁入共用模块，并逐机评审剩余官方内舱/活动机构七组合。唯一方案、静态阻塞和游戏现场继续条件见[官方内舱续研](../engineering/multi-sp-mount-design.md#官方内舱与活动机构续研2026-10-28未发布)，逐组合来源见[静态记录](../../evidence/internal-bay-static-plan-20261028.json)。F35源码仅为未编译候选；其余新布局未启用，原Su-47/TGM掉帧与公共舱失败记录保持有效。

本轮作者操作的实际安装版为2.26.2 alpha1：F35关闭显示后旧通道残留及开启后的对照已采；普通Su-57作者确认关舱正常，两种开关状态均有切换日志。逐次采样覆盖及未采用草稿见[Su-57记录](../../evidence/su57-bay-live-observation-20261028.json)，不把数值回落自动列为动画故障，不扩大为完整新布局已通过。

普通Su-57完整七组合随后已成为[源码候选](../../evidence/pkfa-internal-mount-source-20261028.json)：静态GPB架分区、原双PLSL及一副SWP1局部租约，表增至29机/203组合且77资源不变。作者允许轻微外观贴合差异；任务内门禁、仅模型开启的共舱连续性和仍待实机事项见挂载正文，未编译/安装/测试。

多机舱门续研已从静态比较推进到限定源码候选，范围是普通SU-57/TGM、DarkStar、XFA-33 A/B/C；尚未编译、安装或实机验证。唯一行为说明见[挂载设计](../engineering/multi-sp-mount-design.md#多机舱门收尾源码候选待逐机验证)，待反馈项仍在上方兼容性续研正文维护。旧完整共舱/Tick接管方案未恢复。

| 主题/状态 | 成果锚 | 来源位置 | 已知问题与继续研究入口 |
| --- | --- | --- | --- |
| 完整内舱、共舱动画：已撤回，原料保留 | `archive/full-bay-mount-2.8.1` / `57fd5d3`；[动画研究](../../evidence/mount-expansion-animated.json) | `archive/20261023/pre-governance-main`标签内`docs/archive/*-before-2.16.txt`；主目录`Temp/build-2.8.1*`及相关工作树Temp，恢复前核实 | F-22/Su-47/Su-57 TGM完整几何思路仍可参考，但Su-47/TGM严重掉帧、空架/重复开合；现行保原生。从`src/research/build_animated_mount_candidates.py`、`build_animated_runtime_data.py`及来源证据查起，不直接恢复旧运行整套 |
| 三机产品动画访问适配：半成品，未证明可运行 | [产品访问证据](../../evidence/sp-animation-product-access.json)、[F-22采集](../../evidence/animated-runtime-f22a-capture.json) | `src/mount_animation_product_access.hpp`、`mount_animation_publication_tracker.hpp`、反射适配器；证据所列主目录`Temp/f22a-*20261017-1144*` | 证据`runtime_ready=false`；Tick=true不能证明骨更新或当帧发布，轮询不等于发布见证。NoSkeletonUpdate/Pause/Tick分别归还；从`src/research/summarize_animation_product_access.py`和已保存原码/反射继续核对，不强求值或清原生暂停 |
| F-22腹骨显隐：已撤回 | `archive/f22-belly-bones-2.11` / `9c5d3d2`；研究`8c83e00` | `E:/AC7_MultiSP/Temp/sp-switch-animation-research`及其Temp；旧正文 | 高频骨操作代价，现行保侧8AAM、腹舱原生。恢复时先比较原生骨发布和精确身份，不把Hide/UnHide能调用当完整生命周期成立 |
| X-02S EML/腹舱姿态：已撤回 | `archive/x02s-eml-2.12` / `0b11a26`；撤回`4005e1d` | `E:/AC7_MultiSP/Temp/cfa44-eml-effect-fix`与旧2.12构建/原资源，按旧归档清单定位 | 炮体与舱门冲突；当前用原生外观。继续先处理真实支撑、舱门净空与发射姿态，不能只恢复旧显示表 |
| CFA旧保开度/抑制互切动画：原方案撤回，后来仅局部重新实现 | `archive/cfa-animation-2.15` / `a4f7d1b`；旧取舍`8413328`、`92690a7` | `E:/AC7_MultiSP/Temp/sp-switch-animation-research`及旧2.15冻结；[2.19设计](../../evidence/cfa44-sp-animation-2.19.1-design.json) | 旧强收ECM公共进度会阻断原生发射；当前机制按[挂载设计](../engineering/multi-sp-mount-design.md)。继续须区分旧方案、现行ADMM/EML外观和持续效果，不将旧保开度整套叠回 |
| CFA锁定/炮口/ECM：修订已接收，未解边界保留 | [锁定残留](../../evidence/cfa44-live-seeker-residue.json)、[发射切换审阅](../../evidence/cfa44-fire-switch-2.19.1-audit.json)、[切换计量](../../evidence/cfa44-switch-cost-alpha1-r3-20261021.json) | `C:/Users/83717/.codex/worktrees/cfa44-10ms/AC7_MultiSP`、`cfa44-muzzle-notes/AC7_MultiSP`、`cfa44-startup-review/AC7_MultiSP`；各自Temp | 根因边界与正确清理见[工程经验](../engineering/engineering-lessons.md)。偶发蓝光无已证根因，ECM阶段仍有尖峰；后续按同机/组合/模型开关和计时修订采样，不能承诺固定10ms |
| 原生静态三机：已采用，来源快照保留 | [三机来源](../../evidence/static-native-three-aircraft.json) | 主目录`Temp/remaining-native-mount-research/source`、其`Temp/mount-assets-native-four/source` | F-14A、F/A-18E、TGM最初65资源阶段已被后续碰撞/布局/TGM r2修订接续；不能用原完整补丁覆盖现行77资源，继续取最新精确增量 |
| MiG-31B观察版与r1/r2/r3：已接入，几何观察未全结案 | [观察版](../../evidence/m31b-mount-observation.json)、[r1](../../evidence/m31b-saam-offset-r1.json)、[r2](../../evidence/m31b-saam-offset-r2.json)、[r3](../../evidence/m31b-saam-offset-r3.json) | 主目录`Temp/remaining-native-mount-research/observation`、来源工作树`native-sp-mount-takeover/AC7_MultiSP`及其Temp | 最初后腹SAAM真实三角面有交叠，用户要求先观察；后依次X减6/7/11、Z减2。当前r3仅mask6/7，同编号替换旧资源，支撑/净空仍需观察；从`prepare_m31b_saam_offset.py`、`m31b_offset_data.py`及[当前布局](../engineering/multi-sp-mount-design.md)继续 |
| 五机静态设计：四机采用，ASF-X/RKT未接 | [五机静态方案](../../evidence/active-five-mount-static-plan.json) | 主目录`Temp/remaining-native-mount-research/animated-five/source`；`src/research/review_active_five_mounts.py` | 35组合设计与1960个离线包络案例不代表游戏通过。ASF-X含Body swc3/挂架spwc双域及76个RKT口，专项后置；Block III上层朝向、LACM真实展开也须保留边界 |
| 四机局部机构：已接入，后续资格修订已采用 | [局部接线](../../evidence/local-four-mount-runtime.json)、[碰撞保真](../../evidence/native-seven-collision-preservation.json)、[批次反馈](../../evidence/native-batch-user-feedback-20261020.json) | `src/research/prepare_local_four_mounts.py`、`local_four_runtime_data.py`；来源工作树`native-sp-mount-takeover/AC7_MultiSP` | F-14D/F/A-18F/Su-33/Block III不再是“只有静态方案”。已有用户正常反馈不扩大为全部组合/动态净空/性能；完整动画与其他机体不自动放行 |
| F/A-18E/TGM布局及Socket r2：已采用，旧r1保留 | [布局反馈](../../evidence/f18e-layout-feedback-r1.json)、[机库冻结](../../evidence/f18etg-hangar-freeze-20261021.json)、[r2](../../evidence/f18etg-whole-rack-socket-r2.json) | `native-sp-mount-takeover/AC7_MultiSP`及其`Temp/f18e-layout-feedback-r1`、`Temp/f18etg-socket-r2`；完整输入见[开发说明](../engineering/development.md) | r1序列化Socket引用错，r2修复引用，其他152成员保持。旧资源保研究但不在运行白名单；继续需核对实际cooked索引，不仅看JSON位置 |
| Su-30SM双挂/固定挂架原型：旧原型保留，现行方案已接续 | `c44e3b9`、`d2f2221`；[早期归档](../archive/README.md) | 主目录`Temp/su30sm-dual-mount-research`、`Temp/su30sm-fixed-mount-prototype`及各自ignored输入 | 旧QAAM悬空外移已回滚，不因现行共架获认可而恢复。原型现场有未提交变更快照，继续时先核对源差异和真实支撑 |

本表来源工作树位于`C:/Users/83717/.codex/worktrees/`时，省略的目录前缀均为该路径。旧完整资源、未用副本和动画不能只因当前运行白名单不用就删除；清理必须先证明恢复范围与本机原料齐全。

上表的`cfa44-muzzle-notes/AC7_MultiSP`是历史来源位置：本轮确认其干净且没有ignored原料后已送回收站，源码改从`archive/20261023/cfa44-muzzle-notes`恢复。其他已封存路径的状态见[工作区索引](../project/workspaces.md)，不要按旧分支名继续写入。

## 更新器与其他独立成果

| 主题/状态 | 成果锚 | 来源位置 | 已知问题与继续研究入口 |
| --- | --- | --- | --- |
| 快速任务多SP/皮肤源码、PART待续 | `codex/feat-quick-mission-launch`；main基线 `85ab3a0` | `C:/Users/83717/.codex/worktrees/quick-mission-launch/AC7_MultiSP`；[唯一设计正文](../engineering/quick-mission-launch.md)、[观察摘要](../../evidence/quick-mission-observation-20261024.json) | 已核对23关任务表及原生自由任务出击链，F6、独立预设、主菜单自动出击和Boot片头跳过源码2bef53b已进入2.25.0完整构建；用户确认2.25.1可快速进入但三SP初始化中止；辅助错误隔离和卡片位置修复已进入2.25.2完整构建，bbf0984/4fb9fd0皮肤成果已由集成线程接收，保留alpha2多SP并兼容两种旧schema2，已从2acf8de完整构建alpha3 EXE，尚未安装或实机验证；PART缺完整装备/容量/冲突契约，继续条件见唯一设计正文 |
| 游戏内“版本和更新”：正式采用 | 来源`d9d3dab`、后续功能`2346cdb`；[alpha2接收](../../evidence/2.24.0-alpha2-source-intake.json) | `version-updates/AC7_MultiSP`及main当前`src/version_updates*.hpp` | 原发布当时未安装，后续已知安装记录统一见[项目状态](../../README.md)。本轮未核查线上完整流程；旧[说明](../archive/delivery/Pages与R2发布更新说明.md)为历史参考。功能为检查并由用户打开浏览器 |
| 独立安装器：待接收的新交互成果 | `codex/delivery-installation` / `a99a404` | `C:/Users/83717/.codex/worktrees/delivery-installation/AC7_MultiSP`；现场材料见[工作区索引](../project/workspaces.md) | 0.1.0有用户确认EXE安装2.24.1的记录，0.2.0仅生成新安装器，新界面未运行；当前已知状态统一见[项目状态](../../README.md)。源码未合main，载荷版本不能代替安装器自身验收 |
| 旧update-feature：未提交成果保留、未整套接收 | HEAD `42b87c0`；20261023盘点与`update-feature.zip` | `C:/Users/83717/.codex/worktrees/update-feature/AC7_MultiSP`；本地`src/version_updates.hpp`及Lua/UI/构建/发布工具改动 | HEAD已是main祖先不代表现场改动已接收；与已采用更新页有重叠，继续先比较功能/文案/工具差异，不把整个工作树覆盖到main |
| 独立updater预览：未提交半成品，未进入main | HEAD `391f46d`；20261023盘点与`update-preview.zip` | `C:/Users/83717/.codex/worktrees/update-preview/AC7_MultiSP/src/updater/`、该树`docs/update-preview.md`、`evidence/update-preview-*`及Temp缓存 | 已有下载/大小/摘要/ZIP检查、只读计划和Ed25519原型；签名入口/正式公钥、完整GUI交互、安装事务、回滚、退出等待、F6接入和重启未完成。研究用未签名模式不等于正式方案；从该树说明和现场回执继续 |
| 本机机体/兼容资料：未接收现场保留 | 原分支`codex/aircraft-mod-static-plan`，HEAD `0b75b21`；20261023`root.zip`快照 | 原`E:/AC7_MultiSP`未提交文件；整理后实际保留位置以本轮治理清单为准 | 包含本机研究/兼容资料，不代表正式产品功能；不能因为整理回main而随提交混入。第三方资源、个人设置和原始取证仍不入Git |
| FOV Editor兼容尝试：已卸载，归档保留 | 本机未提交`evidence/compatibility-fov-editor-archived.json`，随根目录快照保留，非main内回执 | `E:/AC7_MultiSP/backup/fov-editor-archive-20261019-081507.zip`及原`Temp/fov-editor-*`；卸载件见[归档入口](../archive/README.md) | 方向键/雷达均无响应，原因未定，不记录功能通过。恢复前另备份后续个人设置，不能拿旧多SP核心覆盖当前版 |
| PW出击退出/本机取证：未解现场保留 | [PW退出](../../evidence/pwmki-sortie-crash-2.20.1.json)、[取证修订2](../../evidence/crash-capture-hardening-2.20.2.json) | 证据所列主目录`Temp/pw01-crash-*`、`Temp/crash-capture-*`与脚本 | PW再次出击成功不算找到首轮根因；修订2只提高取证完整性，不保证每次捕获原因。旧F12系统断点问题已修，不混作Mod死锁 |

## 界面设计原件

- [管理界面原件](management-design/index.html)：历史交互草图，包含用户回传文件名、SHA256、来源提交和具体批注；保留完整可交互原文，不以当前界面截图替代。
- [版本和更新页原件](management-design/version-updates.html)：已采用功能的设计证据，含作者信息位置、授权全文展开、更新内容展示等用户意见；不代表独立安装器或全部设计项已完成。

设计稿只作为来源证据，当前机制以工程正文为准。后续设计按新任务明确范围，不能在旧HTML里偷偷改写用户回传记录。

## 本机恢复材料与缺目录记录

2026-10-23整理前盘点位于`E:/AC7_MultiSP/backup/project-governance-20261023/inventory-before.json`，包括Git锚、现存脏树快照与ignored路径。快照ZIP保存HEAD、二进制差异和未跟踪文件；ignored只列路径，**没有因此被打包或复制**。本轮归档后的分支/工作树实际状态以治理回执为准，不能依本页的原分支名推定它仍活动。

除上表两个更新工作树及根目录外，盘点还保存了`release-1-0-2-f6.zip`、`adf11f-uav-research.zip`、`ecm-tls-persistence.zip`、`persistent-sp-research.zip`、`su30sm-dual-mount-research.zip`和`su30sm-fixed-mount-prototype.zip`。这些旧树存在现场差异，不因已有提交合入就抹去；继续时先读快照说明并核对原料，不直接覆盖main。

20261017的`backup/project-archive-20261017/`另有all-refs.bundle、git-archive-manifest.json、工作树ZIP、historical-builds.zip及materials-archive-manifest.json。其覆盖范围和旧13分支标签见[历史归档](../archive/README.md)。普通Git快照不含ignored资源，工作树锁定只防误清理，不是文件权限只读。

整理前盘点发现以下四个工作树**有Git登记，但对应目录缺失**，不把旧“原位保留”叙述当作今天已验证的事实。恢复锚与登记元数据保留；整理后的实际登记状态见[工作区索引](../project/workspaces.md)。续研时查旧ZIP、清单与其他磁盘副本，核实未提交/ignored原料缺项。

| 登记目录末级名 | Git恢复锚 | 继续入口 |
| --- | --- | --- |
| `mount-expansion/AC7_MultiSP` | `archive/20261017/mount-expansion` / `9cefa43` | 查旧归档清单与资源扩展证据，核实本机ignored模型和构建是否另存 |
| `mount-layout-research/AC7_MultiSP` | `archive/20261017/mount-layout-research` / `10d6301` | 查旧布局脚本、几何输入与ZIP覆盖范围，不能只恢复Git文本就称完整 |
| `sp-combination-technology/AC7_MultiSP` | `archive/20261017/sp-combination-technology` / `e35ff09` | 查静态分区/配方、资源输入和旧未提交快照 |
| `three-sp-ui-polish/AC7_MultiSP` | `archive/20261017/pending-ui-polish` / `7e2fe4a` | 登记名与旧分支名不同；核对历史UI资料及本机快照，不猜造目录 |

恢复时先定位Git提交，再核对未提交快照，最后确认ignored原料和工具。缺少任一层就明确缺项，不反复试错，不删除旧资源、构建、转储或个人备份。
