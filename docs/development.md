# 开发与发布

> 当前版本为 [1.0.1](version-1.0.1.md)。--experience 从对应 cores 目录安装，要求核心/UI 的 dualsp_runtime_gate 导出及第九个 Lua runtime_context.lua。本轮不制作 ZIP；打包器仍固定 1.0.0，会拒绝当前 VERSION。

返回[项目入口](../AGENTS.md)。当前版本是 **AC7DualSP 1.0.1**，本轮研究见[候选记录](version-1.0.1-candidate.1.md)，更早记录见[历史索引](history-1.0.0.md)。

## 当前组成

1.0.1 使用 `dist/DualSP/cores/1.0.1/DualSP.dll`，SHA256 为 `EA3C9C17456EAE89289621E40225F7D4CC2F192AE051E97725FEBFDDB9BA27AF`，与实测候选一致。正式整理只因标题变化重编译 UI；武器核心和 Lua 功能逻辑不变。0.1.9 核心保留供 1.0.0 历史版本使用。

内部名称 DualSP 不变；配置 schema=1、management_revision=2，管理协议为 2。VERSION、加载标记、窗口标题与当前说明统一为 1.0.1，无新发布包。不要把旧 `dist/DualSP/bin/DualSP.dll` 当作当前核心。

## 开发入口

| 入口 | 用途 |
|---|---|
| `src/build-ui.cmd` | 编译当前 UI；输出 dist/DualSP/bin/DualSPUI.dll |
| `src/build-core-portable.cmd` | 独立候选核心输出到 build-core，不覆盖旧正式核心 |
| `src/build.cmd` | 两种构建入口；只需改 UI 时不要调用它 |
| `src/install-resupply-candidate.py --experience` | 现有本机安装受控升级，支持 1.0.1；保留历史脚本名 |
| `src/package-release.py --redist-root <_CommonRedist路径>` | 仅保留的 1.0.0 打包入口，本轮不运行，不能直接生成 1.0.1 |
| `src/inspect_*` | 用户自有资源的研究工具，原始资源和中间结果放 Temp |
| `tests/` | 历史回归模块，非自动流程；未经用户要求不运行或新增 |

LLVM-MinGW 默认位置见[工具清单](../tools/README.md)，可以通过 DUALSP_TOOLCHAIN 指定。发布依赖从本机 Steamworks Shared 的 _CommonRedist 读取；工具本身不随成品发布。SP2.wav 按 [Audio 说明](../dist/DualSP/Audio/README.md)准备，缺失即停止。

## 部署与打包

用户安装以 [TXT](AC7DualSP-安装说明.txt) 的手动复制为准，不需要开发环境。根目录旧 Install/Uninstall 入口、旧 package/prepare-payload/update-local 脚本已退役，恢复旧版请用保留 ZIP 或历史 Git 快照，不能运行旧脚本安装当前源码。

本机开发部署继续核对游戏 EXE、Lua 语法、PE 导出、专用加载器 Lua 转发、原生 UI 名称及保护文件；整目录备份后安装，回读并核对配置、存档和旧恢复载荷。语法编译不执行 Lua，也不运行测试模块。

发布器读取显式运行文件、专用加载器、原始许可证、用户音频及微软运行库；绝不收集游戏目录中的日志、存档和配置。ZIP 内的文件清单覆盖所有载荷，清单本身不包含自身哈希；发布回执另记 ZIP 哈希及全部成员。现有同名 ZIP 不覆盖，以便保留已交付快照。

微软运行库来源：
- [VC++ x64 官方说明](https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist?view=msvc-170)
- [DirectX June 2010 官方说明](https://www.microsoft.com/en-us/download/details.aspx?id=8109)

1.0.0 包含本机 Steam 下载的 VC++ 2022 x64 安装程序，以及完整 DirectX Jun2010 CAB/安装程序集合（不含 Steam installscript.vdf）。安装程序签名在打包时核对为 Microsoft Corporation 且有效。不要从系统目录拼凑第三方 DLL。

## Git 与保护

当前源码、版本 DLL、加载器、第三方许可、研究结论和发布回执进入 Git。Release ZIP、运行库、原始音频、Temp、backup 和个人状态不入 Git；不自动推送。旧 0.1.7 ZIP 及 baseline 清单保持原值。

1.0.0 的版本号不是全面稳定性保证。用户复测、离线核验和未覆盖场景分别记录，不把语法检查等同实机测试。菜单操作授权不包含代理出击或发射武器。
## 管理参数第 2 版部署

本轮仅需重编译管理 UI，候选武器核心不变。Lua/UI 必须成套部署，management_protocol=2；运行时首次迁移 profiles.json 后管理参数 revision=2。安装器仍在启动前验证原配置未变，启动后的预期参数迁移须另行回读并核对机体方案未变，不能把两者混为同一核验。不得用旧 UI 搭配新的设置请求字段。

2026-10-15 飞控兼容候选：管理状态新增 `management_key`，UI/Lua 必须成套更新。本机设为 F6，默认仍为 F8，详情见[管理说明](mod-management.md)。本轮仅构建 UI，旧受保护核心与当前武器核心不变，不新增或运行测试模块。飞控按作者载荷清单复制 AC7MouseAim 目录，合并启用行，共享加载器不覆盖；保留比例制导时不安装 AC7Weapons。安装和备份见[飞控批次回执](../evidence/compatibility-flight-batch-installed.json)。
