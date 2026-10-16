> 历史归档：保留当时结论，不是当前操作指引。当前基线见[项目状态](../../status.md)。

# 0.1.9 整体音质排查

2026-10-14，用户感觉游戏各类音效质量下降，要求卸载对照。21:06 已使用原卸载器，将 DualSP、其独占的 UE4SS 和 dwmapi.dll 移至完整备份；配置保留，存档与图形配置哈希未变。用户随后表示“卸载后也是一样的”，要求恢复。21:11 已恢复原 0.1.9 全套文件与原 mods.txt，并逐文件核验；保留完整备份，没有修改代码、编译、运行测试模块或启动游戏。

此次用户对照未证实模组导致音质下降，也不据此断言所有设备、场景下完全没有影响。回执见[卸载](../../../evidence/0.1.9-audio-comparison-uninstalled.json)与[恢复](../../../evidence/0.1.9-audio-comparison-restored.json)。

## 源码确认

- `src/selection_wave.hpp` 为用户 WAV 创建独立 XAudio2 引擎与输出。0.175 增益只施加于该提示音声源，不修改原生游戏总音量。WAV 的 PCM16/48 kHz 格式不等于改写全游戏音频格式。
- `src/weapon_bridge.cpp` 的 `dualsp_custom_audio_update` 只读取游戏 SFXVolume。未发现设置系统输出格式、全局均衡/滤波、原生音频总线音量或录音设备的代码。
- `weaponSelectionAudio` 仅拦截当前玩家精确匹配的 NORMAL/SPECIAL 选择事件；其他事件传给原函数。锁定音收尾限定于当前玩家 seeker；TLS 收尾限定于玩家所属的 TLS 对象。它们会影响相关声音的触发和结束时机，未发现降低全局音质的操作。
- 当前安装脚本只更新模组载荷，不替换游戏共享 bnk/pck/pak 音库。此结论来自安装路径审查，不是对全部游戏资源进行官方完整性验证。

## 可隔离验证的路径，尚未证实为故障

1. 双 SP 绑定时即创建独立音频引擎，并非第一次播放 SP2 时才创建。提示音结束或取消后，输出仍保留到解绑；额外输出与特定设备/驱动处理的相互影响尚未采集。Microsoft 文档说明该接口默认选择默认输出设备，并将该输出收到的声源送至设备；这本身不是改变全游戏音质的证据。[CreateMasteringVoice](https://learn.microsoft.com/en-us/windows/win32/api/xaudio2/nf-xaudio2-ixaudio2-createmasteringvoice)
2. 切换时在游戏线程销毁旧提示音声源。Microsoft 明确说明 `DestroyVoice` 会等待音频线程空闲，可能短暂阻塞调用线程。该实现存在切换瞬间卡顿的潜在风险；本次没有性能采样或爆音证据，不能用它解释持续发闷。[DestroyVoice](https://learn.microsoft.com/en-us/windows/win32/api/xaudio2/nf-xaudio2-ixaudio2voice-destroyvoice)

如果以后有可重复、仅安装模组才出现的差异，再比较“完整模组”与“不创建自定义音频引擎的模组”。仅将 SP2 音量设为零不能排除额外引擎的影响。本轮依据用户卸载对照结果保留现有实现，不增加未经验证的修复。

返回[音效研究](weapon-switch-audio.md)、[项目入口](../../../AGENTS.md)。
