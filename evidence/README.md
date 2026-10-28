# 证据索引

这里保存各次研究、构建、安装、用户反馈和发布的原始记录。文件名/日期说明记录阶段，不代表今天的状态；当前状态见[项目入口](../README.md)。可复用结论见[工程经验](../docs/engineering/engineering-lessons.md)，待续方案见[研究索引](../docs/research/README.md)。

## 关键入口

主页源码、原上传ZIP及文档快照差异见[20261024主页接收回执](pages-workflow-intake-20261024.json)；作者的2.25.2正式发布确认、本次生成材料与下载核对见[发布准备记录](2.25.2-publication-preparation.json)。发布顺序只维护在[新版发布流程](../docs/project/release-workflow.md)；本地准备不证明Cloudflare、蓝奏云或Nexus已由本工具上传。

| 主题 | 入口 | 回答的问题 |
|---|---|---|
| 六个Mod机体入口用户验收 | [20261028作者功能反馈](mod-aircraft-user-acceptance-20261028.json) | X-02R A/B、J-20A/B/S和RH-172功能正常；不补造逐项测试或追认ZSD组合冲突已修复 |
| X-02R与三个J-20启用 | [20261028切换B组及摘要核对](x02r-j20-enabled-20261028.json) | 作者确认由A切B；四个Mod六份PAK及目录一致，作者页面与未知项登记，未实机验证 |
| 2.26.1作者发布确认 | [20261025发布与main收尾](2.26.1-publication-confirmation-20261025.json) | 作者报告已发布；同步2.26.1清单及当前规则，不提交发布包，不追认本轮独立远端核验 |
| 2.26.1正式EXE与ZIP | [完整构建与交付](2.26.1-exe-delivery.json) | 作者最新中文原字节、四份英文全文重译、同源资源与签名；正式EXE/50成员ZIP，未安装/上传 |
| 2.26.1正文同步核对 | [实际EXE、ZIP及UI DLL回读](2.26.1-document-sync.json) | EXE安装器/部署正文16份、ZIP根正文8份、实际F6界面DLL中日志/授权4份均来自最新user，未执行入口 |
| 2.26.1网页上传包 | [完整Pages包与历史列表](2.26.1-pages-delivery.json) | 顶部2.26.1两条更新，历史最早至2.26.0含2.25.X；R2单入口、旧签名与28成员回读，远端/部署未核 |
| 2.26.1网页历史与R2词条 | [旧包核查及中文逐条批复](2.26.1-pages-history-source.json) | 旧包含2.25.X，线上读取受阻；历史升序至2.26.0、最新2.26.1及正文刷新，R2中文词条逐条获准 |
| 2.26.1四份英文全文重译 | [重新翻译与逐行对照](2.26.1-full-retranslation.json) | 作者要求送回收站后重译；当前中文原稿摘要不变，新英文逐行对应；此前翻译包不交付 |
| 2.26.1作者中文原稿与翻译 | [原字节接收与全局审核规则](2.26.1-author-text-intake.json) | 四份中文摘要、英文同步及中文逐条审核约束；重新制作正式EXE/ZIP/网页，暂停构建原现场保留 |
| 2.26.1正式验收与授权 | [作者反馈及正式版本确认](2.26.1-release-authorization.json) | CFA-44模型开关、EML与舱门、F-2A普通飞行及其他功能反馈；授权合main和重建正式EXE，不扩大为故障注入等专项通过 |
| 2.26.1 alpha3三线程集成EXE | [来源、构建与交付](2.26.1-alpha3-integration-delivery.json) | 原生英文标签、性能及兼容/CFA机腹舱三来源合并；核心/UI/EXE完整编译和签名核对，77资源154成员；未安装/实机验收，不替代正式2.26.0 |
| CFA EML持续掉帧研究 | [现场与动态对照](cfa44-eml-steady-performance-20261025.json) | alpha2同进程EML/ADMM只读采样、内存查询热点、取位连带炮口巡检的证据和限制；未实施修复，不把样本比例当FPS收益 |
| alpha2首轮性能反馈 | [用户观察与日志](2.26.1-alpha2-performance-feedback-20261025.json) | F-2A普通飞行和选装改善；CFA EML模型开启时持续掉帧待定位；不将FPS观察当完整帧剖析 |
| 2.26.1 alpha2性能自测EXE | [构建与交付核对](2.26.1-alpha2-runtime-performance-delivery.json) | main与F6精确来源、核心/UI完整编译、Lua语法/接口/保护摘要及作者签名；只交付EXE，未安装或实机验证 |
| 任务内与原生选装性能基线 | [20261025日志摘要](runtime-performance-baseline-20261025.json) | main与F6来源锚、alpha1进程样本及本机日志快照；不代表alpha2收益或受控A/B |
| 2.26.0验收与main收尾 | [20261025验收与文档接收](2.26.0-main-acceptance-20261025.json) | 作者确认安装包/Mod正常；三份手改TXT原字节接收，文字生成同步；仅授权推送main，不重编或重包，不改原产物/回执 |
| 2.26.0仅R2网页 | [交付与R2核对](2.26.0-pages-r2-only.json) | 停用蓝奏云、隐藏备用区；R2完整SHA256匹配修订2，完整中英文章节与历史签名保留，未部署或重编EXE |
| 2.26.0安装器主页修订2 | [来源与交付](2.26.0-installer-homepage-revision2.json) | 新版提示固定进主页，版本不变；冻结一致的游戏构建复用，C#重编并重签，独立修订目录交付与配套网页签名同步，未运行或上传 |
| 2.26.0网页交付 | [交付与核验边界](2.26.0-pages-delivery.json) | 指定网页线程成果、新蓝奏云链接、完整正式日志和22个成员；作者要求先制作，R2文件不一致尚未通过，未部署 |
| 2.26.0正式EXE与ZIP | [交付与Vortex静态核对](2.26.0-exe-delivery.json) | 作者最新中文原字节、对应英文、来源38f1255、完整构建与三用途签名；50成员ZIP、40项Vortex映射及四尺寸游戏图标核对。待作者上传与最终下载链接，不提前修改stable，release_notes完整使用2.26.0章节 |
| 2.26.0正式版本确认与日志 | [验收与版本确认](2.26.0-release-authorization.json) | 作者确认alpha5正常并指定2.26.0；汇总2.25系列主要变化、移除玩家alpha明细，保留历史记录；正式标识与双语文字已整理，尚未构建或上传 |
| 正式2.24.1 | [integration](2.24.1-integration.json)、[release](2.24.1-release.json)、[授权](2.24.1-release-authorization.json) | 当次来源、检查、发布成员与授权；不证明后续实际安装 |
| 2.24.2双语交付候选 | [交付核对](2.24.2-delivery.json) | 人工正文接收、双语词条、冻结构建、ZIP及EXE摘要和无安装预览；不代表游戏实测或已上传 |
| 2.24.3仅EXE交付候选 | [EXE交付核对](2.24.3-exe-delivery.json) | 新首页、F6及DLC许可调整的完整构建、封装来源、文件版本和EXE摘要；未运行安装器、测试或游戏，未单独交付ZIP |
| 2.25.0三线程整合候选 | [EXE交付核对](2.25.0-exe-delivery.json) | 三线程精确来源、完整核心/UI编译、18 Lua/21运行文件、内嵌ZIP原字节、DPI和EXE版本/摘要；未安装或游戏验收 |
| 2.25.1界面与候选漏机修订 | [EXE交付核对](2.25.1-exe-delivery.json) | 水印/反馈卡片、Patch分类依据及两入口共用判断、964507d接收、完整构建、内嵌ZIP和EXE版本/摘要；未安装或实机验收 |
| 2.25.2快速任务初始化修复 | [EXE交付核对](2.25.2-exe-delivery.json) | 辅助错误隔离、日志上方布局、完整构建/封装、版本和内嵌载荷摘要；未安装或游戏验收 |
| 2.25.3 alpha1自测候选 | [前阶段来源接收](2.25.3-alpha1-source-intake.json)、[最新EXE交付与测试清单](2.25.3-alpha1-exe-delivery.json) | 最新7e255e1、已包含cc924a5、完整构建与内嵌ZIP核对；人工正文、英文源更名及两种语言各四份说明。未安装或游戏验证，正式仍为2.25.2 |
| alpha1用户反馈与alpha2源码 | [反馈/修订](2.25.3-alpha1-feedback-alpha2-source.json) | 快速任务多SP、暂停时退格/空格优先修订；确认当前运行文件匹配alpha1，公共舱两机未生效且暂缓；alpha2未编译、测试或安装 |
| 2.25.3 alpha2自测EXE | [交付与待测项](2.25.3-alpha2-exe-delivery.json) | 快速任务多SP、F6键盘隔离修订的完整构建；21份运行载荷、48成员ZIP、八份说明、版本及保护摘要核对；未安装或游戏验证 |
| 2.25.3 alpha3自测EXE | [交付与待测项](2.25.3-alpha3-exe-delivery.json) | 接收皮肤功能并兼容alpha2多SP预设、修订补给说明报错；完整核心/UI构建、48成员ZIP、八份说明及EXE版本/摘要核对，未安装或游戏验证 |
| 2.24.2文字迁移复核 | [改动与静态检查](2.24.2-localization-review-20261024.json) | 原生选装/补给、通知、错误映射及封装的实际范围；既知翻译回归已修正，补齐Lua启动日志旧号残留；未编译/游戏验证 |
| 2.25.3 alpha4自测EXE | [交付与待测项](2.25.3-alpha4-exe-delivery.json) | 来源9ed9863，完整核心/UI构建，18份Lua、48成员ZIP、八份说明及EXE版本/摘要核对；核心日志版本和Lua旧号移除已静态核实，未安装或实机验证 |
| 2.25.3 alpha5保护与作者入口 | [交付与四项待测清单](2.25.3-alpha5-exe-delivery.json) | 接收6ce2881，作者确认alpha4基本实机通过；复用现有CNG密钥的公钥，三用途签名、DLL精简加载数据、50成员ZIP与双语说明核对；安装器/F6双语主页及咖啡入口同源。未运行安装器/测试或游戏，发行后运行期核验未接入 |
| alpha4补给预览/日志源码 | [定位与修订](2.25.3-alpha4-preview-log-source.json) | alpha3最新日志的同组件挂架记录误判、联合基线核对和VERSION构建注入；仅静态审阅，未编译/测试/安装，F6状态发布性能暂未处理 |
| alpha3皮肤与补给源码 | [来源/故障](2.25.3-alpha3-skin-resupply-source.json) | 皮肤功能此前未进alpha2，schema3保留两种旧多SP格式；补给排版的翻译多返回值根因和修订，构建/实机验证另记 |
| 2.24.3安装器DPI修订 | [DPI修订核对](2.24.3-dpi-exe-delivery.json) | 系统DPI清单、复用游戏载荷、EXE摘要及120 DPI双语页面离线预览；产物生成后用户要求停止后续编译，未安装或操作游戏 |
| 2.23.1验收/标识 | [用户验收](2.23.1-formal-user-validation.json)、[标识变换](2.23.1-formal-version-label.json) | 已测功能和正式标识的关系 |
| alpha1安装 | [预检](2.24.0-alpha1-install-preflight.json)、[完成](2.24.0-alpha1-install-completion.json) | 当时Vortex链接处置、个人资料与实际交换；不当作实时安装状态 |
| 更新模块 | [alpha1来源](2.24.0-alpha1-source-intake.json)、[alpha2来源](2.24.0-alpha2-source-intake.json) | UI、更新清单和通知的接收范围 |
| 快速任务入口研究 | [20261024只读观察](quick-mission-observation-20261024.json)、[原生入口依据](quick-mission-native-entry-20261024.json)、[2.25.0失败定位](quick-mission-failure-20261024.json)、[多SP/皮肤/PART静态依据](quick-loadout-options-source-20261024.json) | 23关实值与M6/DLC2手动出击观察；不证明自动启动或跳CG可用，原始日志及代码仅留本机Temp |
| 挂载完整原料 | [refresh](2.23.0-refresh-integration.json)、[Socket核对](mount-socket-reference-audit-20261021.json)、[TGM r2](f18etg-whole-rack-socket-r2.json) | 77资源阶段的来源及资格 |
| 原生/第三方边界 | [原生退让](special-aircraft-native-fallback-20261022.json)、[UAV/HPM契约](mod-special-sp-contract-20261022.json) | 哪些对象与使用路径可被放行 |
| Mod机体分组安装 | [20261023安装](aircraft-mod-profiles-installed-20261023.json) | 用户选择原作者资源分组启用、部署范围和缺项保留；不代表实机通过 |
| CFA-50与机库名称 | [20261023核对](cfa50-visibility-review-20261023.json) | 呼号包名称覆盖、用户授权禁用、CFA-50两包同字节及当前目录核对；显示仍待用户重启确认 |
| CFA-50检索与F6退格回归 | [20261024源码核对](f6-aircraft-input-source-20261024.json) | FA50/241候选仍在、正式名称解析及退格预读/原始键盘漏隔离；已安装UI与2.25.2交付摘要相符，旧修复已合入；新修订未编译或实测 |
| XFA-33独立替换组 | [20261023入库](fenrir-profile-installed-20261023.json) | 三型替换关系、与Su-35SX隔离、保持A组、CFA-50机库已找到的后续反馈；未启用或实机验证新组 |
| CFA-50的EML与十字键旧现场 | [20261023现场](cfa50-eml-runtime-review-20261023.json) | 当时的资源/姿态采样及十字键核对；后续用户确认机腹炮体存在，旧“炮体未展开”解释已在下条纠正，原回执保留 |
| CFA-50顶部舱门源码候选 | [20261024舱门](cfa50-bay-animation-source-20261024.json) | 用户更正机腹EML模型存在；2.25.2现场/作者分域/原生分派确认旧顶部域残留，新增限定源码收尾；未编译/测试/安装。记录当时其他Mod长按正常、CFA-50 ESM暂缓，后续恢复机制研究见下条；旧回执解释以本轮纠正为准 |
| CFA-50机体ECM与僚机ESM机制 | [20261024机制](cfa50-ecm-esm-mechanism-20261024.json) | 后续恢复研究：机体30秒干扰/3秒后处理、僚机20秒支持、原生长按姿态门禁；用户确认召唤后蓝区，未新增产品修复、编译或安装，未逐次排除历史异常 |
| Mod机体名称与IRCM同类核对 | [20261024共用修复](mod-aircraft-reference-and-ircm-audit-20261024.json) | 176条目录/126条Mod基础类静态核对，软类引用名称修复；S32原生激活/自然结束现场及限定IRCM混装源码候选，核心/Lua须成套；F-14IA继续隔离，未编译测试安装 |
| SU-57/DarkStar/XFA-33舱门分域 | [20261024静态比较](bay-domain-multi-aircraft-static-20261024.json) | CFA-50方法解释、84份资源消费者/轨道对照，区分共用0/1域与公共域；建议共用内核＋精确资源profile，未扩展实现或操作游戏 |
| CFA-44机腹独立舱源码修订 | [20261025来源](cfa44-belly-bay-source-20261025.json) | 用户最终规则、原版swc1机腹双骨/兵装引用与共用单路策略，撤出Light旧复制写入者；未编译、测试、安装或实机验证 |
| 多机舱门限定源码候选 | [20261024源码范围](bay-domain-multi-aircraft-source-20261024.json) | 六目标分为共用0/1域收尾和公共舱重建连续性，CFA-50共用内核、Dark唯一写入责任、固定热路径开销与失败清理；仅静态核对，未编译/测试/安装或性能实测 |
| Mod机体长按切换键 | [20261024静态研究](mod-aircraft-hold-switch-static-20261024.json) | CFA-50复用原生副油箱输入；两处模拟按下/松开会截断真实长按，通用保护方案与未核计时/效果边界；未改代码或游戏 |
| 长按切换键源码修复 | [20261024源码](mod-aircraft-hold-switch-fix-source-20261024.json) | 两处内部切换改走原生跳过长按初始化参数，保留真实输入；未编译/安装/实测。另记录用户关闭总开关后EML仍无动画/炮体，按指令暂缓 |
| 动画半成品 | [产品访问](sp-animation-product-access.json)、[早期动画](mount-expansion-animated.json) | 研究依据与尚未成立的运行条件 |
| 仓库整理 | [20261023治理回执](project-governance-20261023.json) | 删除的分支tip、封存工作树、快照及本轮核对范围 |
| 文档与旧产物归档 | [20261023归档回执](artifact-archive-20261023.json) | 文档路径映射、旧包和DLL归档位置、仍保留依赖及核对范围 |
| 本机资产整理 | [20261023资产清单](local-assets-20261023.json) | 原路径到实际路径、归类、逐项处置、核对范围和GitHub研究恢复锚 |

