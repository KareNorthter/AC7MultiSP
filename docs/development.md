# 开发与恢复

主工作区 E:/AC7_MultiSP，开发分支从 main 创建；授权、版本与待办以 [AGENTS](../AGENTS.md) 为准。

当前main正式基线为2.4.0，标签v2.4.0。用户已确认全部改动及两条研究支线成果正常，授权提交正式版；可配置快捷键、三机型挂载修订及ADFX-01持续效果已统一纳入。正式核心在dist/DualSP/cores/2.4.0/DualSP.dll，UI在dist/DualSP/bin/DualSPUI.dll，均从Temp/build-2.4.0按原字节归档，未重新构建或安装。有效布局及扩机型方法见[挂载设计经验](multi-sp-mount-design.md)。

## 动画布局覆盖层的取用

本分支以`evidence/mount-expansion-animated.json`记录八机深入评审与三机完整七组合，统一正文见[动画候选](multi-sp-mount-design.md#带动画机体的完整七组合候选)。`mount-expansion-handoff.json`的`animated_design_overlay`单独绑定这份数据；旧`eligible_aircraft`／`integration_candidates`仍只服务十机静态路线，旧三批记录不当作最新动画布局。不要直接把三机代码追加到旧运行生成器：本层显式区分owner/provider/呈现架、隐藏provider刷新与内部ammo门控，旧数据格式没有这些保证。

引用线程`01a14787-e328-72b3-9b0e-1bfa7d81cc52`的`analyze_sp_switch_animations.py`、`sp_switch_animation_policies.json`与`sp-switch-animation-research.json`，并从其提交`00bd24c`按原字节吸收`common_bay_animation.hpp`、`native_common_bay_parts.hpp`、`owned_bone_refresh.hpp`、`inspect_common_bay_control.py`与`sp-common-bay-control.json`。三个控制头没有自动注册／新钩子，不改变产品运行代码。几何准入来自本分支覆盖层，YF-23虽有控制profile仍被几何排除。

产品接入必须补齐：安全访问与反射Host；完整原生实例／场景身份校验；同步切换快照与原生Parts更新顺序；Pause/Tick及刷新模式最新原生请求观察和逐组件归还；provider当帧组件空间／世界空间Socket取位；Su-47的严格全开＋端点姿态见证。Parts原生引擎不是UObject；指纹／owner／配置必须通过，写入部分失败及归还失败不得误报成功。TG第三组静态不读骨架字段，第三原生配置的NativeTransition保留原生流程。姿态容差、坐标换算和运行池数量需有真实依据，不能填经验值或直接等于Socket数。

| 离线入口 | 作用 |
|---|---|
| `inspect_mount_assets.py --asset-path` | 可重复指定精确PAK相对.uasset路径；提取同名HUD图与真实模型时避免歧义，仍保留原basename重复保护，不改原游戏包 |
| `read_cooked_skeletal_mount.py` | 严格解析本版本骨架网格Extras、全部LOD/分区/权重/索引与参考骨架；不扫描偏移猜顶点，不支持的布局直接拒绝。格式参考UEViewer的UE4原始序列化实现 |
| `audit_animated_mount_geometry.py` | 核对25份机体／挂架、49LOD的完整读取与参考姿态蒙皮回构，仅输出摘要 |
| `review_animated_us_bays.py` | F-22A／YF-23及FB-22旧参考；真实Socket、骨架、原生接触基线及XSDB展开序列 |
| `review_animated_sukhoi.py` | Su-47／普通Su-57／Su-57TG七组合、完整架、Body及原生弹体核对 |
| `review_animated_dark_falken.py`、`review_animated_fb22.py` | DarkStar／ADF-01／FB-22整架与固定Body干涉，保留拒绝依据 |
| `build_animated_mount_candidates.py` | 只接收人工评审的整机七组合，校验真实owner类／自身弹体／provider点与更新集合，生成独立覆盖层；不启用机型 |
| `build_mount_handoff.py --animated-batch` | 校验覆盖层七组合完整性和来源指纹，将静态路线与动画路线分开索引 |

分析器只读显式manifest与资源，原始提取物／完整大采样在Temp；提交的证据保留摘要、少量接触见证和哈希，不带游戏资源、PAK或原始顶点数组。NumPy仅用于离线几何，未下载新依赖。数据整理与资源分析不属于游戏编译或运行测试，本轮均未执行后者。

复现须提供本机原生资源manifest（包含Pawn／三组SWP／参数表／Body／各序列与武器模型；F-22还需真实XSDB骨架、Skeleton及展开序列），分析器参数见各入口。所有输出必须新路径并放Temp，例如：

```powershell
python -X utf8 src/research/audit_animated_mount_geometry.py --manifest Temp/batch3-manifests.json --manifest Temp/body-manifest.json --batch evidence/mount-expansion-batch3.json --aircraft f22a --aircraft su47 --aircraft su57tg --output Temp/new-geometry-review.json
python -X utf8 src/research/build_animated_mount_candidates.py --batch evidence/mount-expansion-batch3.json --review Temp/animated-us-bays-review.json --review Temp/animated-sukhoi-review.json --review Temp/animated-dark-falken-review.json --review Temp/animated-fb22-review.json --geometry-audit Temp/animated-mount-geometry-audit-final.json --control-source src/common_bay_animation.hpp --control-source src/native_common_bay_parts.hpp --control-source src/owned_bone_refresh.hpp --admit f22a --admit su47 --admit su57tg --output Temp/new-animated-candidates.json
python -X utf8 src/research/build_mount_handoff.py --catalog evidence/mount-expansion-catalog.json --batch evidence/mount-expansion-batch1.json --batch evidence/mount-expansion-batch2.json --batch evidence/mount-expansion-batch3.json --animated-batch evidence/mount-expansion-animated.json --output Temp/new-mount-handoff.json
```

覆盖层中`reviews`的完整分析指纹、`analysis_sources`及`control_sources`限定取用版本；US／Sukhoi分析器入库仅统一行尾及末尾空行，原分析指纹保留，入库源码指纹另列，没有重新运行几何。后续修改控制头、解析器或资源需重核对相应依据。XSDB展开播放推导不能替代实际释放运动，Su-47显隐也不能证明闭舱实体净空。全部新增机体待产品接入和用户实玩，不编译、不安装、不操作当前游戏。

## 有效入口

原版机体扩展研究在独立工作区`codex/mount-expansion`推进，以v2.4.0（8f600e4）为起点，不修改正式版安装。分批结论与整合要求统一见[挂载设计经验](multi-sp-mount-design.md#其他原版机体的分批研究)；完整机体索引见[evidence/mount-expansion-catalog.json](../evidence/mount-expansion-catalog.json)。研究脚本只读取离线资源，新增候选未编译、未运行测试、未安装。

三批分别六、十五、二十三机，已覆盖其余44架各七组合；连同正式三机型共47架／329种情况。用户最新要求整机七组合齐全才做，有任一组合受阻就整机后置，不交付部分组合。吸收技术线程7ae97b6的静态分区依据，新增F-16C、F-2A、Mirage、Rafale四机28条。旧静态路线十机70条（30单挂＋40多挂）。动画覆盖层新增F-22A、Su-47、Su-57TG三机21条，合计13机91条完整布局，其余31机整机暂缓；三机尚需专用动画Host/provider接入，不是已运行适配。统一入口为[evidence/mount-expansion-handoff.json](../evidence/mount-expansion-handoff.json)的`eligible_aircraft`与`integration_candidates`，可读全机表及逐机七组合见[挂载设计经验](multi-sp-mount-design.md#全机型接入总表)。

各批新增`integration_gate.eligible`与逐项`integration_allowed`；先按整机门槛筛选，再看资源状态`disposition`。十机40条多挂包括26条原资源复用与14条`candidate_split_assets`。后者的`split_evidence`绑定回执和配方指纹，`rack_plan.carriers`给出每组原架／副本／仅隐藏自身的规则；P1/P2不能绕过整机门槛。其他未核对的拆架、拆巢、新站和动画／同舱支撑未明继续后置，不把研究完成写成已适配。固定炮、火箭／ADMM管口、UAV部署点与ECM/IEWS效果源按独立角色管理；不把资源点数抄成逻辑长度，不新增TLS或ECM/IEWS功能互斥。研究工具仅扩展离线资料，未改运行时注册或容量。A-10C的42个RKT管口为三对横向簇，实体巢数仍待完整几何确认。

静态分区成果从`codex/sp-combination-technology`的`7ae97b6`按原字节引入四个文件：分区工具、实验入口、配方和往返回执，未带入该分支AGENTS或覆盖其文档正文。工具及配方指纹与回执一致。`build_static_split_mount_plan.py`将已有载体落实到首批四机七组合，验证来源及所属组，仅输出方案数据；本轮未重新生成游戏资产、运行实验入口、编译、测试、打包或安装。游戏资产仍在技术工作区Temp，索引不依赖本机绝对路径。

整合时先读设计说明中的“已核对静态分区的接入约定”：独立路径加载、引擎引用、同组网格替换与归还、NoCollision及烘焙依赖检查都是尚未完成的运行工作。上游回执`integration_allowed=false`不改写；本研究`integration_gate.eligible`只表示方案可继续接入，不等于副本可直接安装。

| 工作 | 工具 |
|---|---|
| 完整构建 | src/build.py，成套构建核心/UI，输出全新 Temp 子目录 |
| 自动检查 | tests/run.py --build-dir 指向上述目录，失败不会写通过回执 |
| 本机升级 | src/install.py，默认预检，显式 --apply 安装，不启动游戏 |
| 本地打包 | src/package-release.py，使用当前产品清单与完整构建回执，已有 ZIP 不覆盖 |

2.0.0已在 Temp/build-2.0.0 完整构建、检查并安装，用户实测通过，历史基线标签为v2.0.0。下列为操作顺序示例，现有目录和安装回执已存在，勿重复执行；后续新构建须使用全新目录并核对当时安装基线：

```powershell
python -X utf8 src/build.py --output-dir Temp/build-2.0.0
python -X utf8 tests/run.py --build-dir Temp/build-2.0.0
python -X utf8 src/install.py --build-dir Temp/build-2.0.0
python -X utf8 src/install.py --build-dir Temp/build-2.0.0 --apply
```

构建输出必须是全新目录；默认工具根为 tools，可指定 --tools-root。每次完整编译，不再接受旧 --reuse-core-build。自动检查不等于实玩。

build.cmd/build-core-portable.cmd/build-ui.cmd 和旧研究构建入口只转发 build.py，全部构建成套产物，不直接覆盖 dist。旧三SP安装入口只转发 install.py；单机型安装入口明确拒绝执行，原实现可从 Git 恢复。其余历史研究/补给安装器不用于当前版本。

## 来源与交付

产品名称与 Lua 清单在 src/product.json；版本在 VERSION。Lua 唯一源 dist/DualSP/Scripts，共十二个脚本。2.4.0正式核心位于dist/DualSP/cores/2.4.0/DualSP.dll，UI位于dist/DualSP/bin/DualSPUI.dll，构建副本在Temp/build-2.4.0，与已实测安装及源码一致；旧2.0.0核心保留，旧UI从v2.0.0标签恢复，仅与当时源码配套。旧 bin/DualSP.dll 仅作保护基线。
加载器/许可从 payload 显式取用；[工具清单](../tools/README.md)、音频、运行库、ZIP 是本地依赖，不入 Git。

构建回执包含 src 产品 C++/头文件、vendor 全树、所有 Lua、正式工具与参数，以及编译命令和工具链全树指纹；构建前后核对一致。安装/打包再次核对整个来源集合，不能只比对清单中少数文件。旧格式回执不接受。

本机升级基线由 product.json 的 upgrade_receipt 指定，本次为2.3.1→2.4.0，candidate=true为冻结构建时的候选标记；正式状态以main、v2.4.0和用户验证记录为准，保留来源及历史安装回执字节。已核对游戏退出后安装2.4.0。不能重跑旧安装覆盖历史回执。未来升级须选择新的完整runtime_files基线。安装器接受schema 2/管理参数5或6并保留个人配置原字节；2.4.0首次启动先独立备份，再将快捷键迁至管理参数6。现有脚本须与基线完整匹配，新模块必须登记，不能静默删除旧模块。
安装必须有匹配构建和测试源码的通过回执。退出游戏后整套暂存、核对、备份模组和存档，再交换目录；保留个人配置、其他模组及外部 PAK。第二步交换失败会尝试归还原目录，不启动游戏。

[2.4.0用户验证](../evidence/2.4.0-user-validation.json)记录正式化授权、范围及回执摘要；正式DLL与用户测试构建逐字节相同。源码清单保留原candidate字段以维持构建来源校验，未改动运行逻辑、构建或旧回执；正式版状态由Git标签与用户通过记录确定。本轮不制作ZIP、不推送、不操作游戏。

2.4.0的[构建](../evidence/2.4.0-build.json)、[检查](../evidence/2.4.0-tests.json)、[安装](../evidence/2.4.0-installed.json)已归档；备份backup/AC7MultiSP-20261016-231446-459750，113项受保护文件核验通过。真实配置安装时保留原字节，首次启动迁移；临时副本迁移已核对方案、旧管理键与其余字段不变。未自动启动游戏。

比例引导mod按用户要求禁用：原Game/Content/Paks/~mods/proportional guided missile_P.pak移到backup/disabled-guidance-20261016-231440，SHA256为41b49ad83d60509795db910a02fd24c159ab39c324b5d1b745906f4a95a094e5，其中disable-receipt.json记录恢复位置；需恢复时退出游戏再原样移回。未删除或改写PAK，其他模组由安装器核验。

2.3.1的[构建](../evidence/2.3.1-build.json)、[检查](../evidence/2.3.1-tests.json)已归档，已安装，见[安装回执](../evidence/2.3.1-installed.json)；最终产物Temp/build-2.3.1-final。完整模组/存档备份位于backup/AC7MultiSP-20261016-223134-297074，个人配置和其他模组保留，111项受保护文件核验通过，未自动启动游戏。先前Temp/build-2.3.1因构建期间源码收尾变更被来源校验拒绝，不能交付。

2.3.0的[构建](../evidence/2.3.0-build.json)、[检查](../evidence/2.3.0-tests.json)、[安装](../evidence/2.3.0-installed.json)已留存；备份为backup/AC7MultiSP-20261016-194543-836550，安装后108项受保护文件核验通过，个人配置及完整模组/存档备份核验通过。未启动游戏，自动检查不证明真实挂点、动画或补给效果。

[2.0构建](../evidence/2.0-build.json)、[检查](../evidence/2.0-tests.json)、[安装](../evidence/2.0-installed.json)、[用户实测](../evidence/2.0-user-validation.json)均保留原记录，对应旧基线3c00f3e。
2.0.0的[构建](../evidence/2.0.0-build.json)、[检查](../evidence/2.0.0-tests.json)、[安装](../evidence/2.0.0-installed.json)回执已保存，完整产物原样存入 dist。原模组与存档备份在 backup/AC7MultiSP-20261016-120724-927183；个人配置、存档、其他模组核验未变。安装时正常关闭游戏且未自动重启；用户随后确认[实测正常](../evidence/2.0.0-user-validation.json)，授权提交2.0.0并合入main。标签v2.0.0保留历史基线，后续从当前main/v2.4.0开发；现存1.1.0 ZIP不变。

## 本轮整理范围

- tests/run.py 运行来源拒绝检查、三SP纯规则、配置迁移/保存失败、协议应答、页面具名接口、冷却、指针、缓存恢复/挂载边界及DLL加载核对；旧双SP控制器/HUD/选装模拟不适用，未纳入当前通过范围。
- 2.2.2补齐页面激活、机库组件重建与开场定位；只向相同身份组件继承原生请求，空池等待，真实异常回退，保持既有兵装事务；检查覆盖21种组合、配弹边界、单多切换、失败隔离和补给交接，不能替代未列出的引擎和故障注入场景；2.4.0已获用户统一实测通过反馈。

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
按用户要求，独立的[挂载设计经验](multi-sp-mount-design.md)统一维护数量概念、三机型七组合、取舍依据及扩机型流程，吸收原架构中的布局说明。后续机体结论在同一文档扩展，不再另建相似挂载专题、状态页或交接流水账。docs/research/three-sp-candidate.txt仅保留旧工具所需的退役指路，不作为有效设计说明。必要回执按[证据索引](../evidence/README.md)读取。
