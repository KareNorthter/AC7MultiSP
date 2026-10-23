# 证据索引

这里保存各次研究、构建、安装、用户反馈和发布的原始记录。文件名/日期说明记录阶段，不代表今天的状态；当前状态见[项目入口](../README.md)。可复用结论见[工程经验](../docs/engineering/engineering-lessons.md)，待续方案见[研究索引](../docs/research/README.md)。

## 关键入口

| 主题 | 入口 | 回答的问题 |
|---|---|---|
| 正式2.24.1 | [integration](2.24.1-integration.json)、[release](2.24.1-release.json)、[授权](2.24.1-release-authorization.json) | 当次来源、检查、发布成员与授权；不证明后续实际安装 |
| 2.24.2双语交付候选 | [交付核对](2.24.2-delivery.json) | 人工正文接收、双语词条、冻结构建、ZIP及EXE摘要和无安装预览；不代表游戏实测或已上传 |
| 2.24.3仅EXE交付候选 | [EXE交付核对](2.24.3-exe-delivery.json) | 新首页、F6及DLC许可调整的完整构建、封装来源、文件版本和EXE摘要；未运行安装器、测试或游戏，未单独交付ZIP |
| 2.23.1验收/标识 | [用户验收](2.23.1-formal-user-validation.json)、[标识变换](2.23.1-formal-version-label.json) | 已测功能和正式标识的关系 |
| alpha1安装 | [预检](2.24.0-alpha1-install-preflight.json)、[完成](2.24.0-alpha1-install-completion.json) | 当时Vortex链接处置、个人资料与实际交换；不当作实时安装状态 |
| 更新模块 | [alpha1来源](2.24.0-alpha1-source-intake.json)、[alpha2来源](2.24.0-alpha2-source-intake.json) | UI、更新清单和通知的接收范围 |
| 挂载完整原料 | [refresh](2.23.0-refresh-integration.json)、[Socket核对](mount-socket-reference-audit-20261021.json)、[TGM r2](f18etg-whole-rack-socket-r2.json) | 77资源阶段的来源及资格 |
| 原生/第三方边界 | [原生退让](special-aircraft-native-fallback-20261022.json)、[UAV/HPM契约](mod-special-sp-contract-20261022.json) | 哪些对象与使用路径可被放行 |
| Mod机体分组安装 | [20261023安装](aircraft-mod-profiles-installed-20261023.json) | 用户选择原作者资源分组启用、部署范围和缺项保留；不代表实机通过 |
| CFA-50与机库名称 | [20261023核对](cfa50-visibility-review-20261023.json) | 呼号包名称覆盖、用户授权禁用、CFA-50两包同字节及当前目录核对；显示仍待用户重启确认 |
| XFA-33独立替换组 | [20261023入库](fenrir-profile-installed-20261023.json) | 三型替换关系、与Su-35SX隔离、保持A组、CFA-50机库已找到的后续反馈；未启用或实机验证新组 |
| CFA-50的EML与十字键 | [20261023现场](cfa50-eml-runtime-review-20261023.json) | 炮体资源完整而已发布姿态仍收纳、舱门域残留；关闭多SP十字键后未发现输入拦截，僚机响应待用户稍后测试；EML根因待原生出击对照 |
| Mod机体长按切换键 | [20261024静态研究](mod-aircraft-hold-switch-static-20261024.json) | CFA-50复用原生副油箱输入；两处模拟按下/松开会截断真实长按，通用保护方案与未核计时/效果边界；未改代码或游戏 |
| 长按切换键源码修复 | [20261024源码](mod-aircraft-hold-switch-fix-source-20261024.json) | 两处内部切换改走原生跳过长按初始化参数，保留真实输入；未编译/安装/实测。另记录用户关闭总开关后EML仍无动画/炮体，按指令暂缓 |
| 动画半成品 | [产品访问](sp-animation-product-access.json)、[早期动画](mount-expansion-animated.json) | 研究依据与尚未成立的运行条件 |
| 仓库整理 | [20261023治理回执](project-governance-20261023.json) | 删除的分支tip、封存工作树、快照及本轮核对范围 |
| 文档与旧产物归档 | [20261023归档回执](artifact-archive-20261023.json) | 文档路径映射、旧包和DLL归档位置、仍保留依赖及核对范围 |
| 本机资产整理 | [20261023资产清单](local-assets-20261023.json) | 原路径到实际路径、归类、逐项处置、核对范围和GitHub研究恢复锚 |

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
