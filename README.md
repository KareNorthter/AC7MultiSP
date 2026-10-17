# AC7MultiSP

在《皇牌空战7》单人任务中选择1至3种特殊兵装，并保留各自弹药、装填和切换。提供原生选装、HUD、部分机体的多SP外观及中英文管理界面。

当前正式版：**2.16.0**，已由用户测试并授权合入 main。该版已知的 CFA-44 切换短卡/ECM模型露出、ADF-01停光卡顿随版记录，不代表全机全组合无问题。

当前已安装整合候选 **2.18.0** 位于`codex/integration-2.18.0`：继续接收CFA切换减负，机库等任务外仅用原生模型；固定SP限制更正为第13关，手柄X注视选项移到第二个“按键”页。成套构建、现有检查及安装核验通过。2.17用户已反馈注视正常、ECM隐藏可接受，切换仍卡；本轮改善待实测，正式包仍为2.16。实现和验证边界见[挂载设计](docs/multi-sp-mount-design.md)。

- [下载本地发布包](Release/AC7MultiSP-2.16.0.zip)
- [中文 ReadMe](docs/README_zh-CN.txt) · [English ReadMe](docs/README_en.txt)
- [开发与恢复](docs/development.md) · [当前架构](docs/architecture.md)
- [挂载设计](docs/multi-sp-mount-design.md) · [研究归档](docs/archive/README.md)
- [证据分类索引](evidence/README.md)
- [版本更新说明](docs/版本更新说明.md)

后续从 main 新建 `codex/` 分支开发，完成后回到 main 交付。旧研究按归档索引恢复，不作为并行正式版本。
