# 本机工具清单

返回[项目入口](../AGENTS.md)，操作入口见[开发说明](../docs/development.md)。工具二进制与 Python 依赖被 Git 排除，保留在本机供用户明确要求的构建和研究使用。

| 位置 | 用途 |
|---|---|
| `llvm-mingw-20261006-ucrt-x86_64/` | clang 23.1.3、UCRT x64 便携 C/C++ 工具链，原安装位置已迁移到本目录 |
| `assets/UAssetGUI.exe` | UAssetGUI 1.1.0，读取 UE4.18 资源副本；CLI 版本参数使用 `VER_UE4_18` |
| `assets/AC7Key.bin`、`AC7Decrypt.cs` | 自有 AC7 资源副本的离线解码资料 |
| `capstone/`、`pefile.py` 等 | 原生反汇编和 PE 信息读取依赖 |
| `test-python/` | 既有 Lua 模拟检查及资源解析所需 Python 依赖 |

PowerShell 7、Python 3.13 和 Git 属于本机通用环境，不复制进项目。新建工具或依赖继续安装在 C 盘以外，临时文件使用 `Temp/`。

来源：[LLVM-MinGW](https://github.com/mstorsjo/llvm-mingw/releases/tag/20261006)、[UAssetGUI](https://github.com/atenfyr/UAssetGUI)、[UAssetAPI 解码实现](https://github.com/atenfyr/UAssetAPI/blob/master/UAssetAPI/AC7Decrypt.cs)。项目安装载荷使用 [Playmore 的 AC7 专用 UE4SS](https://github.com/Playmore-ModdingConcepts/PM-UE4SS/releases/tag/UE4SS)。许可证保留在各依赖及 `payload/ue4ss/Mods/DualSP/licenses/` 中。
