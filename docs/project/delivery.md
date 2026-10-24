# 通用ZIP与双语EXE交付

本文维护交付格式、程序入口和验证边界。玩家步骤只维护[中文安装说明](../user/安装说明.txt)和[英文安装说明](../user/Installation.txt)，文字复用见[玩家文档维护](player-docs.md)。安装器版本完全跟随产品版本，正式与候选状态以[项目入口](../../README.md)为准；旧安装器及回执保留原样。制作范围按当次授权；仅交付EXE时，封装所需ZIP保留在Temp，不单独复制到Release。

新版发布顺序、自动准备命令及人工上传统一按[新版发布流程](release-workflow.md)维护；主页协议见[主页说明](pages-site.md)。R2与蓝奏云使用同一EXE，Nexus由作者手动更新同版通用ZIP，之后上传完整Pages包。不能把本地候选制作完成推定为正式确认、远端上传或主页已更新。

安装器从`a99a404`精确接收必要`src/delivery/`及原`src/package_delivery.py`后按本方案改造，原独立树保持。文案修订后的候选制作结果以各阶段回执记录，不能把旧安装器反馈、封装完成或EXE编译当作本候选已安装/实测。本轮未运行`tests/run.py`、未安装或启动游戏、未上传。

## 一套通用ZIP，三种安装方式

- Vortex和手动方式共用`AC7MultiSP-版本-Universal.zip`；EXE内嵌该ZIP原字节，导出时不重新组包。根目录按游戏相对路径组织为`ue4ss/`、`Game/`及包元数据，不使用旧GameFiles结构。
- 主程序、界面、18份Lua和专用PAK同版成套。21个运行文件为18 Lua、2 DLL及VERSION，挂载资源仍为77份/154成员；音频、许可证、独立启用文件和文档另列封装清单。
- 通用ZIP不含共享UE4SS、微软运行库、个人配置、日志、存档或其他Mod；`DualSP/enabled.txt`独立启用，不要求改写共享mods.txt。
- 四对玩家TXT原字节进入ZIP及EXE对应语言文档；F6只展示更新日志和作者与授权。游戏与安装器固定文字分别来自`docs/user/interface/game.*.json`、`installer.*.json`，不在各展示位置另写副本。安装器首页的短声明、作者署名链接及反馈由同一作者与授权TXT提取，作为`installer/credits-compact/{zh-CN|en}.json`校验资源嵌入EXE，不把网址或邮箱抄入C#。

ZIP根目录另提供`ReadMe_EN/`与`ReadMe_zhCN请读我/`，各四份正文，替代根目录散放的两份安装说明。英文源与包内文件名统一为`Features.txt`、`Installation.txt`、`Changelog.txt`、`Author-and-License.txt`；中文保留原文件名。根目录阅读副本不进入Vortex游戏部署映射，运行目录docs及EXE内嵌正文仍使用同一份来源原字节；历史ZIP和旧回执不回改。

## A阶段作者签名

作者在确认alpha4基本实机通过后，授权接入保护并制作2.25.3 alpha5。本轮复用先前保留的唯一CNG密钥，仅导出并登记RSA3072公钥，不创建新钥、不读取恢复件。接入作者签名、安装前核验和DLL交付副本精简；构建与核对结果由alpha5交付回执记录，不能视为安装或游戏验收。旧正式包仍保留原字节。B阶段游戏/F6运行期核验未接入，安装后手工修改文件不会被游戏主动阻止。

