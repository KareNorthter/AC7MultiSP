# 本地发布产物

当前正式功能版本和发布状态只在[项目入口](../README.md)维护；主要变化见[玩家更新日志](../docs/user/更新日志.txt)。

本次交付[2.25.3 alpha1自测EXE](2.25.3-alpha1/AC7MultiSP_Installer.exe)，完整构建核心、UI和双语安装器，产品显示版本与Mod均为2.25.3 alpha1，Windows文件版本为2.25.3.1。[交付回执与待测清单](../evidence/2.25.3-alpha1-exe-delivery.json)记录来源、摘要和边界。已核对EXE内嵌ZIP原字节、两种语言各四份说明及运行载荷；内部ZIP留Temp，不单独复制到Release。未安装、运行游戏或上传，正式发布仍为2.25.2。

旧2.25.2、2.24.3及其DPI修订EXE原样保留，不覆盖历史产物或回执。

旧2.24.2通用ZIP与EXE保留在E:/AC7_MultiSP/Release/2.24.2/，来源见[旧交付回执](../evidence/2.24.2-delivery.json)，不覆盖历史产物或验收记录。

旧正式2.24.1包位于主工作区E:/AC7_MultiSP/Release/AC7MultiSP-2.24.1.zip，对应[evidence发布回执](../evidence/2.24.1-release.json)，原字节保留。本目录仍保留旧恢复载荷的baseline-0.1.7.json清单。

旧ZIP、预览、解压目录、站点清单及安装器源码bundle已归档到E:/AC7_MultiSP/backup/artifact-archive-20261023/releases/，逐文件清单在上一级release-manifest.json；说明见[历史核心与发布包](../docs/archive/README.md#历史核心与发布包)。归档只是移出日常目录，没有释放这些文件占用的空间。

本次仅制作本地产物，未安装、启动游戏或上传。发布ZIP和EXE均不入Git；冻结源码、编译输入与详细回执留本机Temp任务目录，摘要入evidence。

EXE、ZIP和F6统一使用docs/user/中的四组双语正文，见[复用约定](../docs/project/player-docs.md)；不再编辑包内独有README或更新摘要。
