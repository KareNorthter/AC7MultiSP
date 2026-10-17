# AC7MultiSP

在《皇牌空战7》单人任务中选择1至3种特殊兵装，并保留各自弹药、装填和切换。提供原生选装、HUD、部分机体的多SP外观及中英文管理界面。

当前正式版：**2.16.0**，已由用户测试并授权合入 main。该版已知的 CFA-44 切换短卡/ECM模型露出、ADF-01停光卡顿随版记录，不代表全机全组合无问题。

当前已安装整合候选 **2.17.0**：接收CFA切换及ECM修订、单键持续目标注视，并将固定SP限制更正为第12关。用户反馈CFA切换仍卡，ECM模型隐藏已可接受。当前 `codex/cfa44-switch-performance` 基于该候选继续减少切换开销，并让CFA机库等任务外场景仅用原生模型；本轮源码尚未构建或安装，正式包仍为2.16。实现和验证边界见[挂载设计](docs/multi-sp-mount-design.md)。

- [下载本地发布包](Release/AC7MultiSP-2.16.0.zip)
- [中文 ReadMe](docs/README_zh-CN.txt) · [English ReadMe](docs/README_en.txt)
- [开发与恢复](docs/development.md) · [当前架构](docs/architecture.md)
- [挂载设计](docs/multi-sp-mount-design.md) · [研究归档](docs/archive/README.md)
- [证据分类索引](evidence/README.md)
- [版本更新说明](docs/版本更新说明.md)

后续从 main 新建 `codex/` 分支开发，完成后回到 main 交付。旧研究按归档索引恢复，不作为并行正式版本。
