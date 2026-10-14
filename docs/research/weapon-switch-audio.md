# 武器切换音效研究

2026-10-14 21:11：用户对照完整卸载后的游戏，表示听感相同；已按要求恢复原 0.1.9。源码未发现全局音质设置修改，独立输出与声源销毁的潜在影响及验证边界见[音质排查](audio-quality-audit.md)。本轮未修改音频实现。

## 0.1.9 当前音量

用户确认其他功能正常，要求 SP2 在候选 9 基础上再降低 50%。当前增益为 **0.175 × SFXVolume**，原 WAV 不改，两处声源音量设置保持一致；最终听感待用户试听。下文 0.35/0.70 均为历史设置。

## 体验候选 9 当前音量

用户复测候选 8 仍觉得 SP2 偏响；已安装 DLL 与包含 0.70 增益的构建一致。本批改为 **0.35 × SFXVolume**，相对上一版再减半，两处声源设置同步更新；原 WAV、其他提示音和暂停策略不改。新的听感待复测，下文 0.70 为历史设置。

## 体验候选 7 音量

用户指定的 SP2.wav 保持原文件，XAudio2 两处设置音量统一为 `0.70 × SFXVolume`，相对候选 6 降低 30%。保存的仍是未经缩放的游戏音量，重复播放不会累计衰减。暂停、焦点、尾音取消和 MSL/SP1 原生提示均沿用原逻辑；实际听感待用户试听。

> 2026-10-14：候选 5 源码按用户要求改为本地 `SP2.wav`（PCM16/48 kHz/立体声，约 0.604 秒），现已随体验候选 6 安装，尚待实玩试听。使用系统 XAudio2 2.9 独立声源，当前 SFXVolume、暂停及前台状态在游戏线程更新；不替换共享 Wwise 媒体。实现与限制见[体验说明](../experience-polish.md)。缓冲区存活与声源销毁遵循 [Microsoft XAudio2 播放说明](https://learn.microsoft.com/en-us/windows/win32/xaudio2/how-to--play-a-sound-with-xaudio2)。

> 2026-10-14 最新复测与候选 2：候选 1 用户确认 SP2 无声；媒体解码证明非静音，但 VR Sound 带专用三维配置，且与 NORMAL/SPECIAL 共用一份媒体。候选 2 按用户试听选择改用普通 HUD_WEAPON_RESTRICT_RELEASE（875504770，0.34 秒），游戏听感待复测。声音事件不改变武器限制状态。

> 候选 1 历史设计：[体验源码候选](../experience-polish.md)首次接入 SP2 选择事件时复用 `HUD_WEAPON_SELECT_VR`，核对了激活器播放路径、FString 所有权和音库对象。只过滤当前玩家精确的 NORMAL/SPECIAL 选择事件，最终逻辑选择完成后播放一次，保留锁定音生命周期。该音色随后被用户实测否定，已由候选 2 替换；详见[原研究记录](../../evidence/experience-polish-findings.json)。

> 历史复测：候选 5 的 LAGM 锁距暂未复现异常，但持续锁定音仍在。暂停现场发现音效引用计数与锁定槽标记失配；原生兵装初始化直接丢弃旧槽，漏掉逐槽停止音效。候选 6 在重建前配对释放音效引用，已于 2026-10-14 00:04 编译安装。详见[音效生命周期修订](../input-audio-revision.md)；当时自定义 A/B/C 选择音暂停。

返回[项目入口](../../AGENTS.md)。2026-10-13 用户要求保留研究，先不实现，优先调通原生双 SP 准备页。

目标为 MSL 使用原生音效 A，SP1 使用原生 B，SP2 使用新增自定义 C。循环与直接选键共用最终逻辑选择；同目标、拒绝选择、装填、检查点恢复和补给初始化不应重复播放选择音。

本机只读资源索引确认 AC7 含 Wwise 音库 `game_se.bnk`、`game_se.pck`。游戏映像字符串含 `HUD_WEAPON_SELECT_NORMAL`、`HUD_WEAPON_SELECT_SPECIAL`，以及 `NimbusSoundManager`、`PostEvent2D` 等标识；这些不是已核对的事件 ID、函数签名或可拦截路径。历史反射确认 `NimbusGameInstance.SoundManager` 属性，但未保留其类的音效函数参数。

当前模组会在 SP1→SP2 时先经过原生类别切换；直接追加 C 可能叠加原生 A/B，不满足完整替换要求。还需核对音效管理器实际签名、武器切换到音效的真实调用路径，确认仅替换选择提示，保持其他音效正常。自定义播放也需考虑游戏音量、暂停和失去焦点。

用户随后报告锁定声由 MSL 变为 SP，并伴随 LAGM 锁距异常。此前把音效确定归因于输入残留、重复释放输入并改用 `ToggleSpecial`，缺少依据且用户复测未解决。主菜单只读分析确认完整飞机切换还会更新 seeker，而底层 `ToggleSpecial` 漏掉此步。候选 5 恢复完整流程，锁定声是否同源仍待战斗复测，不能宣称已解决。详见[本轮复核](../native-flow-revision.md)。未播放、替换或生成选择音效，A/B/C 研究仍暂停。

早期自定义选择音研究没有修改音效逻辑或生成占位音频；候选 6 已修改重建前锁定音收尾，详见顶部最新说明。临时索引保留在 `Temp/switch-audio-index.json`，只含资源路径和大小；不分发原始游戏音频。

接口研究参考：[UE4SS RegisterHook](https://docs.ue4ss.com/dev/lua-api/global-functions/registerhook.html)、[Lua 元数据遍历](https://docs.ue4ss.com/release/lua-api.html)、[Wwise 事件机制](https://www.audiokinetic.com/download/documents/WwiseProjectAdventure_en.pdf)。通用文档不能代替 AC7 的运行时证据。
