# 本地发布产物

当前正式功能版本和发布状态只在[项目入口](../README.md)维护；主要变化见[玩家更新日志](../docs/user/更新日志.txt)。

作者已确认alpha5正常，并指定正式版本2.26.0。当前已整理版本标识和双语正式更新日志，尚未制作2.26.0正式EXE/ZIP或上传；[验收与版本确认](../evidence/2.26.0-release-authorization.json)记录边界。以下旧产物仍保持原版本与摘要，不能改名作为正式包。

最近已交付并获作者正常反馈的是[2.25.3 alpha5自测EXE](2.25.3-alpha5/AC7MultiSP_Installer.exe)，完整构建核心、UI和双语安装器，显示2.25.3 alpha5，Windows文件版本2.25.3.5。[交付回执与四项待测清单](../evidence/2.25.3-alpha5-exe-delivery.json)记录来源db0d2c9、摘要和边界。接入作者签名及安装前发行文件核对、交付DLL符号精简和安装器/F6作者区的主页/咖啡按钮；同步作者最新中文说明与对应英文。三用途签名、内嵌ZIP、八份说明、DLL加载数据均在制作阶段完成核对；工具当时未运行安装器/测试、安装、游戏或上传，之后作者确认正常。内部ZIP和最终发行签名原件留alpha5的Temp，未单独复制到Release。2.26.0正式包仍待制作。

旧2.25.3 alpha1/alpha2/alpha3/alpha4、2.25.2、2.24.3及其DPI修订EXE原样保留，不覆盖历史产物或回执。

旧2.24.2通用ZIP与EXE保留在E:/AC7_MultiSP/Release/2.24.2/，来源见[旧交付回执](../evidence/2.24.2-delivery.json)，不覆盖历史产物或验收记录。

旧正式2.24.1包位于主工作区E:/AC7_MultiSP/Release/AC7MultiSP-2.24.1.zip，对应[evidence发布回执](../evidence/2.24.1-release.json)，原字节保留。本目录仍保留旧恢复载荷的baseline-0.1.7.json清单。

旧ZIP、预览、解压目录、站点清单及安装器源码bundle已归档到E:/AC7_MultiSP/backup/artifact-archive-20261023/releases/，逐文件清单在上一级release-manifest.json；说明见[历史核心与发布包](../docs/archive/README.md#历史核心与发布包)。归档只是移出日常目录，没有释放这些文件占用的空间。

本轮只整理2.26.0版本与正文，未制作新包、运行测试、安装或上传。发布ZIP和EXE均不入Git；冻结源码、编译输入与详细回执留本机Temp任务目录，摘要入evidence。

EXE、ZIP和F6统一使用docs/user/中的四组双语正文，见[复用约定](../docs/project/player-docs.md)；不再编辑包内独有README或更新摘要。
