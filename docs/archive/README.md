# 研究与历史归档

日常开发从main/v2.16.0开始，不通读旧流水。这里保存恢复位置、放弃原因和重要证据，不改变现行功能。

## 重要回退方案

| 研究 | 恢复锚 | 为什么不作为当前方案 |
|---|---|---|
| F22/Su47/Su57 TGM完整内舱与共舱动画 | `archive/full-bay-mount-2.8.1` / `57fd5d3` | 几何思路可行，但Su47/TGM严重掉帧、空架/重复开合；转向机外明显模型保留 |
| F22腹部骨显隐 | `archive/f22-belly-bones-2.11` / `9c5d3d2`；研究`8c83e00` | 高频骨操作代价，后来撤去；保侧8AAM、腹舱原生 |
| X02 EML与腹舱姿态 | `archive/x02s-eml-2.12` / `0b11a26` | 炮体与舱门冲突影响观感；`4005e1d`撤销外观适配 |
| CFA保开度、阻止ADMM/EML互切动画 | `archive/cfa-animation-2.15` / `a4f7d1b` | 旧强收ECM公共进度阻断原生发射阶段；2.16恢复原生，性能/隐藏仍待优化 |
| CFA/ADF轻量缓存与原生取舍 | `8413328`，合并成品`92690a7` | 当前发布来源；仍保留已记录卡顿和ECM模型问题 |

完整旧论证分别在[旧挂载研究](multi-sp-mount-design-before-2.16.txt)、[旧架构](architecture-before-2.16.txt)、[旧开发记录](development-before-2.16.txt)。这些文本按整理前原字节保留，里面的“当前”、路径和旧链接属于当时状态；当前规则以正文为准。

## Git与工作树

整理前完整主树标签：`archive/project-before-2.16-release`，指向`92690a7`。老分支tip均转为下表标签；Git bundle、HEAD/二进制差异/未跟踪文件快照及清单在本机`backup/project-archive-20261017/`。12个历史工作树转detached并锁定；8处存在的改动先压缩、逐成员核验，再去掉旧分支名。未删除这些工作树。

**普通Git快照不包含ignored资源。** 各工作树Temp中的原模型、几何、工具和构建继续原位保留，完整恢复须连同它们；锁定不等于文件权限只读。个人安装/存档备份没有清理。

唯一保留的活动研究分支`codex/2.15-hud-mission14-localization`来自线程“修复切换与机库提示”，现在研究新的手柄注视功能；它的旧a07dfe6成果已合入发布，新研究不属于2.16，不因本轮清理而删除。

| 旧分支 | 归档标签 | 提交 |
|---|---|---|
| `codex/adf11f-uav-research` | `archive/20261017/adf11f-uav-research` | `8f600e4` |
| `codex/cfa44-eml-effect-fix` | `archive/20261017/cfa44-eml-effect-fix` | `4005e1d` |
| `codex/ecm-tls-persistence` | `archive/20261017/ecm-tls-persistence` | `4e626e3` |
| `codex/investigate-hangar-exit` | `archive/20261017/investigate-hangar-exit` | `8f600e4` |
| `codex/maintenance-2-0-0` | `archive/20261017/maintenance-2-0-0` | `4e626e3` |
| `codex/management-mount-options` | `archive/20261017/management-mount-options` | `8f600e4` |
| `codex/menu-diagnostics-controls` | `archive/20261017/menu-diagnostics-controls` | `92690a7` |
| `codex/mount-expansion` | `archive/20261017/mount-expansion` | `9cefa43` |
| `codex/mount-layout-research` | `archive/20261017/mount-layout-research` | `10d6301` |
| `codex/pending-ui-polish` | `archive/20261017/pending-ui-polish` | `7e2fe4a` |
| `codex/release-2-0-tooling` | `archive/20261017/release-2-0-tooling` | `3c00f3e` |
| `codex/sp-combination-technology` | `archive/20261017/sp-combination-technology` | `e35ff09` |
| `codex/sp-switch-animation-research` | `archive/20261017/sp-switch-animation-research` | `8c83e00` |

恢复某项研究时，在主工作区从main新建codex分支，用`git show 标签:路径`查看历史，再挑需要的差异；不要整支合并旧的核心/Lua/文档。若Git仓库损坏，可通过本地all-refs.bundle恢复refs，再按git-archive-manifest.json和worktree ZIP核对未提交内容。

## 压缩资料

本机`backup/project-archive-20261017/historical-evidence-and-references.zip`保存早期0.x/1.x候选、旧native/integration取证与历史截图，按原路径附SHA256；整理前标签也能恢复Git内原文。`1.1.0-release.json`作为运行库来源校验仍原位保留。

`historical-builds.zip`压缩旧构建，保留当前2.16、CFA旧2.15、X02旧2.12、完整舱2.8.1及旧正式2.4原目录。压缩档逐成员核验后，收集的原件送回收站，未永久删除；详见materials-archive-manifest.json。

当前生成器依赖的11份证据、全部研究脚本/配方以及50副本模型输入原位保留，不能只为目录整齐移走。ZIP不进Git或公开发布包；从其他机器恢复时，可用Git归档标签先还原文本/证据，本机游戏资源另备。
