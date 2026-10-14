# 历史与踩坑索引

当前使用方式以 [1.0.0](version-1.0.0.md) 和 [TXT](AC7DualSP-安装说明.txt) 为准。历史文件中的“当前”“待测试”只表示当时状态，旧安装命令不可套用当前源码。

## 版本脉络

- 0.1.7：初始发布与保护基线，旧核心和 ZIP 保留。
- 补给/HUD/管理候选：完成原生双选、输入隔离、原生 YES 关闭链、检查点及世界身份校验，形成 [0.1.8](version-0.1.8.md)。
- [体验候选](experience-polish.md)：一步确认、STANDBY/PART、原生图标与布局、部件载量、独立 WAV、TLS 收尾，形成 [0.1.9](version-0.1.9.md)。
- [性能候选](performance-polish.md)：中央样式隔离、菜单扫描与静态刷新优化、补给提前接入；用户复测通过后整理为 1.0.0。
- [音质卸载对照](research/audio-quality-audit.md)：用户卸载后听感相同，未证实模组导致音质下降；恢复回执保留。

## 必须保留的结论

| 问题 | 结论与证据 |
|---|---|
| 原生确认与反射钩子不同步 | setter 不等于用户确认，需要真实输入、同世界/机体、旧页交接；见[原生准备](native-loadout.md) |
| 补给起飞黑屏/提前完成 | 调用真实 YES 完整关闭链，结束事件仅观察；见[流程修订](native-flow-revision.md) |
| 持续锁定音、锁距不对 | 重建前清旧锁定槽音效；走完整 PlayerPlane 切换同步部件；见[输入音效](input-audio-revision.md) |
| 补给后世界身份误判 | 阶段可能在流式子关卡，独立核对阶段 World 与任务 World；见[阶段身份](resupply-completion-revision.md) |
| 动态槽池和 TLS 类型 | ADMM 可超过 64 槽；玩家 TLS 属 AttachedInstantWeapon，按共用接口收尾；见[体验过程](experience-polish.md) |
| UI 克隆与颜色污染 | 只克隆叶控件；中央文字创建干净对象，隔离绑定和全局泛光；见[中央研究](research/persistent-weapon-message.md) |
| 菜单卡顿与补给闪回 | 控件发现和短周期交互分离；视觉接管不等输入开放；见[性能依据](performance-polish.md) |
| 载量与双模型边界 | [弹药规则](research/sp-ammunition.md)、[核心](research/core-findings.md)、[双模型](research/weapon-models.md) |

补给设计与历史比对还保留[原生补给](native-resupply.md)、[AC8 对照](resupply-ac8-comparison.md)、[HUD 合并](hud-candidate.md)和[关键决策](decisions.md)。精选证据不做批量删除。

## 1.0.0 整理

- 重写 AGENTS.md、架构、开发与 Release 索引，移除多段互相矛盾的“当前版本”。研究结论归纳到架构及本页；完整旧内容可从 Git 历史与本轮 backup 恢复。
- 退役根目录 Install.cmd、Install.ps1、Uninstall.cmd、Uninstall.ps1，以及 src/package.ps1、src/prepare-payload.ps1、src/update-local.ps1。它们绑定 0.1.7 或旧载荷，不适用于当前发布；送回收站，不永久删除。
- 保留当前已核验本机部署器 install-resupply-candidate.py，避免为改名重写安装事务；新增单用途 package-release.py 生成手动安装 ZIP。
- 旧核心、旧恢复载荷、0.1.7 ZIP、历史研究脚本和回归源码不删除。原游戏资源、个人存档和配置不进入新发布包或 Git。
- 文档修改前快照位于 backup/release-1.0.0-before-edit，不纳入 Git。发布产物与依赖继续遵守忽略规则。
