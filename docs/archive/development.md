> 历史归档：保留当时结论，不是当前操作指引。当前基线见[项目状态](../status.md)。

# 1.1.0 开发与本地发布

当前工作区/边界见 [AGENTS](../../AGENTS.md)，后续任务见[交接](research/NEXT-STAGE-HANDOFF.txt)。下方旧开发正文保留历史，不能照旧选用 1.0.1 核心。

- 构建：`python -X utf8 src/research/build_su30sm_candidate.py --tools-root E:/AC7_MultiSP/tools --output-dir Temp/<全新目录>`。固定挂载宏 `DUALSP_SU30SM_MOUNT_PROTOTYPE` 仍必须启用；不新增/运行测试模块。
- 版本整理：加 `--reuse-core-build Temp/static-mount-batch11-final-build`，先核对核心源码与旧回执，再原样复用核心，只重建 UI；核心源码变动会拒绝复用。
- 当前发布核心 `dist/DualSP/cores/1.1.0/DualSP.dll`、UI `dist/DualSP/bin/DualSPUI.dll`，十个 Lua 源包含 `loadout_profile.lua`。UI 和 Lua 的版本标识统一 1.1.0；保护旧 bin/DualSP.dll 不变。
- 打包：`python -X utf8 src/package-release.py --redist-root Temp/release-1.1.0-redist`，先核对 evidence/1.1.0-build.json 冻结来源和 DLL，再核对音频/签名，生成新 ZIP 并逐成员回读。指定目录是从核验过的 1.0.2 包恢复的原 Steam 离线运行库，不是重新下载或拼凑系统 DLL。
- 受控部署：`python -X utf8 src/research/install_su30sm_three_sp.py --build-dir Temp/release-1.1.0-build` 默认只预检；关闭游戏后显式 `--apply` 才安装，完整备份 .11、配置、存档，正式回执 evidence/1.1.0-installed.json。本轮未部署，游戏仍 .11。
- 旧 `install-resupply-candidate.py --experience` 不支持本正式核心，禁止用于 1.1.0。用户手动安装依照包内 TXT，不需要开发工具。

源码与必要 DLL/回执入 Git；本地 ZIP、旧包、原始音频/游戏资源、个人状态不入 Git。旧包不覆盖，不自动推送或公开上传。

以下为 1.0.2 及更早历史：

# 开发与发布

当前独立研究分支安装的是 `1.0.2-su30sm-three-sp.1`，未发布。使用 `src/research/build_su30sm_candidate.py` 构建核心和协议 3 UI，使用 `src/research/install_su30sm_three_sp.py` 成套核对、备份和安装；核心快照在 `dist/DualSP/cores/1.0.2-su30sm-three-sp.1/`。禁止用下文正式打包器或原三 SP 候选安装入口处理本集成版本。旧 `bin/DualSP.dll` 仍受保护。来源、安装和回退见[集成说明](research/su30sm-three-sp-fixed-mount.txt)；仅构建、语法和文件核验通过，三挂未实玩。下文为正式 1.0.2 的历史开发说明。

> 当前正式版本为 [1.0.2](version-1.0.2.txt)。--experience 明确复用 cores/1.0.1 核心，要求核心/UI 的 dualsp_runtime_gate 导出及 runtime_context.lua。用户复测正常，已制作本地 ZIP；打包器已更新为 1.0.2，包含 runtime_context.lua 与版本说明。

返回[项目入口](../../AGENTS.md)。当前版本是 **AC7DualSP 1.0.2**，本轮验证见[兼容摘要](version-1.0.2.txt)，更早记录见[历史索引](history-1.0.0.md)。

## 当前组成

1.0.2 同版本发布修订：默认管理键为 F6，已有配置的显式管理键保留；Lua/UI 同步调整，UI 重新构建。旧发布 ZIP 移至 backup 留存，初始回执保留为 evidence/1.0.2-release-initial.json，最新包仍名 AC7DualSP-1.0.2.zip。本轮不重新部署游戏，安装回执和最终实玩记录描述修订前的已测载荷；新包校验单独记录。

