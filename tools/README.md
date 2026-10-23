# 本机工具与依赖

本目录保存本机研究和构建依赖，Git只跟踪本说明，工具二进制与Python依赖均被忽略。主工作区的实际工具根是`E:/AC7_MultiSP/tools`；新工作树中的同名目录通常只有README，不会自动复制依赖。使用前核对实际文件和来源，不把旧记录中的版本号当作现场检查结果。

| 主工具根下的位置 | 用途与调用关系 |
| --- | --- |
| `llvm-mingw-20261006-ucrt-x86_64/` | 便携UCRT x64 C/C++工具链。`src/build.py`通过`--tools-root`定位，再取`src/product.json`指定的子目录；`tests/run.py`使用构建回执记录的同一工具链 |
| `assets/UAssetGUI.exe` | 解析/序列化自有UE4.18资源副本，脚本CLI参数使用`VER_UE4_18`；由`inspect_resupply_assets.py`和多份`src/research/`脚本调用 |
| `assets/AC7Key.bin`、`assets/AC7Decrypt.cs` | 自有AC7资源副本的离线编解码资料；密钥/原始游戏资源不纳入Git或公开包 |
| `dotnet8/` | 项目本地.NET基础与桌面运行库，供UAssetGUI使用；资源脚本对子进程设置`DOTNET_ROOT_X64`，不修改系统PATH或全局环境变量 |
| `test-python/` | Python依赖，包括lupa的Lua 5.4执行环境及资源研究所需模块；既有Python检查通过显式路径导入 |
| `capstone/`、`pefile.py`、`peutils.py`、`ordlookup/` | 原生反汇编及PE结构读取；构建使用pefile核对x64架构与导出。对应dist-info是本机包元数据 |
| `vgmstream/`、`wwiser.pyz` | 自有Wwise音库的离线解析和本地试听；不随Mod发行，原音库/解码试听留Temp |
| `AC7-Integrated-v0.7.0-beta.1/` | 本机保留的独立旧工具成品，目录内有自身说明；不属于main成套构建的默认依赖，不据名称推定兼容或已接收 |

PowerShell、Python和Git是本机通用环境，不复制进项目。新增工具按项目约定放C盘以外，临时产物在`Temp/`；工具缺失时先报告，不临时换来源、全局安装或重复试错。资源原料、工具缓存和旧构建的保留/恢复位置见[工作区索引](../docs/project/workspaces.md)和[研究索引](../docs/research/README.md)。

## 来源与离线使用

- 工具链来源：[LLVM-MinGW 20261006](https://github.com/mstorsjo/llvm-mingw/releases/tag/20261006)。
- 资源工具来源：[UAssetGUI](https://github.com/atenfyr/UAssetGUI)、[UAssetAPI的AC7编解码实现](https://github.com/atenfyr/UAssetAPI/blob/master/UAssetAPI/AC7Decrypt.cs)。本机.NET原下载校验依据为[微软.NET 8发行元数据](https://builds.dotnet.microsoft.com/dotnet/release-metadata/8.0/releases.json)；基础与桌面Runtime分别核验后共同解压，不能只保留桌面组件。
- 音频工具来源：[vgmstream r2117](https://github.com/vgmstream/vgmstream/releases/tag/r2117)、[wwiser v20260808](https://github.com/bnnm/wwiser/releases/tag/v20260808)。这些是历史下载来源，不表示已重新检查上游最新版本。
- 项目加载器来源：[Playmore的AC7专用UE4SS](https://github.com/Playmore-ModdingConcepts/PM-UE4SS/releases/tag/UE4SS)，其载荷和许可证保留在`payload/ue4ss/`；它不等于本目录下可随意替换的通用UE4SS。

构建/资源来源的文件依赖见[开发与恢复](../docs/engineering/development.md)，检查覆盖见[检查说明](../tests/README.md)。当前版本和安装记录只在[项目入口](../README.md)维护，工作规范见[开发规范](../docs/project/workflow.md)。
