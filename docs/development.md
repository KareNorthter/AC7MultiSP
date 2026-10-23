# 开发与恢复

本页说明工程入口、真实文件依赖和恢复方法。当前正式版本、本机已知记录与待接收成果统一见[项目状态](../README.md)；本轮未检查游戏实时状态。历史2.24.1整合/发布当时按授权未安装、未上传，依据为[整合回执](../evidence/2.24.1-integration.json)、[发布回执](../evidence/2.24.1-release.json)及[发布授权](../evidence/2.24.1-release-authorization.json)，不能把当时回执当作今天的安装状态。

开发规范、分支/提交、版本号、并行责任和授权范围统一见[工作流程](workflow.md)。功能机制见[架构](architecture.md)，模型方案见[挂载设计](multi-sp-mount-design.md)，工程教训见[工程经验](engineering-lessons.md)，未采用和待继续的成果见[研究索引](research/README.md)。本页不再累积每轮交接。

用户已明确后续重做打包交付流程。下列构建/安装/发布工具仅记录现有实现与历史恢复依赖，不把旧交付工具的源码冻结规则用作阻止文档整理的条件；本次整理不运行或修改这些工具。

## 源码与产物入口

| 内容 | 唯一入口或用途 |
| --- | --- |
| Lua运行源码 | `dist/DualSP/Scripts/`；产品文件名由`src/product.json`的scripts清单约束 |
| 核心/UI | `src/weapon_bridge.cpp`、相关`.hpp`；`src/overlay.cpp`、`src/management_panel.hpp` |
| 产品标识 | 根`VERSION`、`src/product.json`；Lua日志和UI版本文本随正式变更同步 |
| 挂载生成来源 | `src/research/build_runtime_mount_data.py`及其导入的数据模块；生成`src/mount_expansion_data.hpp`等表及`mount_assets.lua`，以生成器实际输出为准 |
| 资源打包 | `src/build_mount_assets.py`按生成白名单、来源证据、碰撞与Socket资格核对本地`.uasset/.uexp`，生成自有PAK |
| 完整构建 | `src/build.py`；旧`.cmd`包装不替代成套入口 |
| 现有检查 | `tests/run.py --build-dir`；会运行既有检查、编译其C++用例并检查DLL加载导出，须有本轮授权 |
| 安装/预检 | `src/install.py --build-dir`；只有显式`--apply`交换游戏文件 |
| 本地发布 | `src/release_package.py`；`src/package-release.py`为包装入口 |
| 本机取证 | `启动游戏并取证.cmd` → `src/research/start_crash_capture.ps1`；按需使用，原始转储留Temp |

`dist/DualSP/bin/DualSP.dll`是受保护旧核心，不是新构建输出位置。新DLL只生成到独立构建目录；其保护摘要和旧核心保护规则见[工作流程](workflow.md)。

## 构建前必须具备的本机依赖

- 本机工具根为`E:/AC7_MultiSP/tools`，产品清单指定`llvm-mingw-20261006-ucrt-x86_64`；构建还使用`tools/test-python`中的lupa/pefile及仓库`vendor/`。实际执行前核对存在和版本，不用旧文档代替现场检查。
- 当前运行规模为19文件、77资源/154成员。可追溯的完整资源输入为`E:/AC7_MultiSP/Temp/integration-2.23.0-refresh/Temp/mount-assets-2.23.0-refresh/source`，见[refresh整合](../evidence/2.23.0-refresh-integration.json)；整理时目录仍有154成员。数量只用于初筛，名称、字节及当前来源证据必须逐项一致。
- `src/build.py`的默认资源目录仍是旧`Temp/mount-assets-2.14.0/source`，不能据默认值推定它满足当前产品。旧`Temp/mount-assets-2.20.1/source`只有54资源，也不能直接替代当前输入。独立工作树没有自动携带ignored资源，构建须显式传`--mount-assets-dir`。
- Git源码不能代替本地资源。原模型、原版PAK、资源提取、离线几何和构建目录均可能被忽略；接收时应同时核对来源证据与本机原料，保留旧输入，不覆盖同名历史副本。

完整来源冻结由`src/release_tools.py`的`source_snapshot()`定义，包含产品C++/头文件、vendor、清单内Lua、工具、研究脚本和多份原路径证据；已不再是早期文档所写的固定11份证据。`src/build_mount_assets.py`还核对整套Socket引用、七机碰撞保真、F/A-18E反馈及TGM r2等证据。改动或移动这些文件会改变构建来源，不能只更新摘要让旧冻结通过。

TGM当前用修正后的`MSP_f18etg_gpb_inner_whole_r2`，旧r1资源不在运行白名单；完整输入必须与[Socket资格](../evidence/mount-socket-reference-audit-20261021.json)和[TGM r2回执](../evidence/f18etg-whole-rack-socket-r2.json)一致。表、Lua清单和PAK成套接收，不能混用54/65/74/77资源阶段的整表或编号。

## 现有接收与交付工具的依赖参考

