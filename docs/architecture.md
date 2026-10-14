# 架构与关键约束

返回[项目入口](../AGENTS.md)。本文描述 **AC7DualSP 1.0.0**；修订经过与旧方案见[历史索引](history-1.0.0.md)。

## 模块

| 模块 | 职责 |
|---|---|
| main.lua | 游戏线程调度、世界/玩家绑定、配置、补给与检查点生命周期 |
| weapon_cycle.lua | MSL/SP1/SP2 顺序、耗尽跳过 |
| loadout_flow.lua | 准备与补给共用的双选草稿，确认前不保存 |
| native_loadout.lua | 准备、补给、STANDBY、PART 的独立页面适配器 |
| native_hud.lua | 右下固定行、选择箭头、按就绪状态调节名称亮度 |
| weapon_names.lua、weapon_presentation.lua | 共用简称及独立中央绿色常驻提示 |
| json.lua | 配置与状态序列化 |
| weapon_bridge.cpp / DualSP.dll | 兵装重建、独立弹药/冷却、原生输入/音效桥接 |
| overlay.cpp / DualSPUI.dll | F8 管理、只读导航、直接选择输入、内存状态及动作队列 |
| 专用 UE4SS | 游戏线程定时接口、Lua 与原生桥接 |

当前武器核心来自 cores/0.1.9，受保护的 bin/DualSP.dll 仍是旧 0.1.7，不能混装。对外名称为 AC7DualSP，内部文件名和协议仍保留 DualSP。

## 数据与调度

原生双选草稿 → 确认事务 → 按机体保存方案 → 游戏线程武器核心 → HUD。渲染线程只读内存状态，不访问 UE 对象；F8 设置和选择请求交给游戏线程校验。

短命令/状态文件只用于需要的核心事务，带请求编号，状态完整写完后原子替换。任务中不周期写武器快照，空 UI 动作队列不写文件。所有 Lua 更新和延迟输入均在游戏线程，不重新引入异步回调并发。

菜单对象按世界缓存，全局发现每 500 ms 一次；反射激活事件直接传入页面，短周期只检查缓存可见性。非任务世界不扫描 PlayerPlane。STANDBY 在进入/重新开放控制时刷新，PART 每 250 ms 读已装部件，静态信息与弹药颜色只在变化时写。活动补给等待页面期间每 20 ms 发现，正常飞行不扫描补给页。每 15 秒 PERF_TICK 汇总 Lua 回调均值/峰值，不代表整帧开销。见[性能说明](performance-polish.md)。

## 武器与音效

InitializeWeaponSystem 会重建系统：保存普通导弹、机炮/干扰弹、两套 SP 计数、装填队列及捕获时间，再映射到新槽位，用游戏时间推进冷却，不能复用旧实例指针。

每次重建之前逐槽释放锁定音引用，检查归零后再初始化。切换走完整 PlayerPlane 按下/释放流程，使锁定组件重新绑定并应用部件修正，不能只 ToggleSpecial。逻辑切换完成后统一提交箭头、简称和选择音。

TLS 离开前使用共用收尾虚函数自然结束光束；玩家 TLS 为 AttachedInstantWeapon，不以 LaserWeapon 的 Tick 判断。动态槽池按实际数组与可读范围检查，不设 64 槽上限（ADMM 历史实例为 126）。

selection_wave.hpp 使用系统 XAudio2 2.9，SP2 WAV 增益为原生音效音量的 0.175 倍；游戏线程同步暂停与音量，解绑先销毁声源再释放样本。MSL/SP1 使用原生音效。普通导弹保留上一 SP 挂载，双模型未实现。

## 页面与提交

准备页一次确认 SP1、一次确认 SP2，随后交还真实原生动作进入 STANDBY；SP1 返回交还取消。光标预览与保存分离，原生 setter 被调用不等于确认；保存还要求真实决定、相同机体/世界和原页面交接。

双栏只复制叶控件，不复制 UserWidget/Slots 子树；运行时布局支持 Canvas、Overlay、HorizontalBox，退出时还原。STANDBY/PART 共用只读展示，但各自维护控件实例；只在页面隐藏时还原，不把输入停止当作立即隐藏信号。

补给开始即绑定并隔离本页原生列表/YES-NO 输入。控件树可见、世界正确即可视觉接管，不等 IsEnableControl。SP1/SP2 之后是 YES/NO；YES 等决定键释放，恢复原生视觉树，通过真实 YES 入口完成菜单关闭链。结束事件仅观察，不能主动模拟完成；任务报告补给成功且玩家可用后才应用并保存。

阶段 Actor 的 GetWorld 可能来自流式子关卡：保存阶段实例及其关卡 World，另核对 GameInstance 任务 World，不能直接比较两种 World。离开任务、错误或检查点恢复时清理身份与缓存。补给后的新配置不得用于恢复旧检查点快照。

## HUD 与失败边界

中央为干净 TextBlock，固定正常绿色、局部绿色泛光，字号原生的 10/9；不复制动态颜色绑定或隐藏动画。中央与右下未就绪时亮度为正常 35%，共用只读就绪采样，各有独立开关并服从全局显隐。

武器错误停止修改；UI 错误明确展示与记录，不静默改用其他界面。控件关闭要还原原生状态，不能用吞错继续冒充接入成功。安装器拒绝不匹配的 DLL/Lua/加载器组合。

细节依据保留在[核心研究](research/core-findings.md)、[弹药](research/sp-ammunition.md)、[原生流程](native-flow-revision.md)、[输入音效](input-audio-revision.md)与[补给阶段身份](resupply-completion-revision.md)。
