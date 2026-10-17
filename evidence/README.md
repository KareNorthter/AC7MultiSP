# 证据分类索引

当前正式版本 **2.16.0 / main / v2.16.0**。正文只说明当前功能，旧报告描述当时状态，不覆盖最新实测。完整历史与压缩恢复见[归档索引](../docs/archive/README.md)。

## 当前候选

2.17.0按用户指定接收CFA修订、目标注视及第12关更正，来源和安装见`2.17.0-integration.json`与`2.17.0-installed.json`。候选待用户实测；原始研究证据`target-gaze-research.json`保留，不能把原生轴采样当作新功能实测。

## 当前交付

- [用户实测](2.16.0-user-validation.json)：CFA切换短卡/ECM模型露出、ADF停光卡顿，其他基本正常；当前问题暂不修复。
- [安装回执](2.16.0-installed.json) · [整合与原检查](2.16.0-integration.json) · [发布回执](2.16.0-release.json)。本次发布沿用已测字节，无重新编译或运行测试。
- [源码阶段调查](2.16.0-performance-investigation.json)及[上轮反馈](2.15.0-user-validation.json)保留原状态，不能把旧4fps报告当成当前持续复现。
- `Temp/build-2.16.0-final`：实际build-receipt.json、test-receipt.json和完整产物；当前50资源/100成员PAK，17项运行文件。

## 构建输入：原路径、原字节保留

以下11份被source_snapshot或生成器直接引用，不归入压缩清理：

- `mount-expansion-batch1.json`、`mount-expansion-batch2.json`、`mount-expansion-batch3.json`
- `static-expansion-2.11.0.json`、`static-expansion-2.12.0.json`、`static-expansion-2.13.0.json`、`static-expansion-2.14.0.json`
- `sp-combination-static-split-experiment.json`、`mount-expansion-animated.json`
- `su47-native-endpoint-geometry.json`、`sp-animation-product-access.json`

`1.1.0-release.json`仍用于核旧包及微软运行库；`2.15.0-installed.json`是当前构建的升级来源，不能重写历史回执。

## 分类查找

| 名称/前缀 | 内容 |
|---|---|
| `2.*-build/tests/installed/integration/release` | 按版本的构建、核验与交付快照；只有当前2.16为最新正式版 |
| `*-user-validation.json` | 用户反馈及明确边界，不推定未点名组合验收 |
| `mount-*`、`static-*`、`su30sm-*` | 几何、Socket、资源往返、数量与布局研究 |
| `sp-*`、`animated-*`、`x02s-*` | 动画、完整弹舱、发布取位及回退方案 |
| `*uav*`、`*continuous*`、`*ecm*`、`*tls*` | 已登记持续兵装和生命周期研究 |
| `compatibility-*` | 历史兼容实测；第三方版本和场景边界保留，不等于2.16全套复测 |
| `2.4.0-*-investigation*` | 机库退出/菜单性能旧调查，未确认根因继续保留 |
| 本机`backup/project-archive-20261017/` | Git bundle、研究工作树快照、旧证据/截图和旧构建压缩档及校验清单 |

早期0.x/1.x、旧native/integration和截图从当前列表移出；可从归档标签或本机压缩档按原路径恢复。新增反馈写到对应版本，设计变化更新正文；不再把每次流水追加到主管入口。
