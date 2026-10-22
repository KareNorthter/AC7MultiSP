#pragma once
// 按docs/授权.txt和已回传设计稿保存中英文本；作者与贡献者名字保留原名。
// 编译为只读文字，展开页面时不读取磁盘。
namespace dualsp::updates {
inline constexpr char creditsZh[]=R"AC7CREDITS(感谢以下项目、开发者和贡献者：

• UE4SS — Narknon及UE4SS-RE贡献者
提供运行时脚本框架。

• PM-UE4SS — Playmore-ModdingConcepts
提供AC7专用UE4SS适配。

• Dear ImGui — Omar Cornut及贡献者
提供游戏内管理界面所用的界面库。

• MinHook — Tsuda Kageyu
提供函数挂钩库；其中HDE反汇编组件由Vyacheslav Patkov开发。

• nlohmann/json — Niels Lohmann及贡献者
提供JSON数据处理库。

• PROJECT ACES / Bandai Namco
感谢《皇牌空战7》原作。


对应的第三方组件许可证已随包保留，位于ue4ss/Mods/DualSP/licenses。)AC7CREDITS";
inline constexpr char creditsEn[]=R"AC7CREDITS(Thanks to the following projects, developers and contributors:

• UE4SS — Narknon and UE4SS-RE contributors
Runtime scripting framework.

• PM-UE4SS — Playmore-ModdingConcepts
AC7-specific UE4SS adaptation.

• Dear ImGui — Omar Cornut and contributors
User interface library used by the in-game management panel.

• MinHook — Tsuda Kageyu
Function hooking library. The HDE disassembler component was developed by Vyacheslav Patkov.

• nlohmann/json — Niels Lohmann and contributors
JSON data processing library.

• PROJECT ACES / Bandai Namco
Thanks for creating Ace Combat 7.


The corresponding third-party licences are included in ue4ss/Mods/DualSP/licenses.)AC7CREDITS";
}