快速任务2.25.1进入任务后三SP失效的[日志依据与源码修复范围](quick-mission-trisp-failure-20261024.json)记录辅助异常、机体隔离和未编译边界。

## 按主题定位其余记录

| 文件名特征 | 大致内容 |
|---|---|
| 版本号＋integration/installed/release/user-validation | 当次整合、安装、发布、用户反馈。研究号可能重复或高于后来正式号，须结合Git锚 |
| cfa44-*、switch-* | CFA兵装残留、切换成本、队列、炮口、异常与观测边界 |
| f22-*、pwmki-*、crash-capture-*、aircraft-*-pause-* | 准备页生命周期、出击退出、取证与暂停换机 |
| static-*、native-*、mount-*、local-*、m31b-*、f18e*、f18etg-* | 挂载几何、资源配方、碰撞、Socket与用户布局反馈 |
| gun-salvo-*、mod-*、special-aircraft-* | 齐射、第三方机体/兵装与安全退让 |
| camera-*、tracking-*、target-*、ui-*、management-* | 输入、镜头、界面与配置变更依据 |
| compatibility-* | 某次本机兼容尝试，不能推广为产品通用兼容保证 |

在本目录按文件名或字段定向检索即可；不要求日常通读全部JSON。需要全量文件名时用 `rg --files --no-ignore evidence`。关键文档直接链接其依据，避免本索引再复制每个回执的长篇摘要。

## 保存规则

- 原有JSON和被引用的摘要不改写，不为目录整齐随意移动。新事实写对应新记录，纠正当前正文中的错误解释。
- 实验、离线检查、实际安装、用户验收是不同证据；失败/未覆盖字段不能省略成“全部通过”。
- 安装器0.1.0/0.2.0回执目前在codex/delivery-installation分支；原根目录本机兼容回执在Temp/preserved-local-20261023。它们尚未进入此main目录，不能伪造相对链接。
- 原始转储、截图素材、游戏资源、个人文件留本机受保护目录，不为补齐索引而提交或公开。
