# 本地发布产物

当前正式功能版本和发布状态只在[项目入口](../README.md)维护；主要变化见[玩家更新日志](../docs/user/更新日志.txt)。

作者已将安装器修订2及配套文件移至Release/2.26.0根目录。当前仅保留R2下载的新网页包为[AC7MultiSP-Pages-R2-only.zip](2.26.0/AC7MultiSP-Pages-R2-only.zip)，解压目录为Pages-R2-only；R2已核对匹配修订2完整SHA256，待作者上传完整网页包。以下旧交付路径保留阶段说明，现成EXE请使用根目录文件。

此前交付为[2.26.0安装器修订2](2.26.0/installer-revision2/AC7MultiSP_Installer.exe)：新版下载提示固定进入作者主页，版本保持2.26.0，运行文件、文档和图标不变。同目录保存重签的匹配ZIP、发行清单与签名，以及[配套网页包](2.26.0/installer-revision2/AC7MultiSP-Pages-homepage.zip)；网页只更新发行签名材料并保留旧签名。来源与摘要见[修订回执](../evidence/2.26.0-installer-homepage-revision2.json)。新EXE尚未上传，固定链接替换后需再次清缓存。

本次已制作2.26.0首版正式EXE（Release原文件已被外部移走，原冻结封装副本保留）及[同版通用ZIP](2.26.0/AC7MultiSP-2.26.0-Universal.zip)。来源38f1255，产品2.26.0、Windows文件版本2.26.0.0，candidate=false。[交付与静态核对](../evidence/2.26.0-exe-delivery.json)记录全部摘要与边界；同目录提供AC7MultiSP-release.json、AC7MultiSP-release.sig.json及SHA256SUMS.txt，最终文件不再改名、换图或重新封装。

四份中文正文保留作者原字节，仅同步对应英文；EXE、ZIP两个阅读目录和Mod运行目录均逐项核对。EXE内嵌ZIP与独立ZIP一致；安装器文件图标含Steam登记的游戏原图标四个尺寸，窗口读取自身图标。Vortex覆盖指令的40项载荷、dinput游戏根部署、2.26.0与原Nexus Mod 3797标识已静态核对，限Vortex 1.14+单Mod直接安装，Collection不在范围内。未运行安装器、Vortex、测试模块或游戏，未上传。

作者将同一个EXE上传R2和蓝奏云，把ZIP上传Nexus原Mod页面并填写版本2.26.0，说明正文使用docs/user的四组中英文。待提供最终EXE链接后，再核对下载摘要，接收指定网页线程最新成果并制作完整Pages供作者上传。stable的release_notes将逐字采用当前2.26.0章节全部正文，不使用旧16项整理稿；此刻不提前发布清单。

旧2.25.3 alpha1/alpha2/alpha3/alpha4/alpha5、2.25.2、2.24.3及其DPI修订EXE原样保留，不覆盖历史产物或回执。

旧2.24.2通用ZIP与EXE保留在E:/AC7_MultiSP/Release/2.24.2/，来源见[旧交付回执](../evidence/2.24.2-delivery.json)，不覆盖历史产物或验收记录。

旧正式2.24.1包位于主工作区E:/AC7_MultiSP/Release/AC7MultiSP-2.24.1.zip，对应[evidence发布回执](../evidence/2.24.1-release.json)，原字节保留。本目录仍保留旧恢复载荷的baseline-0.1.7.json清单。

旧ZIP、预览、解压目录、站点清单及安装器源码bundle已归档到E:/AC7_MultiSP/backup/artifact-archive-20261023/releases/，逐文件清单在上一级release-manifest.json；说明见[历史核心与发布包](../docs/archive/README.md#历史核心与发布包)。归档只是移出日常目录，没有释放这些文件占用的空间。

本轮制作本地正式产物，未运行测试、安装、启动游戏或上传。发布ZIP和EXE均不入Git；冻结源码、编译输入与详细回执留本机Temp任务目录，摘要入evidence。

EXE、ZIP和F6统一使用docs/user/中的四组双语正文，见[复用约定](../docs/project/player-docs.md)；不再编辑包内独有README或更新摘要。
