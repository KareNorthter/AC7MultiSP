# 新版发布流程

本文是新版发布顺序和分工的唯一维护入口。包的内部结构见[交付方案](delivery.md)，主页结构与协议见[主页说明](pages-site.md)。玩家说明继续只维护[四个主题](player-docs.md)，不为Nexus、网盘、安装器和主页另写不同版本。

## 分工与顺序

| 顺序 | 工具或AI完成 | 作者手动完成 |
|---|---|---|
| 1. 确定版本 | 汇总本次改动，同步中英文正文，核对来源与版本 | 确认正式或候选；提供必要的实机反馈 |
| 2. 制作产物 | 获授权后完整构建、生成通用ZIP与内嵌同一ZIP的EXE；已有可信回执可直接复用 | 审核待发布版本，不以编译代替验收 |
| 3. 准备材料 | prepare校验回执，复制EXE、通用ZIP，生成摘要、清单草稿和操作说明 | 无需手工组包 |
| 4. 上传文件 | 不代操作外部账号 | 同一个EXE上传R2和蓝奏云；通用ZIP更新到Nexus |
| 5. 核对下载 | finalize流式下载R2并比对摘要，生成完整主页ZIP | 提供本次蓝奏云分享链接；从蓝奏云、Nexus下载核对摘要 |
| 6. 更新主页 | 提供完整Pages目录、ZIP、成员摘要与回执 | 上传到现有Cloudflare Pages项目，保留可回退部署 |
| 7. 发布后核对 | verify-online逐字节核对主页、清单及八份玩家正文 | 查看中英文页面、下载按钮及客户端提示，报告实际结果 |
| 8. 归档来源 | 将最终清单同步回site/stable.json，更新README、回执与工作区索引，按授权提交/打标签/推送 | 确认需要公开发布或上传GitHub的范围 |

文件先上传并核对，再更新Pages。Nexus上传通用ZIP；R2和蓝奏云上传同一份EXE。Nexus文件页独立于当前stable下载协议，不临时给协议增加第三种包入口。所有外部上传由作者操作；本地准备工具不登录、不上传、不安装、不启动游戏、不自动运行测试。

## 版本与文字的准备

正式状态只在根README维护。VERSION是产品版本源；产品元数据、构建和封装回执必须与本次目标一致。正式发布需作者确认，不能因候选包已生成就自动转正。发布后不移动旧正式标签，不改旧回执，不以重建覆盖旧文件。

