# Cloudflare Pages 双语主页与更新清单

本页维护主页结构、更新清单及资产来源；完整发布顺序、命令和人工分工只维护在[新版发布流程](release-workflow.md)。来源聊天为“mod内置更新”（01a15c1d-949a-7730-8a4a-34541a522225），接收时来源分支codex/pages-download-home停在7f213aa，主页是其未提交成果；已先保存原文件及差异快照，再从当前main精确接收。原来源树保持不动。

主页源文件位于site/，面向玩家提供浅色中英文下载入口和四个主题的玩家正文。游戏与安装器读取根目录stable.json；主页使用同一清单，不能各维护一份不同的“最新版本”。源码与网页源进入Git，本机上传ZIP、生成目录及备份不进入Git。

固定公开入口沿用现有代码中的地址：

- 主页：https://ac7multisp-updates.pages.dev/
- 更新清单：https://ac7multisp-updates.pages.dev/stable.json

## 内容来源

| 内容 | 唯一维护来源 | 生成或展示方式 |
|---|---|---|
| 功能、安装、更新日志、作者与授权 | docs/user/中四对TXT | 八份原字节复制到输出的user-docs/；正文不复制进HTML或JS维护 |
| 页面固定中英文文字 | docs/user/interface/site.zh-CN.json、site.en.json | 生成器核对同键，将词条填入输出app.js和HTML首次显示文字；不改生成表 |
| 页面结构、行为、浅色样式 | site/index.html、app.js、styles.css | index和app是生成模板；app的SITE_UI_COPY占位由生成器填充，不直接发布源目录 |
| 稳定版版本、摘要及下载入口 | site/stable.json | 原字节进入输出；主页、游戏、EXE共用此协议入口 |
| 历史更新器预览协议 | site/preview.json | 从已准备主页ZIP逐字节接收并保留；不是稳定版，也不是当前2.25系列候选 |
| 静态响应头 | site/_headers | stable.json和preview.json均保留JSON类型、no-store及现有安全响应头 |

新增或修订玩家内容只改docs/user/唯一正文；固定界面词条只改interface中的双语JSON，再生成站点。作者声明、署名链接和联系方式从同语言“作者与授权”正文读取，不另建主页作者配置。两种语言的页首声明均以用户正文为准，不自行改变授权范围。

主页采用浅色主题，菜单和提示均支持中文、英文切换。页首作者声明、Nexus Mods 与 bilibili 链接、邮箱和测试QQ群直接从“作者与授权”中英文唯一正文读取；链接文字显示平台和作者名，不展示整条网址。下载区按“主要下载（Cloudflare R2）”及“备用下载（蓝奏云）”分组，主链接必须恰好一个，备用链接可有一至三个。状态为 preparing 时只显示禁用的分组提示，不包含可点击的下载网址。

## 生成部署目录

从仓库根目录运行：

    python src/build_pages_site.py

默认输出到被Git忽略的Temp/tasks/pages-site/pages-output/。正式准备上传时建议显式指定本次全新目录：

    python src/build_pages_site.py --output Temp/tasks/日期-主页发布/pages-output

生成器检查stable协议、preview产品/通道、40对固定词条、正文编码及必需文件，填充模板并复制玩家正文。不会删除输出目录其他文件；因此发布时使用独立目录，避免夹带旧内容。部署对象是生成后的目录内容，不是源码site/，ZIP中不要额外套一层外部目录。

site/stable.json按作者2026-10-24的正式发布确认更新为2.25.2：R2固定EXE主链和本次蓝奏云EXE备用链。旧主页ZIP中的2.24.1清单仅作为历史原件保留。正式确认、R2下载核对和Pages实际部署分别见[发布准备记录](../../evidence/2.25.2-publication-preparation.json)；不能把候选号自动写入稳定清单。

R2主链接id固定为r2，备用id以lanzou开头且唯一。现行客户端仅登记wwbru.lanzoue.com、downloads.iot-node-service-api.online、ac7multisp-updates.pages.dev等指定主机；更换域名先核对游戏、EXE和主页三处消费者，不自行改写蓝奏云分享地址。R2为主入口，备用允许一至三个；preparing时不展示可点击下载按钮。

