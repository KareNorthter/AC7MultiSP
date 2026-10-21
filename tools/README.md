# 本机工具清单

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