自测版使用`X.Y.Z alphaN`且candidate为true，按完整标识单独记录中英文更新；同号正式版摘要不能从alpha章节冒取。自测源码可在获授权后构建并交给作者测试，但`prepare`拒绝alpha进入stable正式发布流程。验收后再确定正式版本、维护该版本的具体更新并重新制作正式产物，不能只改旧包名称、清单或回执来转正。作者要求正式日志移除alpha明细时，按[开发规范](workflow.md#更新日志与文档)汇总到指定正式章节，发布工具只提取正式章节；历史证据和旧包保持原样。

四份正文的中英文、固定界面词条须在构建前同步。2.25.2之后（从2.25.3起），发布摘要从唯一玩家更新日志对应的完整版本号条目提取；必须提供该版本中英文具体更新，缺少任一语言条目即停止，不回退到“主.次.X”系列摘要。2.25.2及以前复用原有系列段，不回改旧正文和发布记录。不再手写另一份技术流水账。正文改变后要重新构建才能进入新的EXE和通用ZIP；仅生成主页不会修改现成包的内嵌正文。

2.25.2的特殊来源：作者在2026-10-24明确确认它是正式发布，并提供两个EXE链接。现成EXE和通用ZIP来自此前候选构建，原回执与内嵌candidate标识原样保留；新发布确认另记，不虚构游戏验收，也不为改称呼重新编译。本次主页使用现行正文，包内文档仍是原冻结快照。后续新构建由版本整合任务统一正式/候选元数据。

## 作者签名与密钥保管

A阶段接入三种用途的签名，文件及消费者见[交付方案](delivery.md#a阶段作者签名)。最初密钥生成时Windows拒绝事后修改导出策略，用户曾暂缓启用。2026-10-24用户确认alpha4基本实机通过并要求接入保护、制作alpha5，现已复用当时保留的唯一CNG密钥，登记其公开部分。公钥BLOB的SHA256指纹为`7fb2b9911ea8ec785ea12fbaa051ec1d7c683046873e44f66f25c289d1142f2f`。本次不创建新钥、不读取恢复材料。新制作入口遇空登记或不匹配密钥即停止，不提供忽略验签或无签名回退；历史2.25.2原件复用特例仍按原摘要限制。

密钥设置属于独立、显式操作：`src/delivery/create_author_key.ps1 -BackupDirectory 仓库外全新目录`。构建/封装不会调用它。脚本已按实际限制改为保留可导出策略，不再尝试生成后关闭导出，也没有再运行。Windows当前用户CNG密钥存储负责日常私钥，代码通过NCrypt句柄签名，不把私钥参数、字节或密码写到命令行、源码、临时包或日志。可导出密钥仍可能被同账户具有权限的程序导出，不能声称硬件级保护或不可导出。

本次已产生的恢复材料保留在仓库外`E:/AC7MultiSP-Signing/author-20261024/`：`author-recovery.pfx`及`recovery-password.dpapi`，旁留`activation-status.json`记录用户暂停；目录ACL限当前用户和SYSTEM。前者用随机密码和AES256-CBC/SHA256、600000次口令派生加密，密码只经CurrentUser DPAPI保护保存。PFX里的自签证书仅为恢复容器，不安装到证书存储、不用于Windows发布者认证。最初流程中断在公钥登记之前，没有生成local-key/public-key文件；本次只枚举项目专用前缀的唯一CNG密钥并导出公钥。原activation-status.json及恢复件保持原样，新增activation-20261024-alpha5.json记录后续授权与公开指纹，不保存密钥名称。

这是本机加密恢复材料，尚无异机/离线恢复验收；DPAPI依赖Windows用户资料及其保护密钥，拷贝两个文件不保证换机可恢复。作者本次授权复用现有本机密钥出包，不等于异机恢复验收；后续另行确认离线恢复安排，不在聊天展示恢复口令或私钥。不得将`.pfx/.p12`、`.dpapi`、私钥PEM/DER、签名访问凭证或个人密钥名配置提交Git。公钥BLOB及其SHA256指纹可以公开，公钥登记应人工审阅后才进入源码，客户端不能从待核验包自行接收一个“新可信公钥”。

签名协议固定为RSA-PSS3072/SHA256、PSS盐32字节。签名输入为ASCII `AC7MultiSP.signature.v1`加NUL，再加清单原始UTF-8字节的SHA256；包络携带schema、固定algorithm、完整公钥BLOB指纹key_id及Base64签名。各用途独立检查purpose、版本、通道、发布ID；公钥指纹仅在已登记集合中查找。算法和上限不能由下载包自行放宽。采用Windows官方[NCryptSignHash](https://learn.microsoft.com/en-us/windows/win32/api/ncrypt/nf-ncrypt-ncryptsignhash)及[BCryptVerifySignature](https://learn.microsoft.com/en-us/windows/win32/api/bcrypt/nf-bcrypt-bcryptverifysignature)，不自写RSA算法。

启用后的顺序为：完整构建并精简交付DLL→content签名→通用ZIP→全部EXE资源及bundle签名→EXE编译→最终EXE/ZIP的release签名。最终文件签署后不可再改资源、版本、精简、混淆或代码签名；将来增加Authenticode时须放在release摘要之前。任何一步失败保留输出，不覆盖旧成品或旧回执。`build_setup.ps1`如果已编译但最后签名失败，该目录仍不可发布。

新构建可通过`--signing-key-name`/`--signing-provider`显式选当前账户已有密钥；省略名称时按已登记公钥匹配本项目名称前缀的唯一密钥，多个或没有匹配就停止。私钥名称不是密钥本体，但也不作为个人配置写入仓库。安装器中的公钥由封装器生成，日常作者文案仍只改双语唯一正文，再完整构建/封装，不单独改内嵌文本后补同名旧签名。

签名版prepare复制EXE、ZIP和两份发布签名原件，拒绝用`--exe-only`拆开已签名文件集合；finalize验证它们后把签名材料放进Pages根目录和`releases/发布ID/`，不改stable/preview协议。后续使用`finalize --previous-pages-dir 上一版完整Pages目录`验证并保留旧`releases/`签名；只复制签名材料，不复制旧站点代码。未提供旧目录会在发布回执记录未提供，人工上传前须核对有无历史材料，不把仅含当版文件的目录直接覆盖上线。发布签名不包含经常变化的分享网址；现有HTTPS/域名限制仍独立执行。

保护研究阶段仅做静态核对；alpha5按本次授权完整编译，制作时核对三用途签名、封装字节及符号精简的加载契约。实际完成项以alpha5交付回执为准；篡改/丢失签名、公钥替换、双语启动失败、安装事务、Vortex部署及实际耗时/内存仍须按分别获得的授权验证，不能把正常出包验签视为全部异常分支通过。

## 工具入口

统一入口为`src/release_workflow.py`，需要Python 3.11及以上和Git；显式build还需要[开发依赖](../engineering/development.md)。从仓库根目录运行，输出指定到Temp/tasks/日期-发布主题/下的全新子目录。目录已存在会停止，不覆盖旧产物或删除文件。

### 需要重新构建时

仅在用户授权完整编译、打包时执行：

    python -X utf8 src/release_workflow.py build --mount-assets-dir "实际挂载原料目录" --output-dir Temp/tasks/日期-发布主题/build --audio-source E:/AC7_MultiSP/dist/DualSP/Audio/SP2.wav

它依次调用完整游戏构建、通用封装和EXE编译，并生成delivery-receipt.json。编译、封装、测试、安装、游戏实测仍分别记录。工具不会跳过原有冻结校验；旧构建与当前源码不一致时，应使用原交付回执复用已有成品，或经授权新建完整构建，不能修改旧回执应付校验。

### 复用现成产物，准备三处上传文件

本次2.25.2命令：

    python -X utf8 src/release_workflow.py prepare --receipt evidence/2.25.2-exe-delivery.json --confirm-stable --output-dir Temp/tasks/20261024-release-2.25.2/prepare

后续版本把receipt换成对应交付回执或上一步delivery-receipt.json。`--confirm-stable`表示操作者已获得正式发布确认，不是游戏验证通过。默认从site/stable.json读取R2与蓝奏云地址；新版本必须显式提供`--lanzou-url 实际分享链接`，可重复一至三次，避免沿用旧包链接。R2更换地址用`--r2-url 实际地址`，域名仍须在已支持范围内。

输出包含：

- upload/AC7MultiSP_Installer.exe：上传R2与蓝奏云的同一个文件。
- upload/AC7MultiSP-版本-Universal.zip：作者上传到Nexus。默认必需，来源与EXE内嵌ZIP一致。只有明确本轮不更新Nexus文件时才使用`--exe-only`。
- SHA256SUMS.txt：两种发布文件的摘要。
- 签名版另含upload/AC7MultiSP-release.json及AC7MultiSP-release.sig.json，绑定最终EXE/ZIP；下载文件核对必须从可信作者入口取得核验器或公钥，不能仅信同包自述。
- stable.ready.json：清单草稿，不提前上传。
- release-plan.json、手动发布步骤.txt：版本、来源、对象键、摘要和手工步骤。

### 作者上传R2、蓝奏云和Nexus

R2固定对象键为`AC7MultiSP_Installer.exe`，下载地址为`https://downloads.iot-node-service-api.online/AC7MultiSP_Installer.exe`。可先将同一文件另存到`releases/版本/AC7MultiSP_Installer.exe`，再替换固定入口；保留上一版文件及旧Pages部署用于回退。固定入口被替换后，旧网页缓存可能短暂仍显示旧版本，因此先核对真实下载，再及时更新Pages。

蓝奏云上传同一EXE，取得本次真实分享地址。已登记的2.25.2备用地址为`https://wwbru.lanzoue.com/ig8Aj4d59fqd`；以作者本次实际上传结果为准，不把“分享页打开成功”视为文件已核验。蓝奏云链接常变，不写进多份说明；最终通过清单统一提供。

Nexus由作者在本Mod的Files页上传通用ZIP，设置正确的版本和文件说明；Requirements与安装说明沿用现有通用ZIP方案。更新内容从对应中英文玩家更新日志取用，功能、安装及作者授权也复用唯一正文。不要把Pages ZIP当作Mod包，不用旧版ZIP冒充本次版本。上传后从Nexus下载文件核对SHA256SUMS.txt，保留旧版本或按平台方式归档。当前免费账户，网页登录不代表API、Vortex或自动上传已授权。

### 准备Cloudflare Pages整站包

R2文件已到位后运行：

    python -X utf8 src/release_workflow.py finalize --prepared-dir Temp/tasks/20261024-release-2.25.2/prepare --output-dir Temp/tasks/20261024-release-2.25.2/pages-release

如果蓝奏云地址刚改变，加`--lanzou-url 实际分享链接`；多条重复传入。工具核对本地准备文件没有改变，流式下载R2固定地址并核对大小和SHA256，再从当前受管源码生成完整站点。网络受阻或远端文件不一致即停止，不反复重试、不伪造核验。

网络只能由作者访问时，可由作者实际下载R2文件后提供真实摘要，使用`--manual-r2-sha256 实际下载摘要`继续；不能把本地预期摘要直接填进去冒充远端核验。

输出`AC7MultiSP-Pages-homepage.zip`、pages/目录、stable.json及publication-receipt.json。Pages包根目录包含HTML、JS、CSS、stable.json、保留的preview.json、_headers和八份玩家正文，不额外套目录。作者先确认蓝奏云和Nexus文件，再将完整ZIP或pages/内容上传现有Pages项目。单独上传stable.json会替换不完整站点；源site/app.js含模板占位，也不能直接上传源码目录。

### 部署后核对和收尾

    python -X utf8 src/release_workflow.py verify-online --pages-dir Temp/tasks/20261024-release-2.25.2/pages-release/pages --receipt Temp/tasks/20261024-release-2.25.2/online-verification.json

它逐字节核对线上五个核心文件与八份正文，拒绝覆盖旧核对回执。_headers不是公开下载文件，另检查stable.json与preview.json的JSON类型和no-store响应头；页面双语显示、跳转、浏览器下载、游戏/EXE联网提示仍按实际操作记录，不能用字节相等替代。

将最终pages-release/stable.json原样同步回唯一源码site/stable.json后提交；若使用了临时蓝奏云链接尤其不能遗漏。将本次准备与实际部署分开记录，正文索引不再新增逐轮总结。GitHub提交、正式标签和推送按当次授权；发布包、生成目录、原料和凭证不进源码仓库。失败回退先恢复上一版R2固定入口，再恢复对应Pages部署，不把新版本清单配旧文件。

## 本次材料与状态

本机准备根目录：`E:/AC7_MultiSP/Temp/tasks/20261024-release-2.25.2/`。实际完成项见[2.25.2发布准备记录](../../evidence/2.25.2-publication-preparation.json)。该记录与原EXE交付回执并列保存；作者后续手动上传的结果另行补充新记录，不回改原始构建事实。
