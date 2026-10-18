# 证据分类索引

当前已验收主线 **2.19.0 / main / v2.19.0**，用户确认CFA修订有效、卡顿明显减少并授权合入。用户随后授权制作2.19.0本地发布包；2.16旧包保留。旧报告保留当时状态，不覆盖最新反馈。历史与恢复见[归档索引](../docs/archive/README.md)。

## 当前测试候选

[2.19.2双发现场与计时](cfa44-live-seeker-residue.json)：用户授权只读内存并手动复现，确认锁定槽重建遗留目标登记，MSL/EML均扣2；共用入口源码修订覆盖所有机体，尚未构建安装。已安装版本三SP开启模型103次中位数41.18ms，关闭模型10次6.38ms，不能当作本次新源码的成绩。

[CFA短只读批次优化](cfa44-read-batch-optimization.json)：以2.19.2现场分项为依据进一步争取20ms；合并相同内存区间的重复查页，保留动态身份核对和引擎调用边界。计时修订3新增复用量，仅源码，未编译实测。

2.19.2接收CFA/ADF的`1678c74`、`0c1d35a`与R3的`4ba961a`，来源及安装见`2.19.2-integration.json`及`2.19.2-installed.json`。CFA双发路径有行为修订、R3短按有整次回调隔离，FALKEN发射期间丢弃切换请求；本轮已成套构建、通过11项现有检查并安装，17运行文件、50资源/100成员PAK及192项保护文件核验通过，效果待用户实测。2.19.1及更早回执保留，原研究证据不改写为新版本成绩。

[镜头控制扩展](camera-controls-options.json)：用户确认此前R3修订正常后，按新要求实现满0.5秒立即换视角、独立回正键及右摇杆灵敏度。源码协议10/配置9，尚未构建、测试或安装；本次扩展效果待实测，旧回执不改。

## 当前发布

[2.19发布回执](2.19.0-release.json)：已测字节、包内白名单、运行库签名和全部成员回读。
[补充反馈及发布授权](2.19.0-user-feedback-followup.json)：任务结算界面箭头卡顿明显缓解、仍有轻微卡顿；用户授权制作最新版本发布包，不包含上传。

## 当前主线验收

[CFA双/三SP开合源码评估](cfa44-sp-animation-2.19.1-design.json)：后续源码的原生依据、事务时序、盖板同步及性能边界；未编译、测试或实测，不属于2.19已验收范围。

[2.19用户验收](2.19.0-user-validation.json)：确认卡顿明显减少；不扩大为完全无卡或全机全组合验收。已关闭游戏，仅核对已测来源、合入main及同步正文，没有重新构建、测试或发布。

[2.19安装](2.19.0-installed.json)及[整合回执](2.19.0-integration.json)是原交付记录，保留候选时历史字段；后续确认不改写它们。冻结构建原路径`Temp/build-2.19.0-candidate`保持，17运行文件、50资源/100成员PAK一致。

[CFA 2.18热路径调查](cfa44-switch-2.18-hotpath.json)保留原假设与计时，2.19实际改善以新验收回执为准，不改写研究证据。

[CFA后续调查](cfa44-switch-2.17-followup.json)：原研究阶段的用户反馈、日志快照、切换耗时和源码方案；本方案已接收至2.18候选，原报告不改写为新版本的实测结果。

## 独立研究

[右摇杆镜头保持研究](camera-hold-research.json)保留原只读阶段；[实现摘要](camera-hold-implementation.json)记录后续默认关闭、第二个按键页、0.5秒长按及双摇杆干扰弹同时回正的候选源码。原记录保留源码阶段状态；后续2.19.1已构建安装且获用户基本正常反馈，R3短按修订现已接收至2.19.2，见[R3修订来源](camera-hold-r3-short-press-fix.json)，效果仍待用户实测。

## 上一发布包2.16

- [用户实测](2.16.0-user-validation.json)：CFA切换短卡/ECM模型露出、ADF停光卡顿，其他基本正常；为2.16当时状态，CFA以最新2.19反馈为准。
- [安装回执](2.16.0-installed.json) · [整合与原检查](2.16.0-integration.json) · [发布回执](2.16.0-release.json)。本次发布沿用已测字节，无重新编译或运行测试。
- [源码阶段调查](2.16.0-performance-investigation.json)及[上轮反馈](2.15.0-user-validation.json)保留原状态，不能把旧4fps报告当成当前持续复现。
- `Temp/build-2.16.0-final`：实际build-receipt.json、test-receipt.json和完整产物；当前50资源/100成员PAK，17项运行文件。

## 构建输入：原路径、原字节保留

以下11份被source_snapshot或生成器直接引用，不归入压缩清理：

- `mount-expansion-batch1.json`、`mount-expansion-batch2.json`、`mount-expansion-batch3.json`
- `static-expansion-2.11.0.json`、`static-expansion-2.12.0.json`、`static-expansion-2.13.0.json`、`static-expansion-2.14.0.json`
- `sp-combination-static-split-experiment.json`、`mount-expansion-animated.json`
- `su47-native-endpoint-geometry.json`、`sp-animation-product-access.json`

`1.1.0-release.json`仍用于核旧包及微软运行库；`2.15.0-installed.json`是当前构建的升级来源，不能重写历史回执。

## 分类查找

| 名称/前缀 | 内容 |
|---|---|
| `2.*-build/tests/installed/integration/release` | 按版本的构建、核验与交付快照；2.19为已验收主线，最新发布包仍为2.16 |
| `*-user-validation.json` | 用户反馈及明确边界，不推定未点名组合验收 |
| `mount-*`、`static-*`、`su30sm-*` | 几何、Socket、资源往返、数量与布局研究 |
| `sp-*`、`animated-*`、`x02s-*` | 动画、完整弹舱、发布取位及回退方案 |
| `*uav*`、`*continuous*`、`*ecm*`、`*tls*` | 已登记持续兵装和生命周期研究 |
| `compatibility-*` | 历史兼容实测；第三方版本和场景边界保留，不等于2.16全套复测 |
| `2.4.0-*-investigation*` | 机库退出/菜单性能旧调查，未确认根因继续保留 |
| 本机`backup/project-archive-20261017/` | Git bundle、研究工作树快照、旧证据/截图和旧构建压缩档及校验清单 |

早期0.x/1.x、旧native/integration和截图从当前列表移出；可从归档标签或本机压缩档按原路径恢复。新增反馈写到对应版本，设计变化更新正文；不再把每次流水追加到主管入口。

[R3短按修订](camera-hold-r3-short-press-fix.json)：原生回调漏放路径、源码修订和未实测边界。
