# AC7MultiSP

主工作区 `E:/AC7_MultiSP`；主线正式基线为用户实测通过的 **AC7MultiSP 2.4.0**，定位标签 `v2.4.0`。后续功能从 main 新建分支开发。开工先看 Git 状态。
按需读[架构](docs/architecture.md)或[开发说明](docs/development.md)；研究机体挂载先读[挂载设计经验](docs/multi-sp-mount-design.md)，无需通读证据和历史。用户操作见[安装说明](docs/安装说明.txt)，交付范围见[版本说明](docs/release.txt)。

## 当前事实

- Steam build 9855922，单人战役/自由任务；当前安装及正式版为2.4.0。支持原生出击/补给1–3 SP、百分比向上取整配弹、独立弹药/冷却、调试换装、三个可配置单键/组合快捷键，以及机库/补给动态模型。2026-10-16用户确认“这些改动一切正常，两个支线的成果也都验证无误”，授权全部提交main作为正式版；通过记录见evidence/2.4.0-user-validation.json。
- 固定挂载只适配原版 Su-30SM、F-35C、F-15E。用户最新认可当前布局：F-15E的6AAM下落段与SFFS几乎不再穿插；F-35C和Su-30SM恢复原有布局更合理，保留十二处8AGM。有效布局与扩机型方法统一维护在挂载设计经验中。用户确认 .11 的暂停、管理、实时换装及 F-35C 各组合/动画正常；不能扩大为所有机型、任务和检查点通过。
- 用户确认 2.0“实测各个功能都没问题”，已合入 main。2.0.0完成配置/协议分离、页面具名接口、状态清理、挂载边界及文件写入失败检查；2026-10-16用户确认“测试没问题，可以提交了”，标签v2.0.0保留历史基线，当前正式基线为v2.4.0。
- 用户报告机库预览转镜头明显掉帧，实验多挂开启/关闭均出现。原因未定，后续对照完全禁用mod、仅禁用模型、开启模型的同场景帧时间，暂不归因为游戏或mod。
- 历史崩溃/自行退出根因未定位；本次通过反馈不能证明所有任务和长期稳定性。
- 2.4.0仅对已核对的ADFX-01原生IEWS开放多挂并实验保留IEWS/TLS跨SP效果；其他ECM与UAV仍仅单挂，其他TLS跨SP仍结束；机库/补给动态预览已接入，本版已有用户基本正常反馈；完整绿/黄性能条未完成。后续先补原生流程/稳定性证据，再扩机型。
- 2.4.0已整合codex/mount-layout-research的三机型布局修订与codex/ecm-tls-persistence的ADFX-01原生IEWS/TLS持续效果，连同挂载设计经验提交main。研究工作区保留原样，用户已确认两支线成果正常；不扩大为全机型、所有任务或故障注入均验证。
- 用户要求禁用`proportional guided missile_P.pak`，已移至backup/disabled-guidance-20261016-231440，原PAK哈希和恢复路径见其中disable-receipt.json；其他模组保留。
- 2.4.0已作为本地正式版提交main并标记v2.4.0，未制作ZIP或远端发布；2.0/2.0.0均未制作ZIP；本地旧包 `Release/AC7DualSP-1.1.0.zip` 保持原发布字节。验证凭据按需查[证据索引](evidence/README.md)。

最新2.8.0反馈：Su-47三SP任务约1fps、机库正常，关闭多挂模型恢复；F-22A/TG布局满意但任务约7fps。主线另修热路径，本研究仅修Su-47完整左右分区候选，不编译/安装，不证明性能改善。此前主工作区2.7.0：F-16C、Mirage、Typhoon正常；F-2A模型成功但卡顿，RKT连射中切换触发控制器错误；Rafale资源已加载、Socket顺序校验回退。此分支按反馈补齐Typhoon三个双挂空位，数据与正文同步；运行修复源码在E:/AC7_MultiSP，未构建或安装，不能将修订候选计为实测通过。详见evidence/mount-expansion-feedback-20261017.json。

## 工作规则

挂载方案研究位于`codex/mount-expansion`，从main/v2.4.0（8f600e4）创建。三批44机七组合的离线研究完成，连同正式三机共47机；不编译、运行测试或安装。用户最新要求整机七组合齐全才做，否则整机后置，不能只做部分组合。依据静态分区技术成果7ae97b6新增F-16C、F-2A、Mirage、Rafale四机；原静态路线十机70条；动画覆盖层当前F-22A／Su-57TG／CFA-44三机21条可继续接入布局，合计13机91条，其余31机整体暂缓；Su-47左右七组合设计须先半侧骨架资源。CFA-44按用户要求多挂左ADMM42管口／右EML1射点（原窗口2不缩减）／隐藏IEWS外观，使用原生骨支路显隐候选，需新profile与反射宿主；三单保持原生。ADF-11F左UAV／右双QAAM已给三LOD未来配方，但仍需静态派生资源和UAV生命周期，整机暂缓。动画方案须专用Host/provider接入。Su-47现为原生左右分区七组合设计，因SAAM整架离架扫架须先SWP1左半/SWP2与SWP3右半派生资源，整机暂缓。后续采用公共舱全开显隐和实时原生provider取位，不沿用旧38Body骨精确门槛；用户允许轻微挂载/离架接触，仍须明确明显扫架和真实轨迹边界，不自动加入旧静态数组或运行白名单。新入口为evidence/mount-expansion-animated.json及handoff的animated_design_overlay；控制源码及完整边界见挂载设计经验。交接入口`evidence/mount-expansion-handoff.json`的`eligible_aircraft`；各批`integration_gate.eligible`是最终门槛，不能单看`candidate_existing_assets`或`candidate_split_assets`。仅接受有全部LOD与编码往返依据的静态副本，未核对分区、新站及动画/支撑未明继续后置，资料不表示已适配或实测。ECM/IEWS与TLS不因持续效果预设互斥，具体类接入与布局分别核对。

1. 中文交流。遇到资料或环境阻塞及时报告。
2. 2.4.0已获用户完整功能及两支线实测通过反馈，授权作为正式版提交main。默认不自动编译、新增或运行测试、安装、操作游戏；开发阶段的构建安装须在用户授权范围内。正式化沿用已实测字节，不重新构建；不自动推送或上传。
3. 新分支从 main 创建，前缀 `codex/`；旧工作区仅供恢复，不继续开发。未合入成果的定位见开发说明。
4. Lua唯一源`dist/DualSP/Scripts/`；2.4.0正式核心为`dist/DualSP/cores/2.4.0/DualSP.dll`，UI为`dist/DualSP/bin/DualSPUI.dll`，与Temp/build-2.4.0及已实测安装一致。2.0.0旧核心保留，仅与v2.0.0源码和当时UI配套，旧UI从Git标签恢复。旧`dist/DualSP/bin/DualSP.dll`不得改，SHA256为`D7D7055CA7909021EEACA15E63FF5FF253D2EB7E60A3FE3B79DCEEAB231DE339`。
5. 功能变化更新对应正文，不追加流水账或另建重复交接文档。默认只维护本入口、架构、开发、用户安装及版本说明；确有独立用途才新增文档。
6. pwsh7 优先，复杂文件处理用 Python；UTF-8，Windows 启动脚本沿用 BOM。临时文件 Temp，备份 backup；无 Git 文件修改前备份，清理送回收站。
7. 不提交个人状态、游戏资源、音频、第三方 PAK、工具、转储及 ZIP；遵守同一范围与文件所有权。

默认检索由 `.ignore` 排除 evidence、references、vendor；需要时定向读取或用 `rg --no-ignore`。历史说明只留 Git，不作为日常预读材料。
