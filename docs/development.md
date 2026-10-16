# 开发与交付

工作区固定为 `E:/AC7_MultiSP`，主线 `main`。功能基线、待整合分支及安装记录见[当前状态](status.md)。
本次整理不授权编译、测试、部署或游戏操作；下列命令仅供后续有授权的任务使用。

## 当前唯一入口

| 用途 | 入口与边界 |
|---|---|
| 完整候选构建 | `src/research/build_su30sm_candidate.py`；必须包含固定挂载宏，输出全新 Temp 子目录 |
| 本机受控升级 | `src/research/install_su30sm_three_sp.py`；默认只预检，显式 --apply 才安装，仅支持已登记相邻版本 |
| 正式包生成 | `src/package-release.py`；绑定 1.1.0 来源、回执和 DLL 摘要，不能作为任意版本通用打包器 |
| 用户安装 | [安装说明](AC7DualSP-安装说明.txt)；使用成套发布载荷 |

完整构建示例（不在本轮执行）：

```powershell
python -X utf8 src/research/build_su30sm_candidate.py --tools-root E:/AC7_MultiSP/tools --output-dir Temp/build-next
```

输出目录必须不存在。脚本会构建核心/UI、核对 Lua 语法和导出，并记录来源；这些核验不等于运行测试或用户实玩。

`src/build.cmd`、`src/build-core-portable.cmd` 不包含当前正式挂载构建条件，不能用于生成 1.1.0 成套核心。
`src/install-resupply-candidate.py --experience` 使用旧核心；旧 three-sp 准备器和单机型安装器也不是当前部署入口。
文件暂留供历史恢复或内部依赖，不因存在就说明可执行。功能工具本轮未重构或改名。

## 发布与依赖

- Lua 唯一源：`dist/DualSP/Scripts/`，共十个脚本，包含 loadout_profile.lua。
- 当前核心：`dist/DualSP/cores/1.1.0/DualSP.dll`；UI：`dist/DualSP/bin/DualSPUI.dll`。
- `dist/DualSP/bin/DualSP.dll` 是受保护旧核心，不是当前运行核心；保护摘要见 AGENTS。
- 加载器和许可证从 `payload/` 显式取用，不采集其旧模组副本。
- `tools/`、原音频、运行库和 ZIP 不入 Git；依赖位置见[工具清单](../tools/README.md)。
- 发布器核对 [1.1.0 构建回执](../evidence/1.1.0-build.json)、来源摘要、DLL 和微软签名，再逐成员回读 ZIP。已有包不覆盖。
- 主工作区的 1.1.0 ZIP 已按[原发布回执](../evidence/1.1.0-release.json)原样复制。当前说明的编辑不改变该冻结包；重新打包须另有明确任务。

旧发布工作区保留在 `Temp/su30sm-fixed-mount-prototype`，其中构建目录、运行库和采集资料仅作历史恢复来源。不要回到那里开发，也不要自动运行旧采集脚本。

## 安装边界

本机升级器依赖旧回执和 `install_su30sm_candidate.py` 的辅助函数。1.1.0 路径仍会读取 1.0.2 安装回执、固定挂载候选 .2 回执及 .11 安装回执；这些不是可随意移动的文档垃圾。

仅在有授权的部署任务中核对实际安装版本、进程、来源和配置，再做预检。关闭游戏、完整备份、暂存与摘要核验通过后才允许安装。原配置、存档及其他模组不应被计划外改变。
最后安装记录不是实时事实，不能据旧 PID 或旧回执跳过现场核对。本轮未读取游戏现场。

## 已知工程债务

1. `--reuse-core-build` 只核对两个核心源码文件，构建回执也没有覆盖全部头文件、第三方源和编译条件。暂不把它当作“完整来源一致”的证明；完善依赖清单前，新功能改动应在有授权时完整构建。
2. 正式流程仍用研究名称与宏，普通入口容易误用。下一轮可统一入口，保留现有强校验；此次只明确导航，不改行为。
3. `tests/` 主要对应旧双 SP 协议，不能宣称覆盖 1.1.0；未经用户要求不新增或运行。
4. 核心/主调度/页面适配器职责集中，后续按模块所有权逐步拆分，不能与本轮整理混做。

## 文档和证据

当前事实在对应文档原位更新；不再不断追加“后来修正”。实测、离线核验、未验证分开记录。
旧叙事位于 `docs/archive/`；原始回执保留路径和字节，防止破坏工具依赖及发布凭据。默认检索排除历史目录；必要时按[证据索引](../evidence/README.md)定向读取。