1.0.2 使用 `dist/DualSP/cores/1.0.1/DualSP.dll`，SHA256 为 `EA3C9C17456EAE89289621E40225F7D4CC2F192AE051E97725FEBFDDB9BA27AF`。兼容修订在 Lua/UI，武器核心不变；1.0.2 整理因标题变化重编译仓库 UI。异步换机等待及视觉连续性修改已部署，用户复测正常，日志确认等待分支触发并恢复。0.1.9 核心保留供 1.0.0 历史版本使用。

内部名称 DualSP 不变；配置 schema=1、management_revision=2，管理协议为 2。仓库、本机安装及 ZIP 的 VERSION、加载标记、窗口标题统一为 1.0.2。安装回执见 evidence/1.0.2-installed.json，发布回执见 evidence/1.0.2-release.json。不要把旧 `dist/DualSP/bin/DualSP.dll` 当作当前核心。

## 开发入口

| 入口 | 用途 |
|---|---|
| `src/build-ui.cmd` | 编译当前 UI；输出 dist/DualSP/bin/DualSPUI.dll |
| `src/build-core-portable.cmd` | 独立候选核心输出到 build-core，不覆盖旧正式核心 |
| `src/build.cmd` | 两种构建入口；只需改 UI 时不要调用它 |
| `src/install-resupply-candidate.py --experience` | 现有本机安装受控升级，支持 1.0.2；保留历史脚本名 |
| `src/package-release.py --redist-root <_CommonRedist路径>` | 当前 1.0.2 打包入口，显式选择实测核心及完整脚本，不覆盖旧包 |
| `src/inspect_*` | 用户自有资源的研究工具，原始资源和中间结果放 Temp |
| `src/build_local_compatibility.py` | 按明确机体清单筛选作者 PPDT 并生成仅供本机使用的目录 PAK，不自动安装 |
| `tests/` | 历史回归模块，非自动流程；未经用户要求不运行或新增 |

LLVM-MinGW 默认位置见[工具清单](../../tools/README.md)，可以通过 DUALSP_TOOLCHAIN 指定。发布依赖从本机 Steamworks Shared 的 _CommonRedist 读取；工具本身不随成品发布。SP2.wav 按 [Audio 说明](../../dist/DualSP/Audio/README.md)准备，缺失即停止。

## 部署与打包

用户安装以 [TXT](AC7DualSP-安装说明.txt) 的手动复制为准，不需要开发环境。根目录旧 Install/Uninstall 入口、旧 package/prepare-payload/update-local 脚本已退役，恢复旧版请用保留 ZIP 或历史 Git 快照，不能运行旧脚本安装当前源码。

本机开发部署继续核对游戏 EXE、Lua 语法、PE 导出、专用加载器 Lua 转发、原生 UI 名称及保护文件；整目录备份后安装，回读并核对配置、存档和旧恢复载荷。语法编译不执行 Lua，也不运行测试模块。

发布器读取显式运行文件、专用加载器、原始许可证、用户音频及微软运行库；绝不收集游戏目录中的日志、存档和配置。ZIP 内的文件清单覆盖所有载荷，清单本身不包含自身哈希；发布回执另记 ZIP 哈希及全部成员。现有同名 ZIP 不覆盖，以便保留已交付快照。

