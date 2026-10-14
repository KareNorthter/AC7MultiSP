# 精选证据索引

> 最新版本为 0.1.8：[用户初步验证](0.1.8-user-validation.json)、[运行文件清单](0.1.8-runtime-manifest.json)、[安装回执](0.1.8-installed.json)。下文候选“当前/尚未实玩”为当时记录；用户现已确认补给及所有功能初步可用，大范围和持久测试未完成。

返回[项目入口](../AGENTS.md)，相关结论见[架构](../docs/architecture.md)、[核心研究](../docs/research/core-findings.md)、[模型研究](../docs/research/weapon-models.md)。

| 保留内容 | 说明与边界 |
|---|---|
| `0.1.7-*.json` | 迁移前的最近离线检查及安装核验；本轮没有重新执行这些检查 |
| `native-hud-layout.json` | 自有原生 HUD 资源的结构摘要 |
| `native-weapon-message-structure.json` | 2026-10-13 离线解析的中央武器提示控件、布局与页面动画/调用摘要；尚未确认运行时隐藏机制 |
| `native-loadout-slots.json` | 候选 2 加载失败后，只读核对标题、列表、名称及图标的槽/父容器；不证明活跃运行时类型 |
| `native-ui-slot-switch-findings.json` | 候选 2 的用户反馈、三次出击槽判断错误及候选 3 呈现事务修复方向；候选 3 未实玩 |
| `ac8-resupply-comparison.json` | 用户三张补给截图的观察、实现取舍、原生结束函数签名与未验证项；不证明真实跳转和时序 |
| `preparation-review-findings.json` | 准备页重选/退出/重进的源码缺陷、双栏资源坐标、交接取舍与编译安装摘要；未实玩 |
| `preparation-live-validation.json` | 授权键盘操作的准备、取消重进、修改、保存与出击结果，以及未测边界和显示遗留；不代表飞行或补给通过 |
| `resupply-live-findings.json` | 用户实玩补给双选及起飞卡住的日志证据、离线修复记录及候选 6 安装信息；最新实玩失败与修订另见 input-audio-revision.json |
| `management-candidate.7-installed.json` | 当前全套安装；Lua 分别校验补给阶段关卡与运行任务，DLL 沿用候选 6；安装核验通过，效果待实玩 |
| `resupply-completion-revision.json` | 候选 6 用户复测及暂停现场只读证据；阶段 World 误判原因与候选 7 修订边界 |
| `management-candidate.6-installed.json` | 历史安装；用户随后确认本次音效未出现、双选及起飞正常，起飞后停用另见候选 7 |
| `management-candidate.5-installed.json` | 历史候选 5；真实补给 YES 桥接、飞机切换锁定组件同步；独立核心构建，配置/存档/正式核心保护核验，尚未实玩 |
| `management-candidate.4-installed.json` | 历史全套安装；准备布局/字体/导航/保存、无限弹药 HUD 修复及文件保护核验；实机另记 |
| `management-candidate.3-installed.json` | 准备双栏、确认编辑、取消交接及重进处理的成套安装；界面重新编译、核心沿用，配置/存档保留；未实玩 |
| `management-candidate.2-installed.json` | Lua 补给双栏/YES-NO 更新的整套安装核验，DLL 均沿用、配置及存档保留；未实玩 |
| `management-candidate-installed.json` | 管理候选 1 的 UI 构建、配套导出、八 Lua 语法、十个运行文件、完整旧模组备份和保护核验；不代表实玩通过 |
| `merged-candidate.3-installed.json` | 候选 3 的成套安装核验、旧模组完整备份与保护文件；不代表加载或切换已实玩通过 |
| `native-loadout-structure.json` | 准备页类、控件与布局摘要 |
| `native-resupply-structure.json` | 2026-10-13 只读解析的补给页父类、控件、布局及蓝图调用摘要；不证明运行时事件顺序 |
| `resupply-candidate-installed.json` | 2026-10-13 候选核心接口、Lua 语法与安装文件核验；保留配置、存档、原模组备份及正式核心，不代表实玩通过 |
| `merged-candidate-installed.json` | 补给与 HUD 合并候选的十个运行文件、Lua 转发接口与安装保护核验；不代表实玩通过 |
| `native-ui-api-failure.json` | 用户实玩出击选择失败的日志摘要，以及共用补给/HUD 接口误用的离线诊断 |
| `merged-candidate.2-installed.json` | 移除旧引擎缺失接口后的合并候选 2 安装核验；运行时菜单和补给仍待复测 |
| `weapon-model-layout.json` | Su-35S 挂架、Socket 与武器装填数量摘要 |
| `dual-mount-prototype.json` | 仅几何分配原型，未安装游戏功能；内部旧版本字样属于当时结论 |
| `unusable-packed-disk-code.json` | 加壳磁盘代码不适合作为写入接口的依据 |
| `native/` | 精选历史函数体与反射接口，不能重用旧进程绝对地址 |
| `integration/` | 早期实机原型通过记录，保留为研究依据，不等同于最新版本通过 |

完整 PAK 资源副本、资源解析中间 JSON、早期日志、失败尝试和重复版本检查已回收；重要失败机制与未知项保留在[关键决策](../docs/decisions.md)。当前运行文件与原始发行包 hash 另见[基线记录](../Release/baseline-0.1.7.json)。

本次整理的范围与结果见[迁移概要](migration-summary.json)，逐文件复制审计留在被忽略的 `Temp/`，不作为游戏功能验证记录。
## 本轮原生流程复核

[native-flow-revision.json](native-flow-revision.json) 记录 Su-30SM QAAM＋LAGM 的候选 4 失败复测，以及用户停留主菜单时取得的原生 YES/武器切换调用链。候选 5 已构建并成套安装，尚未实玩验证。此前仅根据函数名称判断结束入口、根据日志判断控件树故障的结论已撤回，详见[修订说明](../docs/native-flow-revision.md)。

## 候选 5 复测与候选 6 修订

[input-audio-revision.json](input-audio-revision.json)记录本次用户复测、暂停现场的音效引用失配，以及两个原生补给输入入口。候选 6 已编译安装，尚未实玩，不能把只读函数分析当成实玩通过。说明见[输入与音效修订](../docs/input-audio-revision.md)。

## 候选 6 复测与候选 7 修订

[补给结束记录](resupply-completion-revision.json)记录正常 YES 后阶段世界身份误判。当前候选 7 只更新 Lua，两枚 DLL 沿用候选 6；配置、存档与正式载荷保护核验通过，未由代理实玩。上文“尚未实玩”属于对应候选安装当时状态。
