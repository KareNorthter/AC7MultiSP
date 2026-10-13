# 精选证据索引

返回[项目入口](../AGENTS.md)，相关结论见[架构](../docs/architecture.md)、[核心研究](../docs/research/core-findings.md)、[模型研究](../docs/research/weapon-models.md)。

| 保留内容 | 说明与边界 |
|---|---|
| `0.1.7-*.json` | 迁移前的最近离线检查及安装核验；本轮没有重新执行这些检查 |
| `native-hud-layout.json` | 自有原生 HUD 资源的结构摘要 |
| `native-loadout-structure.json` | 准备页类、控件与布局摘要 |
| `weapon-model-layout.json` | Su-35S 挂架、Socket 与武器装填数量摘要 |
| `dual-mount-prototype.json` | 仅几何分配原型，未安装游戏功能；内部旧版本字样属于当时结论 |
| `unusable-packed-disk-code.json` | 加壳磁盘代码不适合作为写入接口的依据 |
| `native/` | 精选历史函数体与反射接口，不能重用旧进程绝对地址 |
| `integration/` | 早期实机原型通过记录，保留为研究依据，不等同于最新版本通过 |

完整 PAK 资源副本、资源解析中间 JSON、早期日志、失败尝试和重复版本检查已回收；重要失败机制与未知项保留在[关键决策](../docs/decisions.md)。当前运行文件与原始发行包 hash 另见[基线记录](../Release/baseline-0.1.7.json)。

本次整理的范围与结果见[迁移概要](migration-summary.json)，逐文件复制审计留在被忽略的 `Temp/`，不作为游戏功能验证记录。
