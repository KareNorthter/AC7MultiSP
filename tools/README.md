# 本机工具清单

当前内部测试版 **2.22.2 alpha1**，位于`Temp/integration-2.22.2-alpha1` / `codex/integration-2.22.2-alpha1`。从正式main/2.22.1接收三个研究线程的精确增量。用户授权编译安装，正式main、v2.22.1与已有发布包保持；待用户实测确认后才定名2.22.2或2.23，不自行递增。

本轮功能：
- 调试页“无限弹药 / Infinite ammo”“快速装填 / Fast reload”默认关闭、勾选即时生效，仅作用于本Mod管理的SP，不影响普通导弹、机炮或干扰弹。无限弹药保持当前库存、不自动补满；快速装填将合格SP等待缩短到最多1秒，排除PLSL/HGP/MGP及持续效果类。开关只在本次游戏会话保持。
- 注视正常开始、结束静默；错误/无目标警告保留。英文界面按已批准中文同步。
- Lua/核心/UI共用后台日志入口：mod四份、native六份，每份最多1,000,000字节，总上限10MB。低优先级、限速批量写入；有界队列忙时丢弃、重复抑制，不让调用线程等待磁盘。启动后台整理旧超大日志，日志页按需读最近64KiB；最后未落盘记录可能丢失，流畅度仍待实测，不承诺零开销。
- Mod电子战依据真实CDO类、回调及装填机制判定，兼容合格ECM/ESM/IEWS；TLS/UAV保留既有资格与退场边界，不仅按武器编号放行。不可支持的回调明确拒绝。
- 有符号32位负弹药数表示无限，零为空弹、正数沿原规则；保留原负值、不按比例缩减，机库/HUD显示∞，直选/轮切/齐射同步放行。数组长度、待发数量和装填计时仍严格检查；无限库存仅使用现有模型池容量。
- CFA利用开场已有EML初始化预热炮口字段与模板缓存，合并重复查找，不额外轮切、发射或逐帧扫描。开场启用任务多SP模型时，先显示“加载多SP模型中，请等待...”，等待真实显示后继续初始化，持续到兵装/HUD/模型准备结束且至少显示1秒；最低显示时间不拖延武器就绪。关闭模型、离场、换机或错误清理提示。性能改善幅度仍待实测。

整合保持管理20/配置19/注视4/镜头8/换机2/视角选目标3。新增SP调试、后台日志及加载提示导出须核心/UI/Lua成套。有效配置19保持原字节，不使用--reset-config；本机模型快捷键-、SP组合=及个人方案保留。资源沿用77项/154成员PAK的已验收字节，不新增布局。

版本工具只接收三段数字版本或带“ alphaN”的内部标识，安装回执使用不带空格的2.22.2-alpha1前缀；不把研究2.27.x写成正式发布序列。来源清单见evidence/2.22.2-alpha1-source-intake.json，构建与安装另记。

以下正式2.22.1及历史记录保留原阶段含义，当前候选以上文为准。

正式主线/发布为 **main / v2.22.1**。用户已验收2.23.0-refresh安装成果并授权合main及本地发布；研究编号不作为正式版本。管理20/配置19/注视4/镜头8/换机2/视角选目标3，19运行文件、77资源/154成员成套。核心与PAK沿用验收字节，UI仅等长版本文字替换，指令段不变；不重新编译或运行测试模块，沿用对应11项已通过检查。旧冻结和安装回执不改，本机运行文件仍是已验收2.23.0标识，本次只另改本机两个快捷键，不重装、不上传。

管理三页；手柄/键盘注视各默认关；视角保持默认关、Ctrl+Alt+F6；齐射默认开、白点默认关、文本切后1秒。任务/机库模型独立，切组合/任务模型发布默认未绑定。旧配置先原字节备份，再重置为配置19，不迁移；有效配置19正常保留。F/A-18E TGM用r2，其他152资源成员保持；离线资格守卫不增加游戏逐帧扫描。

发布工具src/release_package.py的--accepted-label-only-build严格绑定原构建、检查、显式validated_installed_receipt安装回执及用户授权；仅准许四处标识源文件的精确变化。approved_tooling_files可为空或仅src/install.py，空值表示不接收额外工具改动。product仅改candidate、upgrade_receipt和既有receipt_prefix为正式元数据；其余来源不变。DLL核心逐字节复用，UI唯一NUL分隔等长版本文本仅位于不可执行.rdata，.text一致。旧2.21.1包完整回读后仅复用固定依赖及158微软运行库，逐成员核对新包。

