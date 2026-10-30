# 开发与恢复

本页说明源码、文字生成、构建依赖和恢复入口。正式版本及本机已知记录统一见[项目状态](../../README.md)，不把候选制作记作正式发布或游戏验收。开发责任与授权见[开发规范](../project/workflow.md)，交付实现见[交付方案](../project/delivery.md)。

安装器从`a99a404`精确接收`src/delivery/`及原`src/package_delivery.py`后改造；旧独立工作树保持。完整构建、封装及EXE编译分别记录来源与结果。用户可只要求交付EXE：仍按既定流水线生成内嵌的通用载荷，但独立ZIP只作为Temp中的封装输入，不复制到Release。构建不自动运行`tests/run.py`、安装、操作游戏或上传。

功能机制见[架构](architecture.md)，模型方案见[挂载设计](multi-sp-mount-design.md)，工程教训见[工程经验](engineering-lessons.md)，未采用和待继续成果见[研究索引](../research/README.md)。

候选版本支持 `alphaN` 和 `beta`；同号版本按 alpha、beta、正式版排序，二者均使用候选签名渠道，不作为 stable 发布。

## 源码与产物入口

| 内容 | 唯一入口或用途 |
| --- | --- |
| Lua运行源码 | `dist/DualSP/Scripts/`，文件名由`src/product.json`的scripts清单约束；姿态指示器候选已登记`attitude_hud.lua`，运行文件数量以该清单为准，不在本文重复维护 |
| 核心/UI | `src/weapon_bridge.cpp`及相关头文件；`src/overlay.cpp`、`src/management_panel.hpp` |
| 产品标识 | 根`VERSION`与`src/product.json`；游戏候选标记为candidate=true，安装器程序集、manifest和清单版本均派生产品VERSION |
| 玩家文字唯一源 | `docs/user/`四个主题的中英文TXT，以及`docs/user/interface/`下游戏和安装器的双语JSON |
| 游戏文字生成 | `src/user_texts.py`生成`src/generated/user_texts.hpp`和`dist/DualSP/Scripts/user_texts.lua`，不手改生成副本 |
| 挂载数据 | `src/research/build_runtime_mount_data.py`及其导入模块生成核心表和`mount_assets.lua`；资源打包由`src/build_mount_assets.py`核对来源和资格 |
| 完整游戏构建 | `src/build.py`先生成文字，再冻结源码并成套构建核心/UI/Lua和自有PAK |
| 现有检查 | `tests/run.py --build-dir`，覆盖和条件见[检查说明](../../tests/README.md)；构建不等于已运行此入口 |
| 唯一封装入口 | `src/package_delivery.py`；`src/package-release.py`仅是它的薄包装 |
| 双语EXE编译 | `src/delivery/build_setup.ps1 -InputDirectory`，消费封装目录内冻结的C#、资源和编译参数 |
| 历史工具 | `src/install.py`保留旧手动安装/预检实现；`src/release_package.py`只留旧helper供历史恢复，旧GameFiles命令行入口已停用 |
| 本机取证 | `启动游戏并取证.cmd` → `src/research/start_crash_capture.ps1`；按需使用，原始转储留Temp |

