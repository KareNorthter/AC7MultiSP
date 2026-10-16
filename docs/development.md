# 开发与恢复

主工作区 E:/AC7_MultiSP，开发分支从 main 创建；授权、版本与待办以 [AGENTS](../AGENTS.md) 为准。

当前整合分支为`codex/management-mount-options`。2.4.0已完整编译、通过适用检查并安装，产物Temp/build-2.4.0；支持三个快捷键自定义单键/组合键，合入三机型挂载反馈修订和ADFX-01持续效果实验。2.3.1快捷键已获用户实测正常反馈；2.4.0新绑定、网格换位和持续效果待用户实测，未合入main或发布。

## 有效入口

| 工作 | 工具 |
|---|---|
| 完整构建 | src/build.py，成套构建核心/UI，输出全新 Temp 子目录 |
| 自动检查 | tests/run.py --build-dir 指向上述目录，失败不会写通过回执 |
| 本机升级 | src/install.py，默认预检，显式 --apply 安装，不启动游戏 |
| 本地打包 | src/package-release.py，使用当前产品清单与完整构建回执，已有 ZIP 不覆盖 |

2.0.0已在 Temp/build-2.0.0 完整构建、检查并安装，用户实测通过，主线基线标签为 v2.0.0。下列为操作顺序示例，现有目录和安装回执已存在，勿重复执行；后续新构建须使用全新目录并核对当时安装基线：

```powershell
python -X utf8 src/build.py --output-dir Temp/build-2.0.0
python -X utf8 tests/run.py --build-dir Temp/build-2.0.0
python -X utf8 src/install.py --build-dir Temp/build-2.0.0
python -X utf8 src/install.py --build-dir Temp/build-2.0.0 --apply
```

构建输出必须是全新目录；默认工具根为 tools，可指定 --tools-root。每次完整编译，不再接受旧 --reuse-core-build。自动检查不等于实玩。

build.cmd/build-core-portable.cmd/build-ui.cmd 和旧研究构建入口只转发 build.py，全部构建成套产物，不直接覆盖 dist。旧三SP安装入口只转发 install.py；单机型安装入口明确拒绝执行，原实现可从 Git 恢复。其余历史研究/补给安装器不用于当前版本。

## 来源与交付

产品名称与 Lua 清单在 src/product.json；版本在 VERSION。Lua 唯一源 dist/DualSP/Scripts，共十二个脚本。2.4.0成套核心/UI位于Temp/build-2.4.0，与游戏当前安装及源码一致；dist/DualSP/cores/2.0.0/DualSP.dll 和 dist/DualSP/bin/DualSPUI.dll 仍保留v2.0.0原件，不与当前开发Lua配套。旧 bin/DualSP.dll 仅作保护基线。
加载器/许可从 payload 显式取用；[工具清单](../tools/README.md)、音频、运行库、ZIP 是本地依赖，不入 Git。

构建回执包含 src 产品 C++/头文件、vendor 全树、所有 Lua、正式工具与参数，以及编译命令和工具链全树指纹；构建前后核对一致。安装/打包再次核对整个来源集合，不能只比对清单中少数文件。旧格式回执不接受。

本机升级基线由 product.json 的 upgrade_receipt 指定，本次为2.3.1→2.4.0，candidate=true。已核对游戏退出后安装2.4.0。不能重跑旧安装覆盖历史回执。未来升级须选择新的完整runtime_files基线。安装器接受schema 2/管理参数5或6并保留个人配置原字节；2.4.0首次启动先独立备份，再将快捷键迁至管理参数6。现有脚本须与基线完整匹配，新模块必须登记，不能静默删除旧模块。
安装必须有匹配构建和测试源码的通过回执。退出游戏后整套暂存、核对、备份模组和存档，再交换目录；保留个人配置、其他模组及外部 PAK。第二步交换失败会尝试归还原目录，不启动游戏。

2.4.0的[构建](../evidence/2.4.0-build.json)、[检查](../evidence/2.4.0-tests.json)、[安装](../evidence/2.4.0-installed.json)已归档；备份backup/AC7MultiSP-20261016-231446-459750，113项受保护文件核验通过。真实配置安装时保留原字节，首次启动迁移；临时副本迁移已核对方案、旧管理键与其余字段不变。未自动启动游戏。

比例引导mod按用户要求禁用：原Game/Content/Paks/~mods/proportional guided missile_P.pak移到backup/disabled-guidance-20261016-231440，SHA256为41b49ad83d60509795db910a02fd24c159ab39c324b5d1b745906f4a95a094e5，其中disable-receipt.json记录恢复位置；需恢复时退出游戏再原样移回。未删除或改写PAK，其他模组由安装器核验。

2.3.1的[构建](../evidence/2.3.1-build.json)、[检查](../evidence/2.3.1-tests.json)已归档，已安装，见[安装回执](../evidence/2.3.1-installed.json)；最终产物Temp/build-2.3.1-final。完整模组/存档备份位于backup/AC7MultiSP-20261016-223134-297074，个人配置和其他模组保留，111项受保护文件核验通过，未自动启动游戏。先前Temp/build-2.3.1因构建期间源码收尾变更被来源校验拒绝，不能交付。

