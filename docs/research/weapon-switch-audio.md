# 武器切换音效研究（暂停）

> 最新复测：候选 5 的 LAGM 锁距暂未复现异常，但持续锁定音仍在。暂停现场发现音效引用计数与锁定槽标记失配；原生兵装初始化直接丢弃旧槽，漏掉逐槽停止音效。候选 6 在重建前配对释放音效引用，已于 2026-10-14 00:04 编译安装，尚未实玩。详见[音效生命周期修订](../input-audio-revision.md)。自定义 A/B/C 选择音仍暂停。

返回[项目入口](../../AGENTS.md)。2026-10-13 用户要求保留研究，先不实现，优先调通原生双 SP 准备页。

目标为 MSL 使用原生音效 A，SP1 使用原生 B，SP2 使用新增自定义 C。循环与直接选键共用最终逻辑选择；同目标、拒绝选择、装填、检查点恢复和补给初始化不应重复播放选择音。

本机只读资源索引确认 AC7 含 Wwise 音库 `game_se.bnk`、`game_se.pck`。游戏映像字符串含 `HUD_WEAPON_SELECT_NORMAL`、`HUD_WEAPON_SELECT_SPECIAL`，以及 `NimbusSoundManager`、`PostEvent2D` 等标识；这些不是已核对的事件 ID、函数签名或可拦截路径。历史反射确认 `NimbusGameInstance.SoundManager` 属性，但未保留其类的音效函数参数。

当前模组会在 SP1→SP2 时先经过原生类别切换；直接追加 C 可能叠加原生 A/B，不满足完整替换要求。还需核对音效管理器实际签名、武器切换到音效的真实调用路径，确认仅替换选择提示，保持其他音效正常。自定义播放也需考虑游戏音量、暂停和失去焦点。

用户随后报告锁定声由 MSL 变为 SP，并伴随 LAGM 锁距异常。此前把音效确定归因于输入残留、重复释放输入并改用 `ToggleSpecial`，缺少依据且用户复测未解决。主菜单只读分析确认完整飞机切换还会更新 seeker，而底层 `ToggleSpecial` 漏掉此步。候选 5 恢复完整流程，锁定声是否同源仍待战斗复测，不能宣称已解决。详见[本轮复核](../native-flow-revision.md)。未播放、替换或生成选择音效，A/B/C 研究仍暂停。

早期自定义选择音研究没有修改音效逻辑或生成占位音频；候选 6 已修改重建前锁定音收尾，详见顶部最新说明。临时索引保留在 `Temp/switch-audio-index.json`，只含资源路径和大小；不分发原始游戏音频。

接口研究参考：[UE4SS RegisterHook](https://docs.ue4ss.com/dev/lua-api/global-functions/registerhook.html)、[Lua 元数据遍历](https://docs.ue4ss.com/release/lua-api.html)、[Wwise 事件机制](https://www.audiokinetic.com/download/documents/WwiseProjectAdventure_en.pdf)。通用文档不能代替 AC7 的运行时证据。