`dist/DualSP/bin/DualSP.dll`是受保护旧核心，不是新构建输出位置。其保护摘要见[AGENTS](../../AGENTS.md)；构建还核对`cores/1.0.1`、`cores/1.1.0`、`cores/2.0`三份旧DLL，依赖原位保留。其他旧核心的恢复条件见[历史核心](../archive/README.md#历史核心与发布包)。

## 文档迁移后的工具边界

游戏与安装器已接入`docs/user/`唯一文字源，构建冻结不再读取已退役的`docs/授权.txt`。`src/user_texts.py`同时校对中英文键、格式占位符、列表NUL、ImGui标识和原始消息匹配映射；`--check`只核对生成结果是否过期，`src/build.py`会自动生成。正文的展示分工与维护方式见[玩家文档维护](../project/player-docs.md)，不能为兼容旧脚本重新建立第二份正文。

游戏源码冻结由`src/release_tools.py`的`source_snapshot()`定义，包含产品C++/头文件、生成头文件、vendor、产品清单内Lua、构建工具、研究来源和证据，以及四对玩家TXT、游戏JSON与文字生成器。它不包含安装器C#和安装器JSON；安装器与封装来源单独冻结，因此只修安装器文字或C#不需要冒充游戏功能重建。

`src/package_delivery.py`核对完整游戏构建与文字生成结果，再冻结封装脚本、`src/delivery/`及`docs/user/`输入，生成通用ZIP、EXE资源和冻结C#副本。EXE编译入口只消费该次封装目录，不从正在编辑的工作树重新取C#。旧GameFiles发布入口、标识替换工具和旧文档路径仅用于恢复历史，不再决定新交付的格式。

首次目录迁移见[归档回执](../../evidence/artifact-archive-20261023.json)。旧回执、正式2.24.1包和旧冻结来源保持当时含义，不因本轮文字或工具改造而改写。

## 构建与封装依赖

alpha2开发故障包使用`src/release_workflow.py build --development-fault-tests`；此标志传入完整构建并写入构建和封装回执。常规包不带该标志；正式版candidate=false时构建拒绝该标志。入口、恢复和执行边界统一见[故障测试指导](aircraft-fault-testing.md)，不将开发配置加入普通玩家设置。

- 本机工具根为`E:/AC7_MultiSP/tools`，产品清单指定`llvm-mingw-20261006-ucrt-x86_64`；构建使用其中lupa/pefile及仓库vendor。使用前核对实际文件，不把旧版本记录当现场检查。
- `src/build.py`的`--mount-assets-dir`为必填。main基线为77资源/154成员；本分支外挂机体候选需92资源/184成员，已准备于`C:/Users/83717/.codex/worktrees/base-dlc-multi-sp/AC7_MultiSP/Temp/tasks/20261028-base-dlc-multi-sp/mount-assets-92/source`，同级上层`input-manifest.json`记录摘要。后续构建只能使用与分支白名单一致的整套输入，不沿用旧54/77资源目录。位置和保留条件见[本机资产索引](../archive/local-assets.md)，数量相同不能替代摘要。本轮未编译或打包。
- 本批离线资源可由`src/research/prepare_remaining_external_mounts.py`重现：显式传`--source-manifest`、`--tools-root`、任务Temp内新目录`--output-root`，以及新文件`--receipt`、`--audit`；资源与来源记录均拒绝覆盖。生成器`build_runtime_mount_data.py`再输出运行表和Lua资源清单；该过程不编译核心、不运行测试或游戏。旧77资源保持原字节，先新增13资源，再由`mgp-single-pod-20261028.json`追加2份单MGP挂架，均经编码/解码、cooked引用和碰撞载荷静态核对。单MGP配方保存在该记录的`resource_reports[].recipe`及任务Temp的`mgp-single-pod-resources/recipes.json`，使用同一`split_static_mount_asset.py`和原`native-source`输入重现，不改旧回执。
- 独立工作树可用`--temp-root E:/AC7_MultiSP/Temp`，同时把`--output-dir`指定到该根内全新的任务子目录。构建拒绝覆盖旧输出，编译中间文件也留在本次目录；失败后保留现场，修正后使用新目录。
- 封装显式提供`--build-dir`、全新`--output-dir`、`--icon-source 本机游戏ICO`及`--audio-source E:/AC7_MultiSP/dist/DualSP/Audio/SP2.wav`。ICO原料及冻结规则见[交付方案](../project/delivery.md#安装器游戏图标)，不将游戏资源提交Git。提示音核对原验收摘要，不使用个人替换音频分发；新封装不依赖旧手装ZIP。
- EXE使用系统64位.NET Framework编译器。`build_setup.ps1`先核对冻结C#/manifest、资源、响应文件及独立ZIP摘要，编译后再复核输入，单独写`EXE编译回执.json`；编译成功不代表安装器已运行。

表、Lua清单和PAK必须成套。`src/build_mount_assets.py`继续核对Socket引用、七机碰撞保真、F/A-18E反馈和TGM r2；当前TGM资源为`MSP_f18etg_gpb_inner_whole_r2`。依据见[Socket资格](../../evidence/mount-socket-reference-audit-20261021.json)和[TGM r2](../../evidence/f18etg-whole-rack-socket-r2.json)，不能混用54/65/74/77资源阶段的整表、编号或旧资源。

Git不含ignored原料、音频、工具、原版PAK或冻结产物。接收时分别核对源码差异、证据、生成结果和本机输入；不能只恢复Git文本就称完整恢复，也不能修改旧摘要让冻结通过。

## 验证与安装保护

游戏构建、既有检查、ZIP成员回读、EXE编译、实际安装、用户实测和正式发布分别记录。构建按当次正式或候选授权执行；完整构建会做Lua语法和产物/来源核对，封装会回读ZIP，但这些不等于运行`tests/run.py`或游戏实测。新候选的双语显示、安装事务和实际部署边界见[交付方案](../project/delivery.md#待验证范围)。

当前EXE的旧安装识别、文件备份、管理器托管拒绝和失败恢复在`src/delivery/setup/Bundle.cs`维护；通用ZIP的Vortex部署映射由封装器逐文件生成。玩家操作只维护[中英文安装说明](../project/player-docs.md)，本页不另写安装步骤。

历史`src/install.py`的完整runtime_files基线、`--reset-config`、`--verify-other-mods`及其回执字段仍按原实现解释，不作为新EXE接口。历史Vortex链接物化安装见[预检](../../evidence/2.24.0-alpha1-install-preflight.json)和[完成回执](../../evidence/2.24.0-alpha1-install-completion.json)；它只证明当时处理，不替代新候选验收，也不授权跟随链接覆盖Vortex源包。

有效配置19保留；旧格式/损坏JSON先按原字节备份，再生成新版默认值，当前配置内部无效设置仍报错。EXE备份个人文件不等于游戏已完成配置读取或迁移，不能把封装内的来源回执当作实装证明。

## 恢复与取证

恢复未采用研究先读[研究索引](../research/README.md)、[历史归档](../archive/README.md)和[本机资产索引](../archive/local-assets.md)。从main建立新研究分支后提取所需旧差异，不能用旧核心、Lua、资源和文档整套覆盖当前产品。Git bundle、未提交ZIP与ignored原料分别承担不同恢复范围；工作树锁定也不等于文件只读。

整理前正文保存在`archive/20261023/pre-governance-main`，旧文中的“当前”、研究号与路径属于当时阶段。旧产物重放需先恢复其指定核心和原料，不直接交给新交付入口假装兼容。

本机取证入口为单实例：已有取证工作进程或本工具的调试器时，弹窗报错并停止启动，不复用旧会话；两个入口同时启动也由启动锁阻止。残留进程需先确认不再调试运行中的游戏，再处理。取证修订2使用硬件退出/消息框断点、独立输出和产物核验，仍有附加前窗口、非Win32界面、外部终止等边界。调试器可能改变时序，不用于无干预帧率对照，也不保证每次都能捕获根因。详见[取证改进](../../evidence/crash-capture-hardening-2.20.2.json)；转储留Temp，不入发布包。

## 仍未解决或未覆盖的事项

| 事项 | 边界与继续入口 |
| --- | --- |
| 新候选交付 | 构建与封装核对不代替游戏实测；双语界面、ZIP部署、EXE安装/更新/卸载及异常恢复分别验证，见[交付方案](../project/delivery.md) |
| 线上更新 | 本轮未上传或核查线上清单及下载文件；[Pages与R2旧说明](../archive/delivery/Pages与R2发布更新说明.md)仅作历史参考，不代表新包已上线 |
| CFA切换尖峰与偶发视觉 | 用户确认明显减轻，仍有尖峰；ECM阶段未细分到单函数，偶发EML蓝光根因未证实，见[工程经验](engineering-lessons.md)和[挂载设计](multi-sp-mount-design.md) |
| 选装进入SP1/光标、结算箭头及剩余切换成本 | 结算轻卡及选装专项保留；CFA EML持续掉帧与F-2A普通飞行已获2.26.1有效反馈，模型开时CFA切换短降帧可接受；不扩大为全场景性能问题解决，见[性能验收](../research/runtime-performance.md#alpha3验收与正式2261) |
| MiG-31B r3及动态净空 | 整体布局获认可，后腹SAAM位移、支撑及机腹净空仍需观察；离线包络不替代动态验收 |
| F-14IA原生异步依赖 | 当前提前退回原生并保留本机隐藏隔离，不代表修好第三方蓝图加载顺序，见[安全退让](../../evidence/special-aircraft-native-fallback-20261022.json) |
| PW旧退出、CFA暂停异常 | PW首轮根因未明；CFA对象创建跳板异常不能与ADMM队列或最终退出断点强判同因，见[PW记录](../../evidence/pwmki-sortie-crash-2.20.1.json)、[CFA记录](../../evidence/cfa44-admm-debug-pause-crash-20261021.json) |
| 未采用成果 | 完整内舱、ASF-X/RKT、旧updater、FOV及未提交研究继续保留，见[研究索引](../research/README.md) |

ADMM无限队列边界、停用Bomb误拦截、内存交接可靠性及暂停换机接续已接收；故障现场与适用限制留在工程经验，不扩大为全部机型、所有Mod或固定帧率保证。