2.3.0的[构建](../evidence/2.3.0-build.json)、[检查](../evidence/2.3.0-tests.json)、[安装](../evidence/2.3.0-installed.json)已留存；备份为backup/AC7MultiSP-20261016-194543-836550，安装后108项受保护文件核验通过，个人配置及完整模组/存档备份核验通过。未启动游戏，自动检查不证明真实挂点、动画或补给效果。

[2.0构建](../evidence/2.0-build.json)、[检查](../evidence/2.0-tests.json)、[安装](../evidence/2.0-installed.json)、[用户实测](../evidence/2.0-user-validation.json)均保留原记录，对应旧基线3c00f3e。
2.0.0的[构建](../evidence/2.0.0-build.json)、[检查](../evidence/2.0.0-tests.json)、[安装](../evidence/2.0.0-installed.json)回执已保存，完整产物原样存入 dist。原模组与存档备份在 backup/AC7MultiSP-20261016-120724-927183；个人配置、存档、其他模组核验未变。安装时正常关闭游戏且未自动重启；用户随后确认[实测正常](../evidence/2.0.0-user-validation.json)，授权提交2.0.0并合入main。标签 v2.0.0 固定后续功能基线；现存1.1.0 ZIP不变。

## 本轮整理范围

- tests/run.py 运行来源拒绝检查、三SP纯规则、配置迁移/保存失败、协议应答、页面具名接口、冷却、指针、缓存恢复/挂载边界及DLL加载核对；旧双SP控制器/HUD/选装模拟不适用，未纳入当前通过范围。
- 2.2.2补齐页面激活、机库组件重建与开场定位；只向相同身份组件继承原生请求，空池等待，真实异常回退，保持既有兵装事务；检查覆盖21种组合、配弹边界、单多切换、失败隔离和补给交接，不能代替引擎模型与动画实测。

## 支线整合与恢复

2.4.0按差异整合两工作区的最新未提交候选。挂载研究保留原分支最新网格替换头、纯布局和恢复处理，并保留核心整合分支的全场景F7模式5；持续效果研究带入continuous_sp.hpp和核心接入，保留核心协议4比例配弹。原工作区不写入、不提交、不清理；研究记录分别见evidence/mount-layout-research-latest.json、evidence/ecm-tls-persistence-research.json。已完整编译、通过适用检查并安装，产物Temp/build-2.4.0。

三个快捷键配置在shortcuts中保存；UI/设置协议升6，原管理键整数只保留主键镜像。检查使用既有模块扩展，覆盖全修饰键匹配、冲突、旧配置备份迁移，以及持续引用数组的定向移除；不把合成检查当真实效果验收。

2.3.1的早期候选布局已由2.4.0按用户反馈修订；旧研究记录保留原字节，不能作为当前布局说明。

待专项测试：用户在2.3.0报告机库预览转镜头明显卡顿，实验挂载开启和关闭均出现；记录为性能疑点，未证明根因。后续在同机型/涂装/分辨率/镜头路径下对照完全禁用mod、mod开启但模型关闭、模型开启的帧时间，分离页面扫描、文件发布及模型巡检耗时；本轮不改变性能策略。

`codex/pending-ui-polish` 保存 f800518、07c81c0、7e2fe4a 三个提交；当前开发分支已按改动点吸收标题、弹药高亮和中央提示，配置/UI逻辑按2.0.0模块结构实现，未整枝覆盖主线，也未发布。

机库与补给模型研究结论见[架构](architecture.md)对应章节。已只读采样F-15E机库和ms06补给，确认补给展示任务玩家本体，旧显隐钩子保留进补给前多挂状态。实现须加入明确的预览显示所有权，不能只停周期更新或直接复用任务兵装会话。后续定向采集候选前后、起飞交接、其他机型模型池和F-35C动画驱动证据；2.2.2已接入独立视觉上下文与页面生命周期并完成编译、检查、安装；2.2.2已有用户“基本没看到问题”的实测反馈，但没有逐机型/任务明细，不能扩大验证范围。
原根目录36个未提交文件存于提交 `0637abc` / 标签 `archive/20261016/three-sp-research`。其余旧分支有同前缀恢复标签；完整映射在[仓库整理回执](../evidence/repository-consolidation-20261016.json)。
五个旧工作区保留本地产物并锁定，只有 AGENTS 有封存提示；不在那里继续开发。备份位于 backup/maintenance-20261016，含已核验 bundle。

历史说明已从当前目录移除，完整整理前快照为 `archive/20261016/docs-before-prune`。需要某个旧结论时，用 `git show <标签>:<当时路径>` 定向读取；不要恢复整套历史文档。
恢复代码应从归档标签新建 codex/ 分支，不重置 main。通常无需查历史，先读当前架构和相关代码。

## 文档分工

AGENTS 只维护状态/规则；architecture 维护模块和实现约束；本页维护工具/恢复；两个 TXT 供用户及打包器。
不再独立维护状态页、管理专题、挂载专题或整理流水账。必要回执按[证据索引](../evidence/README.md)读取，禁止每次改动再复制一套交接说明。