微软运行库来源：
- [VC++ x64 官方说明](https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist?view=msvc-170)
- [DirectX June 2010 官方说明](https://www.microsoft.com/en-us/download/details.aspx?id=8109)

1.0.0 包含本机 Steam 下载的 VC++ 2022 x64 安装程序，以及完整 DirectX Jun2010 CAB/安装程序集合（不含 Steam installscript.vdf）。安装程序签名在打包时核对为 Microsoft Corporation 且有效。不要从系统目录拼凑第三方 DLL。

## Git 与保护

固定挂载研究分支使用专用构建：`python -X utf8 src/research/build_su30sm_candidate.py --tools-root E:/AC7_MultiSP/tools --output-dir Temp/su30sm-candidate.2-build`。输出目录必须不存在；构建启用 `DUALSP_SU30SM_MOUNT_PROTOTYPE`，语法编译 Lua 并检查核心导出，不运行脚本、不加载 DLL、不安装。研究 Lua 新增必需接口，不能用普通核心构建或正式 DLL 搭配。专用安装器 `src/research/install_su30sm_candidate.py --build-dir Temp/su30sm-candidate.2-build` 默认只预检，显式加 `--apply` 才执行；它只接受已核对的正式 1.0.2 现场，拒绝覆盖已经安装的研究候选。候选 .2 已备份并安装、待实机观察，见[研究与恢复说明](research/su30sm-fixed-mount.md)。该段只适用于独立研究分支，不改变主线构建方式。

当前源码、版本 DLL、加载器、第三方许可、研究结论和发布回执进入 Git。Release ZIP、运行库、原始音频、Temp、backup 和个人状态不入 Git；不自动推送。旧 0.1.7 ZIP 及 baseline 清单保持原值。

1.0.0 的版本号不是全面稳定性保证。用户复测、离线核验和未覆盖场景分别记录，不把语法检查等同实机测试。菜单操作授权不包含代理出击或发射武器。
## 管理参数第 2 版部署

本轮仅需重编译管理 UI，候选武器核心不变。Lua/UI 必须成套部署，management_protocol=2；运行时首次迁移 profiles.json 后管理参数 revision=2。安装器仍在启动前验证原配置未变，启动后的预期参数迁移须另行回读并核对机体方案未变，不能把两者混为同一核验。不得用旧 UI 搭配新的设置请求字段。

2026-10-15 飞控兼容候选：管理状态新增 `management_key`，UI/Lua 必须成套更新。本机设为 F6，默认仍为 F8，详情见[管理说明](mod-management.md)。本轮仅构建 UI，旧受保护核心与当前武器核心不变，不新增或运行测试模块。飞控按作者载荷清单复制 AC7MouseAim 目录，合并启用行，共享加载器不覆盖；保留比例制导时不安装 AC7Weapons。安装和备份见[飞控批次回执](../../evidence/compatibility-flight-batch-installed.json)。

## 本地新增机体目录（2026-10-15）

`build_local_compatibility.py` 输入作者 PPDT 解码 JSON、显式编号/代码清单、全新输出目录、UAssetGUI 和 .NET 路径。当前输入及中间资源均在 `Temp/compatibility-20261015`；清单为 `adler-j20-manifest.json`，输出为 `adler-j20-catalogue-plain`。该工具核对编号与代码、保留完整选中行，用 UAssetGUI 写回再解析比较，生成含 uasset/uexp 的未压缩 UE4 PAK v3，写出构建回执。输入不符或输出目录已存在即停止，不自动猜测、追加或回退。

当前目录仅 47 条原战役记录加 Adler A/B 和 J-20A，总计 50；其余共用武器/名称/皮肤表使用作者原包。原始 52 条中的四条 VR 与 Avril 记录不在作者共用表中，本批不扩展该范围。完整 195 条 PPDT 不安装。输出 PAK 只在用户本地游戏目录使用，不入 Git、不放入 DualSP 发布包、不分发作者资源；外部原包不改字节。具体输入哈希、输出哈希、行集合及安装回执见 [本批回执](../../evidence/compatibility-nexus-samples-installed.json)。

本轮还部署 main.lua 的机体类身份检查修订，语法核对通过，核心/UI 无需编译。安装前备份 DualSP、存档和被替换的 J-20 旧包，安装后核对 16 个 PAK、脚本及未改保护项。游戏未启动，未新增或运行测试模块，兼容性仍待用户实玩。
