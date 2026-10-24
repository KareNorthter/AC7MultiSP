# Pages 与 R2：既有交付参考

本说明形成于**2.24.1原发布阶段**，保留当时的包摘要、清单格式和手动操作方法，供后续交付重做参考。文中“当前”“本次”均指该阶段；本轮整理未复核线上服务或本机安装，也未执行下述操作。实时状态不能由本文推定，已知项目状态统一见[项目入口](../../../README.md)。

现行主页与清单见[主页说明](../../project/pages-site.md)，发布顺序及自动工具见[新版发布流程](../../project/release-workflow.md)；本页仅用于历史查证，不再作为另一套日常发布说明维护。


本次本地发布包：`E:/AC7_MultiSP/Release/AC7MultiSP-2.24.1.zip`，大小 28,348,417 字节。上传前后的文件应具有相同 SHA256：

```text
b09f1300d67840a54a06bdea5f0e5a1f1bf859a6f350f623bd63742faee1146f
```

## 一、各处分别放什么

| 位置 | 放置内容 | 当前使用的位置 |
| --- | --- | --- |
| 蓝奏云 | 玩家下载的完整 Mod ZIP | 上传后取得本次文件的新分享链接 |
| R2 | 同一份完整 Mod ZIP | 当前已见桶名 `ac7multisp-update-preview`，请核对其公开域名绑定 |
| Pages | 告诉游戏版本号、更新说明和下载链接的 `stable.json` | 项目 `ac7multisp-updates`，固定入口 `https://ac7multisp-updates.pages.dev/stable.json` |

本次发布包为 `AC7MultiSP-2.24.1.zip`，不附带 DirectX/VC 运行库。蓝奏云和 R2 应上传**同一个 ZIP**，不要把 Vortex 测试包、旧预览包或游戏目录重新压缩后混用。

发布顺序：**先上传 ZIP → 确认两个链接都能下载 → 修改 stable.json → 部署 Pages → 进游戏检查。**

## 二、上传到 R2

