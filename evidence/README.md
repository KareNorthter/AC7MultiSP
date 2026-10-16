# 按需证据

日常状态见 [AGENTS](../AGENTS.md)，无需通读本目录。

- 当前2.0.0：[构建](2.0.0-build.json)、[检查](2.0.0-tests.json)、[安装](2.0.0-installed.json)；自动检查和安装核验通过，用户已确认[实测正常](2.0.0-user-validation.json)。安装时未自动启动游戏。
- 已验证2.0：[构建](2.0-build.json)、[检查](2.0-tests.json)、[安装](2.0-installed.json)；用户已确认[实测正常](2.0-user-validation.json)，已合入主线。
- [1.1.0 用户验证](1.1.0-user-validation.json)：实测反馈与稳定性边界。
- [1.1.0 构建](1.1.0-build.json)：来源/DLL/导出/语法核对，不代表实玩。
- [1.1.0 发布](1.1.0-release.json)：原 ZIP 摘要与成员。
- [仓库整理](repository-consolidation-20261016.json)：旧分支/工作区恢复映射；文档数量与路径是上一轮快照。

当前安装器默认仅以 [2.0](2.0-installed.json) 完整运行文件作为2.0.0升级基线，也可明确指定新的完整基线回执；不再读取无关1.0.2/.2回执。旧机器记录保留原字节，研究工具需要时定向读取。
旧回执提及的说明可从 Git 历史读取，方法见[开发说明](../docs/development.md)。旧“当前”、PID、安装状态不能当作实时事实。
默认检索排除此目录，需要时按文件读或用 rg --no-ignore 定向查找。