验收：[2.22.1用户确认](../evidence/2.22.1-formal-user-validation.json)；发布及版本证明分别为evidence/2.22.1-formal-release.json、evidence/2.22.1-formal-version-label.json。

以下旧研究与交付记录保留原阶段含义；当前规范以上文及正式ReadMe为准。

正式主线/发布统一为 **main / v2.22.0**。用户确认已安装2.26.0研究候选“测试了，没问题”，授权本地发布2.22.0，不上传、不重装游戏。核心和77资源/154成员PAK沿用验收字节，UI仅等长版本文本换版，Lua版本注释/日志及VERSION改为2.22.0；本机安装回执仍为2.26.0。安装核验简化工具a62ea25并入源码，不改游戏功能。验收/发布记录使用2.22.0-formal前缀，旧2.22.0及2.23～2.26候选记录不覆盖。

以下研究、旧构建和安装记录保留原阶段含义；当前规范以本节及正式ReadMe为准。

本地安装器默认只严格核验本Mod、挂载包、关键加载器和存档，保留完整备份与回退。其他Mod全量字节审计可加`--verify-other-mods`；其结果不代表兼容性。详见[开发说明](../docs/development.md#安装保护核验范围)。当前2.26.0安装与冻结不变，本轮工具改动下次授权构建后使用。

返回[项目入口](../AGENTS.md)，操作入口见[开发说明](../docs/development.md)。工具二进制与 Python 依赖被 Git 排除，保留在本机供用户明确要求的构建和研究使用。

| 位置 | 用途 |
|---|---|
| `llvm-mingw-20261006-ucrt-x86_64/` | clang 23.1.3、UCRT x64 便携 C/C++ 工具链，原安装位置已迁移到本目录 |
| `assets/UAssetGUI.exe` | UAssetGUI 1.1.0，读取 UE4.18 资源副本；CLI 版本参数使用 `VER_UE4_18` |
| `assets/AC7Key.bin`、`AC7Decrypt.cs` | 自有 AC7 资源副本的离线解码资料 |
| `dotnet8/` | 用户授权的项目本地 .NET 8.0.31 基础与桌面运行库，供 UAssetGUI 使用；不修改系统 PATH |
| `capstone/`、`pefile.py` 等 | 原生反汇编和 PE 信息读取依赖 |
| `vgmstream/`、`wwiser.pyz` | vgmstream r2117 与 wwiser v20260808，解析自有 Wwise 音库和生成本地试听；来自官方 GitHub Releases，不随模组发行 |
| `test-python/` | 既有 Lua 模拟检查及资源解析所需 Python 依赖 |

PowerShell 7、Python 3.13 和 Git 属于本机通用环境，不复制进项目。新建工具或依赖继续安装在 C 盘以外，临时文件使用 `Temp/`。

本地 .NET 8 来自[微软官方发行元数据](https://builds.dotnet.microsoft.com/dotnet/release-metadata/8.0/releases.json)，两个 Windows x64 ZIP 分别按元数据 SHA512 核对后解压至 `dotnet8/`。桌面 ZIP 只含桌面组件，需同时安装基础 Runtime ZIP。`src/inspect_resupply_assets.py` 只为资源解析子进程设置 `DOTNET_ROOT_X64`，不设置全局环境变量。

来源：[LLVM-MinGW](https://github.com/mstorsjo/llvm-mingw/releases/tag/20261006)、[UAssetGUI](https://github.com/atenfyr/UAssetGUI)、[UAssetAPI 解码实现](https://github.com/atenfyr/UAssetAPI/blob/master/UAssetAPI/AC7Decrypt.cs)。项目安装载荷使用 [Playmore 的 AC7 专用 UE4SS](https://github.com/Playmore-ModdingConcepts/PM-UE4SS/releases/tag/UE4SS)。许可证保留在各依赖及 `payload/ue4ss/Mods/DualSP/licenses/` 中。

音频研究工具来源：[vgmstream r2117](https://github.com/vgmstream/vgmstream/releases/tag/r2117)、[wwiser v20260808](https://github.com/bnnm/wwiser/releases/tag/v20260808)。原始音库和解码试听只保存在 Temp，不加入 Git。
