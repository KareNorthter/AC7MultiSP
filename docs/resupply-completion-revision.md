# 管理候选 7：补给正常起飞后模组停用

> 2026-10-14 更新：用户确认候选 7 的中途补给正常、所有功能初步可用，现整理为 [0.1.8](version-0.1.8.md)。大范围及持久测试待完成；下文候选状态和“待实玩”属于历史记录。当前安装见[0.1.8 回执](../evidence/0.1.8-installed.json)。

返回[项目入口](../AGENTS.md)。输入隔离和音效生命周期沿用[候选 6](input-audio-revision.md)。

## 用户实测与本轮范围

用户复测候选 6：持续锁定声未再出现；SP1、SP2、起飞确认和起飞流程正常，但返回飞行后模组全部失效。LAGM 锁距此前用户暂未复现异常。本轮读取暂停现场，未恢复飞行、写入运行内存或操作存档。

候选 7 只修改 Lua 的阶段身份校验，候选 6 核心和 UI 均沿用，未重新编译 DLL、未新增或运行测试模块。已于 **2026-10-14T08:50:13.678319+08:00** 成套安装，见[安装回执](../evidence/management-candidate.7-installed.json)；修复效果尚未实玩验证。开场单 SP HUD 闪现继续暂缓。

## 已确认的失败点

日志在 `2026-10-14T00:38:43Z` 依次记录起飞请求、调用、源视觉恢复及 `NATIVE_RESUPPLY_YES_RETURNED`；下一秒报错 `补给阶段不属于当前任务世界`。原生日志也有 `RESUPPLY_NATIVE_YES_BEGIN/RETURNED`。这次真实 YES 已完成调用，错误来自随后 Lua 的补给结束前钩子。全局错误使成功通知不再处理，补给事务没有提交，HUD 和切换一直暂停。

旧代码把 `phase:GetWorld()` 与 `GameInstance:GetWorld()` 直接比较。这两个查询在本机专用加载器中不是相同语义：

- 返航阶段 `MiniGamePhaseBP_ReturnLine_C` 的 Outer 是 `ms06_Phase_ReturnLine` 子关卡；加载器的 Actor 查询沿 Outer 链返回该子关卡 World。
- 游戏原生 Actor 查询通过所属 Level 的 `OwningWorld` 获取运行世界；返航阶段、玩家与 GameInstance 实际均属于主任务 `ms06`。

现场对象关系、游戏代码和本机 UE4SS 导出反汇编共同确认这一差异，不是通过错误文字猜测。游戏原生 Actor GetWorld 位于 RVA `0x15632F0`，取 Level 后读取 `+0xC0`；本机 UE4SS Actor 包装位于 RVA `0x368660`，沿 Outer 查找 World。匹配版本的 [UE4SS LuaAActor 源码](https://github.com/UE4SS-RE/RE-UE4SS/blob/48e7edfe/UE4SS/src/LuaType/LuaAActor.cpp)也确认 Lua 绑定调用此 Actor 包装；本机二进制证据优先于上游版本。

因此此前“同一任务对象的 GetWorld 地址必然相等”的假设不成立。旧方案允许结束时重绑阶段也没有本轮证据支持；这次开始到结束没有阶段替换记录。

## 新的事务约束

1. 菜单开始时绑定阶段实例，并保存它自己的关卡 World。
2. 结束通知必须来自同一阶段实例；它的关卡 World 必须与开始时一致。
3. GameInstance 的运行任务 World 必须与菜单接入时一致；任务成功回调仍匹配本轮 Mission，恢复时仍匹配原机体和任务。
4. 原生结束函数返回后标记事务完成；收到任务补给成功、玩家武器可用后，才提交配置、释放页面、重建两套 SP 并恢复 HUD 和切换。

不再比较子关卡 World 与主任务 World，也不在结束回调中改绑陌生阶段；其他身份或顺序异常仍明确报错。音效逐槽收尾、原生输入隔离、完整飞机切换及真实 YES 入口保持不变。

## 核验与复测边界

安装只核验 Lua 语法、核心导出、文件哈希和保护文件，不代表实玩通过。重启后优先复测同一任务的一次完整补给，确认右下角双 SP、中央当前武器、MSL/SP1/SP2 切换和补给后的弹药恢复。正常日志应出现 `FINISH_VALIDATED → FINISH_ACCEPTED → RESUPPLY_LOADOUT_APPLIED`；成功通知可能嵌套在结束函数内部，所以不要求所有日志固定排列。

诊断摘要见[证据](../evidence/resupply-completion-revision.json)。修改前现场备份为 `backup/native-flow-20261014-084125`。本轮没有把现有错误任务热恢复为正常任务，需要重启加载新 Lua。