stable中的release_notes是当前可下载版本的中英文短摘要，按唯一玩家更新日志提取，不把全部开发日志另写进去。它描述下载包，不能因仓库正文有新条目就宣告新版已发布。preview是旧schema_version预览协议，当前记录2.22.0链路、install_enabled=false，保留原字节及状态，不把它改成stable格式或新候选。

site/_headers 为静态 JSON 设置 application/json 内容类型和 no-store 缓存，并为页面资源设置安全响应头。Cloudflare Pages 会读取发布目录中的 _headers 并将规则用于静态资源；若以后改为 Pages Functions，需要在函数响应中自行设置响应头，参见[Cloudflare Pages响应头说明](https://developers.cloudflare.com/pages/configuration/headers/)。

## 发布边界

A阶段源码候选由发布工具额外复制同版`AC7MultiSP-release.json`、`.sig.json`到站点根目录及`releases/发布ID/`。它们不替代stable/preview，当前公钥暂缓启用且未部署。后续整站发布必须携带以前已发布的签名目录；现有Direct Upload替换整站，不能让新目录丢失旧签名。具体密钥和制作状态只见[发布流程](release-workflow.md#作者签名与密钥保管)。

完整顺序见[新版发布流程](release-workflow.md#分工与顺序)。工具prepare复用有摘要回执的EXE和通用ZIP，finalize核对R2后生成整站及ZIP，verify-online在人工部署后核对线上文件。R2、蓝奏云、Nexus通用ZIP和Cloudflare Pages都由作者手动上传，本工具不操作外部账号。

本任务只准备本地上传材料，不更改线上Cloudflare Pages。直接上传Pages会用新目录替换整套线上文件；部署前需查看当前Production内容并保留仍在使用的其他清单或资源。现行源码保留stable与旧preview；若线上还有其他独有文件，核对并接收后再部署，不能用历史ZIP推定线上清单完整。

生成目录是预构建静态资源，线上上传按现有项目的 Direct Upload 方式执行；Cloudflare 的[Direct Upload说明](https://developers.cloudflare.com/pages/get-started/direct-upload/)要求上传构建输出目录。当前任务不触发上传。

历史操作细节见[Pages与R2发布参考](../archive/delivery/Pages与R2发布更新说明.md)，其中版本、包摘要和当时的线上状态仅属于原发布阶段，不代表今天已经部署。

## 现有资产及接收状态

| 位置 | 含义 |
|---|---|
| E:/AC7_MultiSP/Temp/tasks/20261024-release-2.25.2/ | 本次2.25.2准备材料；prepare/含EXE、Nexus通用ZIP及摘要，pages-release/含新整站ZIP和回执。完成状态以发布准备记录为准 |
| E:/AC7_MultiSP/Temp/tasks/20261024-updates-web-publish/AC7MultiSP-Pages-homepage.zip | 来源线程旧上传包，21,972字节；SHA256 d9ede927b156a57a0dfd3f70fb4efedee9eed29dfc6314cdf4251cc80b384ae5。本轮保持原字节，仅供追溯，不再用于发布2.25.2 |
| 同目录首页发布准备.json | 来源线程的原准备记录，包含成员、旧链接、preview摘要及尚未部署状态；保留原样 |
| 同目录homepage-site/、homepage-source/及AC7MultiSP-Pages.zip | 旧清单包与准备时的目录快照，供追溯；不作为另一份现行正文或可编辑主页源 |
| E:/AC7_MultiSP/backup/20261024-pages-homepage/ | 来源线程更新主页包之前的ZIP与准备回执备份 |
| E:/AC7_MultiSP/backup/pages-intake-20261024/ | 本轮接收前9份来源文件、未提交差异、状态、上传ZIP及准备回执快照；与主线接收差异分开保存 |

[本轮接收回执](../../evidence/pages-workflow-intake-20261024.json)记录精确文件摘要及差异。原主页ZIP中的功能说明与更新日志（中英文各一份）共四份正文落后于本轮main，不能用该ZIP反向覆盖当前文档。后续若需更新上传包，从main生成新目录、保留旧ZIP，再由当次发布任务交付。

本轮接收源码并准备2.25.2发布材料，实际生成、下载摘要及未完成项见发布准备记录。来源线程曾完成生成、JavaScript语法检查和本地HTTP预览，其结果属于原ZIP；本轮不据此宣称改组后浏览器排版或游戏联网已验证。