1. 先定位来源提交、相对已接收基线的增量及所需本机资源。只接收本次增量，分别列清源码、生成表、Lua、证据与资源输入；研究分支曾写“未构建”只说明当时状态，是否已进入main须对照提交和后续回执。
2. 有构建授权时，使用当前工作树的唯一源码、显式工具/资源目录和全新`Temp/build-*`。构建冻结来源、工具链、DLL、对象、Lua与PAK；失败目录保留，修复后另建新目录。构建不会安装或启动游戏。
3. 有检查授权时，现有检查入口消费该次`build-receipt.json`并生成独立`test-receipt.json`。协议或字段变化应同步现有检查的夹具；旧双SP模拟已排除，不能用它证明当前引擎行为。离线检查通过不等于实机通过。
4. 有安装授权时，`--base-receipt`必须对应实际已安装的完整runtime_files。main内product历史默认指向`evidence/2.24.0-alpha1-installed.json`，只是工具配置，不证明当前实装；执行前需核对现场。预检通过后才按授权应用，游戏应退出，完整旧Mod、个人文件和存档先备份，交换后回读。
5. 用户反馈、main合入、正式标识、安装和发布分别记账。2.24.1原发布当时使用`--uninstalled-release --without-runtimes`；该回执只说明当时未安装，后续EXE安装记录见[项目状态](../README.md)。日后继续发布必须匹配当轮授权与真实来源。

历史版本仅改标识的发布有两条受限路径：`--validated-build-dir`核对标识重建，`--accepted-label-only-build`沿用已测指令字节、只允许指定只读版本文本精确变化。允许的源文件、元数据、长度和PE节差异由发布工具及原验收证明决定；不能把它当通用二进制修改入口，也不能改写旧构建/安装回执。发布不附运行库须显式`--without-runtimes`并与授权一致。

## 安装保护与配置

默认安装严格核验本Mod、自有PAK、共享加载器关键文件、游戏程序及存档，不读取其他Mod/PAK的全部内容。`--verify-other-mods`可额外做完整字节审计；这仍不是兼容性测试。回执`other_mod_files_preserved=null`表示未全量核验，`other_mod_files_touched=false`只表示本安装器未写入；不能互相代替。

当前配置19：有效文件保留，旧格式/损坏JSON先按原字节另名备份，再生成新版默认值；配置19内部无效设置仍报错。`--reset-config`会额外隔离旧profiles，属于明确的重置操作，不能用于普通保留升级。旧18→19迁移链和当时备份名留在历史记录，不作为当前仍会执行的承诺。

Vortex部署的符号链接不能由手动安装器跟随覆盖。alpha1现场按用户选择先把本Mod游戏端链接备份，再物化普通文件并复核；没有改Vortex原包或状态库，后续Vortex部署仍可能替换手动候选。相关现场见[预检](../evidence/2.24.0-alpha1-install-preflight.json)和[完成回执](../evidence/2.24.0-alpha1-install-completion.json)。

## 恢复与取证

恢复未采用的研究先读[研究索引](research/README.md)及[历史归档](archive/README.md)。从main建立新研究分支，使用标签查看所需旧文件，再提取精确差异；旧运行核心、Lua、资源和整套文档不能整支覆盖当前产品。Git bundle、未提交差异/未跟踪ZIP与ignored原料各自承担不同恢复范围，锁定工作树也不代表文件只读。

本轮整理前正文保存在`archive/20261023/pre-governance-main`标签中；早期正文和归档清单见[归档索引](archive/README.md)。旧文中的“当前”、候选编号和绝对路径属于当时阶段，需与当前提交、现场文件及后续回执交叉核对。

取证脚本修订2使用硬件退出/消息框断点、独立输出和产物核验；完整转储有磁盘预算，仍存在附加前窗口、非Win32界面、外部终止等边界。调试器可能改变时序，不用于无干预帧率对照，也不保证每次根因都能捕获。原始转储留Temp，不入发布包，详见[取证改进](../evidence/crash-capture-hardening-2.20.2.json)。

## 仍未解决或未覆盖的事项

| 事项 | 当前证据边界与继续入口 |
| --- | --- |
| 更新与交付流程 | 历史发布回执未覆盖后续EXE安装；已知记录与待接收安装器统一见[项目状态](../README.md)。本轮未核查游戏目录、线上清单或下载文件；旧[发布更新说明](Pages与R2发布更新说明.md)仅作历史参考，流程由用户后续重做 |
| CFA切换尖峰与偶发视觉 | 用户确认明显减轻，仍有尖峰；ECM持续效果阶段未细分到单函数。偶发EML蓝光根因未证实，见[工程经验](engineering-lessons.md)、[挂载设计](multi-sp-mount-design.md) |
| 选装进入SP1/光标、结算箭头、F-2A性能 | 结算轻卡仍有，其他专项保留；没有全场景性能结论，不因旧候选已合入就写成解决 |
| MiG-31B r3及动态净空 | 整体布局获认可，后腹SAAM的r3位移、支撑/机腹净空仍需观察；离线包络不能替代动态验收 |
| F-14IA原生异步依赖 | 当前提前退回原生并保留本机隐藏隔离；不代表修好第三方蓝图加载顺序，见[安全退让证据](../evidence/special-aircraft-native-fallback-20261022.json) |
| PW旧出击退出、CFA暂停对象创建异常 | PW一次再试成功，首轮根因未明；CFA原始异常在UI对象创建跳板，不能与ADMM队列或最终退出断点强判同因。见[PW记录](../evidence/pwmki-sortie-crash-2.20.1.json)、[CFA记录](../evidence/cfa44-admm-debug-pause-crash-20261021.json) |
| 兼容与半成品 | 完整内舱、ASF-X/RKT、旧updater、FOV及来源工作树的未提交改动继续保留；状态与重启研究入口见[研究索引](research/README.md) |

已修复的ADMM无限队列边界、停用Bomb误拦截、内存交接可靠性和暂停换机接续不再列为未接收候选；它们的故障现场与限制仍保留在[工程经验](engineering-lessons.md)。当前用户确认不扩张为全部机型、所有Mod、固定帧率或每个边界通过。
