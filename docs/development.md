# 开发与发布

返回[项目入口](../AGENTS.md)。当前版本是 **AC7DualSP 1.0.0**，旧候选过程见[历史索引](history-1.0.0.md)。

## 当前组成

1.0.0 复用 `dist/DualSP/cores/0.1.9/DualSP.dll`，SHA256 为 `46731AE6AD519E0BD145E0EA03C32A2662947C8B03B56C524D7CDF46CA8096E1`。本轮仅因可见标题更名而重新编译 UI；武器核心不重编译，Lua 逻辑沿用用户确认的性能候选。

内部 DualSP 名称、导出、配置格式保持不变。VERSION、main.lua 加载标记、管理窗口标题、发布包和说明使用 1.0.0。不要把旧 `dist/DualSP/bin/DualSP.dll` 当作当前核心。

## 开发入口

| 入口 | 用途 |
|---|---|
| `src/build-ui.cmd` | 编译当前 UI；输出 dist/DualSP/bin/DualSPUI.dll |
| `src/build-core-portable.cmd` | 独立候选核心输出到 build-core，不覆盖旧正式核心 |
| `src/build.cmd` | 两种构建入口；只需改 UI 时不要调用它 |
| `src/install-resupply-candidate.py --experience` | 已有本机安装的受控升级，支持当前 1.0.0；历史文件名保留，避免重写已验证部署流程 |
| `src/package-release.py --redist-root <_CommonRedist路径>` | 按显式文件清单打包，不调用旧 payload 同步器；校验后输出独立 ZIP |
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