密码学格式和密钥保管的唯一说明见[发布流程的作者签名](release-workflow.md#作者签名与密钥保管)。`src/author_signing.py`通过Windows CNG使用固定RSA3072/PSS/SHA256、32字节盐，先核验签名，再读取清单列明文件；公钥固定生成到安装器，不接受包中自带的新公钥作为信任依据。

| 签名用途 | 最终字节及位置 | 消费者 |
|---|---|---|
| content | ZIP中`ue4ss/Mods/DualSP/author-content.json`及`.sig.json`，绑定同版Mod载荷；自身不入自己的摘要集合 | 随Vortex映射或手动部署；EXE校对它与安装载荷的范围及摘要 |
| bundle | EXE内`bundle.json`及`bundle.sig.json`，逐项绑定散文件、共享前置、通用ZIP、双语正文/短署名及UI JSON | `AuthorSignature.Initialize`在正常界面读词条前验签；首次失败用同一JSON生成的双语最小提示，避免资源验证循环 |
| release | 最终EXE及通用ZIP旁`AC7MultiSP-release.json`及`.sig.json`，不放进被签名的EXE/ZIP造成自引用 | 可信安装器的`--verify-release 文件夹`只读核对，或维护者使用`author_signing.py verify-release --files-dir 文件夹` |

三种清单均限制64KiB、256成员和1GiB总载荷，每成员不超过256MiB；签名包络限制2KiB。固定用途域与原始UTF-8字节一起做SHA256，不把JSON重排后验签；拒绝重复键、越界/非法路径、大小写冲突、未知算法/公钥及版本/用途混用。新清单绑定完整版本、candidate/stable、发布ID及Git来源锚。Git锚不替代构建回执中实际源码摘要。

`VerifyAllResources`在安装、卸载、恢复、应用及导出入口写盘前完整核对内嵌资源，每安装器进程只完成一次；后续读取资源仍校对本项摘要。先核对发行载荷，不拿目标目录中玩家自改的音频、配置、运行状态或其他Mod作盗版判定；目标的备份/迁移/管理器拒绝规则继续沿用原事务。Vortex不会自动执行作者验签。客户端完全被替换时仍可能伪造本地结果，外部可信来源不可省略。

`src/distribution_symbols.py`先把新构建两DLL复制到`private-debug/`，再用同工具链`llvm-strip --strip-all`精简交付副本（COFF不使用不支持的keep-section参数，保留[LLVM的COFF处理](https://github.com/llvm/llvm-project/blob/main/llvm/lib/ObjCopy/COFF/COFFObjcopy.cpp)所述加载节并继续逐字节核对），静态比较必需导出、映射节字节、异常/重定位/TLS等加载契约；不一致即停止，不恢复覆盖旧核心。私有诊断件摘要和精简工具摘要进入新构建回执，私有DLL不进入通用ZIP。它们随本机构建保留；精简结果由构建回执记录，加载契约的静态一致不等于实际游戏加载或性能已验证。

## 安装器游戏图标

2.26.0起，构建与封装显式传入`--icon-source 本机ICO路径`。本轮从Steam App 502500登记的DisplayIcon读取《皇牌空战7》原图标，原字节冻结到本轮`setup-source/app.ico`，通过`/win32icon`编入EXE；主窗体读取自身关联图标。ICO范围、来源及摘要进入封装回执，编译前后核对冻结输入，图标必须在最终release签名前编入，不能签名后换图。

游戏ICO不进入Git或通用ZIP，只作为本机游戏资源保留在本轮Temp。恢复源码不等于恢复图标原料，缺失时构建前即停止，不用另一图标冒充。

## 生成、冻结与封装入口

| 阶段 | 入口与输入 | 输出和核对 |
| --- | --- | --- |
| 游戏文字生成 | `src/user_texts.py`读取四对TXT及游戏双语JSON；`src/build.py`自动调用 | 生成C++头文件与Lua，检查双语键、占位符、NUL及ImGui标识；输出不手改 |
| 完整游戏构建 | `src/build.py`，必须显式`--mount-assets-dir`，独立树可指定`--temp-root E:/AC7_MultiSP/Temp` | 独立目录内核心/UI、Lua来源、PAK及构建回执；冻结游戏源码和文字，不冻结安装器C#/JSON |
| 唯一封装 | `src/package_delivery.py --build-dir 完整构建目录 --output-dir 全新目录 --audio-source E:/AC7_MultiSP/dist/DualSP/Audio/SP2.wav --icon-source 本机ICO路径` | 核对完整构建及生成文字，回读通用ZIP各成员，生成EXE资源、产品版本属性、manifest、冻结C#和`封装回执.json`；不依赖旧手装ZIP |
| 双语EXE编译 | `src/delivery/build_setup.ps1 -InputDirectory 封装目录`，可指定`-SigningKeyName`/`-SigningProvider` | 编译前验证bundle和冻结资源，编译前后核对源码/响应文件/ZIP；生成EXE及独立回执，再给最终EXE/ZIP生成release签名。任一步失败均不可发布 |

封装使用全新输出目录，已存在即拒绝覆盖。提示音核对原验收来源摘要，不分发个人替换音频；原始资源、旧构建、旧包与历史回执保持。

封装新增可选`--signing-key-name`及`--signing-provider`。省略名称时，只在当前用户CNG存储中查找本项目名称前缀，并且必须与登记公钥匹配且唯一；不自动生成密钥。无登记/匹配时停止。安装器源与签名工具、公钥、全部词条共同冻结；生成的`TrustedAuthorKeys.cs`和`SignatureBootstrap.cs`只存在本次封装快照，不另设可编辑翻译来源。封装期间不改变原同源TXT字节。

游戏来源冻结已改用`docs/user/`，不再缺失旧`docs/授权.txt`。安装器封装另冻结`src/delivery/`、封装脚本及玩家文字，编译仅使用封装目录的快照。`src/package-release.py`是新封装入口的薄包装；`src/release_package.py`只保留历史helper，旧GameFiles命令行已停用。完整工具依赖见[开发与恢复](../engineering/development.md)。

安装器没有独立版本源。封装从产品版本生成`setup-source/ProductVersion.cs`的程序集、文件及信息版本，并替换manifest的版本占位符；生成文件与其摘要纳入冻结源码，仍由编译入口核对。正式版三段数字追加`.0`作为Windows四段属性；自测版`X.Y.Z alphaN`必须标记candidate，显示、信息版本和载荷保留完整标识，Windows四段属性使用`X.Y.Z.N`，各段不超过65534。版本顺序由产品语义判断，不能按该四段属性决定升降级。

安装器`ReleaseVersion.cs`统一比较安装目标、旧安装和更新提示：先比较三段基号，同号按alpha序号递增，正式版晚于所有同号alpha。因此可从2.25.2升到2.25.3 alpha1，再升到正式2.25.3；拒绝从正式2.25.3降为同号alpha。stable更新清单继续只接受正式三段版本，不因本地使用alpha而放宽远端校验。`settings.json`不再维护`delivery_version`；兼容清单字段由产品version派生，显示与更新User-Agent也使用同一产品版本。旧回执中的独立版本只表示当时事实。

## Vortex部署映射

保留现有《皇牌空战7》游戏支持扩展，省略自制“AC7MultiSP安装支持”扩展。旧扩展源码与研究成果继续保留，不是新包必需组件。

封装器在通用ZIP生成`vortex_override_instructions.json`，指定`setmodtype=dinput`，使用Vortex内置Engine Injector把文件部署到游戏根目录。映射逐项覆盖程序、Lua、提示音、文档、许可证、启用文件及专用模型PAK；不能把整包误当普通PAK包，也不能只删扩展名称而省略映射。

按[Vortex官方覆盖指令规范](https://github.com/Nexus-Mods/Vortex/wiki/File-Based-Override-Instructions)，该机制从Vortex 1.14.0支持。静态核对要求每个Mod载荷恰有一条源/目标一致的copy、路径无越界/大小写冲突，标明2.26.0、Nexus Mod 3797及游戏ID；版本更新用同一Mod页的新文件，由Nexus提供新fileId，不能在包里预填尚未上传的fileId。包的阅读目录、共享UE4SS及玩家配置不进入部署映射。下载、版本检查、更新、启用、停用和部署由Vortex负责。路线限定单个Mod直接安装；Collection依赖安装可能不应用包内覆盖指令，不在支持范围内。Vortex/手动方式由玩家先准备已核验的AC7专用UE4SS，Requirements只是依赖提示，不保证自动安装、运行中部署拦截或未知共享加载器处理。

旧手动安装转Vortex需先移出旧程序及专用PAK、保留个人配置，避免停用时恢复旧手装文件；不能用覆盖ZIP替代此迁移。玩家的具体步骤以唯一安装正文为准。

## EXE前置与旧安装处理

EXE另内嵌固定来源的AC7专用UE4SS，可在空环境准备前置，或共用核验兼容的已有版本；这些共享文件不进入导出的通用ZIP。加载器摘要与策略在`src/delivery/settings.json`维护，不因存在同名DLL就认定兼容。

旧安装识别与事务实现位于`src/delivery/setup/Bundle.cs`：按来源清单备份将改动的文件及profiles，移走新版废弃的已知程序，保留个人配置和自定义提示音。未知额外脚本、被外部修改的废弃程序、管理器托管文件或未知加载器须停止并给出对应语言提示，不能递归清空旧DualSP目录。

失败时按事务记录尝试恢复；卸载只处理自身记录的产品文件，保留个人配置与共享前置。双语GUI、文档页、确认/进度/错误和导出界面已按新文字源接入，但实际操作及异常分支仍需验证，不能沿用旧预览结论。

## 安装器界面与更新状态

- 首页仅显示直接安装、导出通用ZIP两种方式，下方使用无滚动条的紧凑作者和反馈区；作者的两个网页只显示可点击署名，不显示长网址。首页不放四份文档标签或右上角作者全文入口。直接安装与导出结果页继续显示完整文档。
- 右上角下拉框左侧固定显示`语言/Language`，默认“跟随系统”，读取当前Windows界面语言；中文系统显示简体中文，其他系统显示英文，也可手选中文或英文。
- 单EXE在manifest内声明系统DPI感知，主窗体及确认框以96 DPI为设计基准，在启动时按Windows实际缩放直接绘字，避免125%等缩放交给系统位图拉伸。首页自绘按钮的测量、留白与警告行使用同一DPI比例；首次显示由WinForms缩放，后续切页/语言仅缩放新控件一次。此方案不附带外置exe.config；采用系统DPI模式，更换到不同缩放的显示器或修改系统缩放后应重新打开安装器，不宣称逐显示器动态重排已验证。模式依据见[微软DPI说明](https://learn.microsoft.com/en-us/windows/win32/hidpi/setting-the-default-dpi-awareness-for-a-process)。
- 导出按钮说明额外加粗提示：本Mod同时含UE4SS组件和PAK，不支持CreeperUX启动器安装，中英文同步。
- 产品版本附近显示等待、检测中、失败、已最新或有新版状态，并提供手动检测。启动后台检测完成后只更新该区域，不切页、不弹窗、不重建正在阅读的正文；仅在清单有可用新版时显示可点击下载链接；该链接固定打开同源作者入口AuthorLinks.Homepage（https://ac7multisp-updates.pages.dev/），由主页提供主备下载，不再跳转Nexus或更新清单中的直链。该修订按作者要求保持2.26.0，旧签名产物保留，新产物在独立修订目录重签。沿用HTTPS、状态/产品/通道、三段版本、响应时间/大小和下载白名单等清单校验。
- 点击安装/更新/重装时若检测到游戏仍在运行，先提示未保存进度可能丢失。仅用户选择“关闭游戏并安装”后，才尝试关闭`Ace7Game`与`Ace7Game-Win64-Shipping`：先请求窗口退出，3秒仍未退出则结束该进程并再等待3秒。取消或关闭失败都不进入安装事务；卸载仍要求用户自行完整退出。计划、恢复及逐项写入前的`Closed()`检查继续保留，防止确认后游戏重新启动。

源码静态审阅及编译不等于实际关闭进程、启动安装器、联网检测或验证排版；具体完成范围由该次交付回执记录，未执行项不能表述为已通过。

## 手动方式边界

手动方式先准备正确的AC7专用UE4SS，再按安装正文合并通用ZIP中的游戏文件。旧版程序应先移出备份，再恢复需保留的个人配置；配置读取/迁移由游戏中的Mod处理，封装不提供个人配置或替换其他Mod的共享设置。

通用ZIP的封装清单和构建来源回执用于识别载荷，不代表该电脑已安装、已迁移配置或已实测。新EXE的安装记录与旧`src/install.py`历史回执分开解释，不能相互冒用。

## 待验证范围

2026-10-22的旧2.23.1 Vortex测试包已有无自制配套扩展的首装、根目录部署、更新、入游戏、停用、旧手装迁移和重新启用记录，最终31个部署文件匹配。该记录仅证明当时路线，不能代替新通用ZIP或双语EXE验证；旧完整卸载、Collection、自动依赖首装也没有因此通过。来源见[本机资产索引](../archive/local-assets.md)所列Vortex现场。

新候选仍须分别确认：

1. 构建、通用ZIP成员、来源摘要和EXE内嵌/导出ZIP一致；完成结果写各自回执，不把封装回读称作游戏测试。
2. 游戏F6与原生选装页的双语文字、语言切换、排版、错误分支和四份正文复用；实际功能及游戏性能不因文字生成通过而视为验收。
3. Vortex无自制扩展时的新包部署位置、更新、启停、卸载及旧手装迁移；保留个人配置和其他Mod，不以Collection替代此入口。
4. EXE空环境前置、已有兼容前置、可识别旧安装更新、管理器/未知文件拒绝、备份与失败恢复、卸载及双语交互。
5. 手动安装的文件范围、旧程序隔离与个人配置恢复；线上清单、Nexus、Pages/R2或其他上传发布另行记录，不由本地包生成推定完成。

旧独立分支的交付设计、Vortex扩展和预览产物作为来源保留；其中配套扩展必需、依赖旧手装ZIP、旧GameFiles入口及重复正文要求均不再适用本轮新交付。
