# 按需证据

日常状态见 [AGENTS](../AGENTS.md)，无需通读本目录。

- [1.1.0 用户验证](1.1.0-user-validation.json)：实测反馈与稳定性边界。
- [1.1.0 构建](1.1.0-build.json)：来源/DLL/导出/语法核对，不代表实玩。
- [1.1.0 发布](1.1.0-release.json)：原 ZIP 摘要与成员。
- [仓库整理](repository-consolidation-20261016.json)：旧分支/工作区恢复映射；文档数量与路径是上一轮快照。

安装器仍读取 [1.0.2](1.0.2-installed.json)、[固定挂载.2](su30sm-fixed-mount-candidate.2-installed.json)、[三SP.11](su30sm-three-sp-candidate.11-installed.json)等旧回执，研究工具也引用结构数据。它们是机器记录/依赖，本轮不改内容或位置。
旧回执提及的说明可从 Git 历史读取，方法见[开发说明](../docs/development.md)。旧“当前”、PID、安装状态不能当作实时事实。
默认检索排除此目录，需要时按文件读或用 rg --no-ignore 定向查找。