1. 登录 Cloudflare，进入 **Storage & databases → R2 Object Storage**，打开存放本 Mod 的桶。当前账户中已见 `ac7multisp-update-preview`，桶名带 preview 不影响放置版本独立的新文件，无需为此新建桶。
2. 先在桶的 **Settings → Custom Domains** 核对公开下载域名是否为 `downloads.iot-node-service-api.online`，且处于可用状态。这里应使用玩家可访问的公开域名，不是 S3 API Endpoint 或控制台地址。
3. 返回 **Objects**，在桶根目录选择 **Upload**，上传 `AC7MultiSP-2.24.1.zip`。不解压 ZIP。等待上传成功提示。[Cloudflare 上传说明](https://developers.cloudflare.com/r2/objects/upload-objects/)
4. 保留旧版本对象。新文件使用新版本号文件名，避免覆盖旧包后遇到缓存或回退困难。
5. 如果域名绑定正确，且对象确实位于桶根目录、名称与上述一致，公开地址应为：

   `https://downloads.iot-node-service-api.online/AC7MultiSP-2.24.1.zip`

   这是上传后的预期地址，本轮没有替你上传或验证它已存在。若上传到了子目录，链接也必须包含该目录。
6. 用浏览器打开实际公开地址，下载一次，确认得到本次 ZIP，而不是错误页面或旧包。将下载文件与本地发布包的 SHA256 对比；下方给出核对方法。

## 三、上传到蓝奏云

1. 把同一份 `AC7MultiSP-2.24.1.zip` 上传到蓝奏云。
2. 复制**本次文件**的公开分享页链接，打开确认文件名和版本正确。不要复用仍指向旧包的分享页。
3. 目前客户端支持的蓝奏云主机是 `wwbru.lanzoue.com`。如果得到的是其他子域名，请先确认客户端支持再发布；不要自行改写分享链接的主机名。
4. 若设置提取密码，在更新说明中明确写出；客户端只打开浏览器分享页，不会自动填写密码或下载文件。

## 四、填写最新 stable.json

旧清单使用 `schema_version`，而游戏内更新页要求 **`"schema": 1`**。此前日志中的 `key 'schema' not found` 就是在读取该字段时失败。发布时应替换为下面的完整结构，不能只在旧清单中添加一个 schema 字段而保留不兼容的其余字段。

把下面内容保存为 UTF-8 的 `stable.json`。先将 `REPLACE_WITH_NEW_SHARE_ID` 替换为真实蓝奏分享路径，并核实 R2 URL。**含占位符的模板不要部署为 ready。**

```json
{
  "schema": 1,
  "product": "AC7MultiSP",
  "channel": "stable",
  "status": "ready",
  "latest_version": "2.24.1",
  "release_notes": {
    "zh": "新增版本和更新页、手动检测更新、作者联系方式及双语授权。通知统一在屏幕上方显示，添加ACMultiSP: 前缀。修正管理搜索退格触发暂停，以及无多SP模型方案机体的开场提示。",
    "en": "Adds Version and updates, manual update checks, author contacts and bilingual credits. Notices appear at the top with the ACMultiSP: prefix. Fixes Backspace triggering native pause in management search and the startup notice for aircraft without a multi-SP model layout."
  },
  "downloads": [
    {
      "id": "lanzou",
      "label": {
        "zh": "蓝奏云下载",
        "en": "Lanzou download"
      },
      "url": "https://wwbru.lanzoue.com/REPLACE_WITH_NEW_SHARE_ID"
    },
    {
      "id": "r2",
      "label": {
        "zh": "CloudFlareR2 下载",
        "en": "CloudFlareR2 download"
      },
      "url": "https://downloads.iot-node-service-api.online/AC7MultiSP-2.24.1.zip"
    }
  ]
}
```

以后发布时主要改三处：`latest_version`、中英文 `release_notes`、两个下载 `url`。版本号使用三段正式号，不加 `v`、alpha 或其他后缀。

保持 `schema=1`、`product=AC7MultiSP`、`channel=stable`。确认下载可用后才设 `status=ready`。JSON 中不能写注释、重复字段或末尾多余逗号；每个入口需有不同的 id 以及完整中英文 label。清单总大小不超过 32 KiB，中英文更新说明分别不超过 8192 个 UTF-8 字节。

如果尚未准备好发布，可暂用下面的完整结构。2.24.1 会显示“更新信息尚未发布”，不展示下载按钮；旧客户端可能仍显示检查失败，因此正式流程测试应等 ready 清单部署后再做。

```json
{
  "schema": 1,
  "product": "AC7MultiSP",
  "channel": "stable",
  "status": "preparing"
}
```

## 五、部署到 Pages

先保存现有 Pages 项目的完整发布目录和旧 `stable.json`，便于回退。Pages 是重新部署一套站点文件，不是在在线文件列表中单独替换一个文件。

**如果项目采用直接上传：**

1. 进入 **Workers & Pages → ac7multisp-updates → Create a new deployment**，选择 **Production**，不要只发布 Preview。
2. 在原完整站点目录中替换根目录下的 `stable.json`，保留原有 `index.html`、`preview.json`、其他清单和配置等文件。不要用只有 stable.json 的空目录覆盖整个现有站点。
3. 将完整目录或其部署 ZIP 上传，选择 **Save and Deploy**，等待部署成功。上传内容应使 `stable.json` 出现在站点根目录，不能多套一层文件夹。[Cloudflare 直接上传说明](https://developers.cloudflare.com/pages/get-started/direct-upload/)

**如果项目显示已连接 Git：**在它当前关联的仓库和生产分支中更新同一份源文件，按该项目原有方式部署。无需重建 Pages 项目，也不要改到不相关仓库。

对于静态 Pages 站点，可在发布目录的 `_headers` 文件中保留或补充以下规则，让固定地址及时返回新 JSON。已有其他规则继续保留：

```text
/stable.json
  Content-Type: application/json; charset=utf-8
  Cache-Control: no-store
```

若该地址由 Pages Functions 或 Worker 动态生成，应在对应响应中设置这些头；`_headers` 不作用于函数生成的响应。[Cloudflare 响应头说明](https://developers.cloudflare.com/pages/configuration/headers/)

部署后直接打开固定入口：

`https://ac7multisp-updates.pages.dev/stable.json`

应看到新 JSON：`schema` 为 1、`latest_version` 为 2.24.1、`status` 为 ready，两个链接分别指向本次文件。不要只检查带部署编号的临时网址；游戏读取的是上面的固定网址。

## 六、核对文件与游戏流程

在 PowerShell 中可用以下方式核对下载前后的文件。将第二条路径改为实际下载位置；两次 Hash 应相同。

```powershell
Get-FileHash -Algorithm SHA256 -LiteralPath 'E:\AC7_MultiSP\Release\AC7MultiSP-2.24.1.zip'
Get-FileHash -Algorithm SHA256 -LiteralPath 'C:\Users\83717\Downloads\AC7MultiSP-2.24.1.zip'
```

- 发布 ready 清单后，重新启动目前安装的 2.24.0 alpha1，进入主菜单并稍等，确认提示新版 2.24.1。旧版每次启动只自动查一次，先前已经失败的进程不会自行重查。
- 打开 F6 的“版本和更新”页，分别点击蓝奏云和 R2，确认默认浏览器打开对应入口，下载到的两个包与本地发布包 Hash 一致。
- 退出游戏后按发布包说明手动安装 2.24.1，再检查“手动检测更新”：检查中按钮禁用，结束后可再点击；当前清单仍为 2.24.1 时显示已是最新版本。
- 检查操作反馈、模型加载和无适配提示：都在屏幕上方，且只带一次 `ACMultiSP:` 前缀。

## 七、常见问题

| 现象 | 先核对什么 |
| --- | --- |
| 日志提示缺少 schema | 固定 Pages 地址是否仍返回旧格式；生产部署是否成功 |
| 显示更新信息尚未发布 | status 是否仍为 preparing；两处下载就绪后再发布 ready |
| 显示无法检查更新 | 查看 native.log 中最新 UPDATE_CHECK 记录；HTTP 状态或 WinHTTP 错误、JSON 格式及公开访问是否正常 |
| 网页仍显示旧版本 | 是否只部署了 Preview；固定地址是否更新；检查缓存及 Cache-Control |
| 浏览器能打开但客户端拒绝清单 | 是否返回 JSON、是否含完整字段、是否使用支持的 HTTPS 下载主机；不要使用控制台链接、登录链接或重定向后的登录页 |
| 提示新版但下载的是旧包 | 分享页是否选错文件，R2 对象键和清单 URL 是否仍指向旧版；重新比较 Hash |
| R2 下载 404 | 检查对象名大小写、目录路径和公开域名绑定，不能使用 S3 API Endpoint 当玩家下载地址 |

需要回退时，恢复先前完整 Pages 部署或原 stable.json，并重新确认其中的版本与下载文件相符；保留旧 R2 对象可供回退，不必删除新旧文件。
