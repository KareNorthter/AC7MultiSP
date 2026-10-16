# AC7DualSP 项目入口

当前本地发布版本 **1.1.0**，基于用户验证通过的 `1.0.2-su30sm-three-sp.11`。保留普通导弹，支持 1 至 3 SP、固定挂载与任务内调试。Steam build 9855922；单人战役/自由任务是目标。

## 工作区和交接

唯一写入工作区 `E:/AC7_MultiSP/Temp/su30sm-fixed-mount-prototype`，分支 `codex/static-mount-batch`。主工作区 `E:/AC7_MultiSP` 只读，不切分支、不覆盖文件。
新对话先读[开发交接](docs/research/NEXT-STAGE-HANDOFF.txt)、[SP 挂载通用经验](docs/research/SP-MOUNT-GUIDE.txt)、[版本说明](docs/release-1.1.0.txt)。旧 [STAGE-HANDOFF](docs/research/STAGE-HANDOFF.txt) 保留候选时间线，新的交接优先。

## 当前完成与验证

- 原生出击/补给选 1 至 3 SP，部件后按实际数量均分，独立弹药/冷却；键盘 1/2/3/4、手柄左/上/右/下对应 MSL/SP1/SP2/SP3。
- 原版 Su-30SM、F-35C、F-15E 已适配多 SP 模型共存及离架。固定挂载以美观为主，保留真实发射空位、弹药、装填和性能；不能把当前三架结果推广全部机型。
- 用户本轮确认 `.11` 暂停、管理、实时换装、F-35C 各组合和动画正常，暂未遇崩溃；Su-30SM/F-15E 的既有反馈见历史证据。每架七组合之外，原生补给、检查点和长时边界仍要有目的覆盖。
- 历史 `.9` crash 和 `.10` 静止后自行退出根因未定位。本轮未复现不等于彻底修复。Steam 错误 84 重试成功另行记录，不直接归因。
- 三栏 UI、PART 弹药预览及中央清晰度已合并；完整绿色/黄色性能条尚未完成，中央移位/边框此前暂缓。
- F6 管理支持纯键盘与任务内 SP 调试；调试不保存个人方案、不覆盖既有检查点、不换飞机；原生暂停归属明确交接。

## 发布与本机

本地包 `Release/AC7DualSP-1.1.0.zip`，不上传/推送，旧 1.0.2 包保留。本次保持当前游戏运行与 `.11` 安装不变；1.1.0 仅变版本标识/UI 标题，核心原样复用。运行版与发布版不能混淆。
见[用户验证](evidence/1.1.0-user-validation.json)、[构建](evidence/1.1.0-build.json)、[发布](evidence/1.1.0-release.json)、[安装说明](docs/AC7DualSP-安装说明.txt)。源码/DLL/说明/精简证据入 Git，ZIP/原始音频/资源/配置/存档不入 Git。
新核心 `dist/DualSP/cores/1.1.0/DualSP.dll`；UI `dist/DualSP/bin/DualSPUI.dll`；Lua 唯一源 `dist/DualSP/Scripts/`，新增 `loadout_profile.lua` 不可漏装。
受保护旧核心 `dist/DualSP/bin/DualSP.dll` 必须保持 SHA256 `D7D7055CA7909021EEACA15E63FF5FF253D2EB7E60A3FE3B79DCEEAB231DE339`。
构建继续用 `src/research/build_su30sm_candidate.py`（含固定挂载宏），打包用 `src/package-release.py`。本机 .11→1.1.0 安装入口为 `src/research/install_su30sm_three_sp.py`，默认预检；不得使用旧 `--experience` 入口装回旧核心。

## 下一阶段

优先补充代表性原生流程/稳定性证据，再完成 F-22A 共用弹仓七组合；之后活动挂架与 PLSL/EML、阵列类。TLS 跨 SP 持续、UAV/ECM 多挂和任务内换机后置。详细采样步骤、类型差别和验收模板见交接与通用经验。
机库多挂预览/演出实例未适配；UAV/ECM 只允许单挂；TLS→其他 SP 仍结束，F-15E TLS→MSL 已验证可自然结束。

## 协作规则

1. 交流、代码注释和文档用简洁中文。函数、变量和关键逻辑有必要注释，避免过度设计、补丁式掩盖、吞错和静默回退。背景不足或环境阻塞及时求助，不盲试。
2. 阶段授权允许修改后编译、语法核对、备份安装和停止游戏；允许只读现场研究与菜单操作。开始任务和发射由用户完成。未经明确要求不新增或运行测试模块。
3. 默认不自动启动游戏，不推送或公开发布。每次先核对实际 PID/安装版本，不复用旧对象地址；当前游戏运行时不覆盖已加载文件。
4. 每架按三单挂、三双挂和三挂一次交付，集中测试。布局以美观为主，但保留真实锁定数、齐射、弹药/冷却、离架和补装。不以调试验收代替原生补给/出击。
5. 正常加载、换机、补给和 HUD 交接保持多 SP 布局连续；未就绪用受控等待，不展示猜测数据、不隐藏整页。真正接入失败/超时允许恢复原生单 SP，但须明确记录和显示错误。
6. F6 的普通“保存设置”和“应用 SP 调试方案”分开；原生出击失败不得静默转调试选装。暂停按拥有者交接，失败可见，不强制写零。
7. 功能改动同步更新说明，严格区分用户实测、离线研究和未验证项。清晰标出被后续修正的旧结论。
8. 优先 pwsh7，复杂处理用 Python。新文件 UTF-8，Windows 启动脚本 UTF-8 BOM；Temp/backup 均忽略。无 Git 的既有文件修改前备份；清理送回收站，不永久删除。
9. 不提交原始游戏资源、工具、个人配置/存档、转储、临时产物、第三方 PAK 或 SP2.wav。允许 subAgent，但须遵守文件所有权与当前上级委派规则。

## 其他导航

[开发发布](docs/development.md)；[管理](docs/mod-management.md)；[架构](docs/architecture.md)；[历史项目记录](docs/research/pre-1.1.0-project-history.txt)。旧架构/研究文档按注明版本理解，以 1.1.0 交接及当前代码为准。
