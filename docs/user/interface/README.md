# 界面文字资源

本目录供维护者和AI编辑，玩家说明仍是上层四个主题的中英文TXT。

| 文件 | 内容 |
|---|---|
| game.zh-CN.json、game.en.json | 游戏固定界面、选装页、通知及错误显示词条 |
| [游戏盘点](game-inventory.json) | 已迁移入口、原始消息匹配及外部数据排除范围 |
| installer.zh-CN.json、installer.en.json | EXE界面、确认、进度、错误与导出提示 |
| [安装器盘点](installer-inventory.json) | 对应源码入口、双语及格式核对范围 |
| site.zh-CN.json、site.en.json | 双语主页的40对固定词条；不重复维护作者、联系方式和四份正文 |

游戏资源改值后运行`python src/user_texts.py`，提交源JSON及生成的C++头/Lua表；构建也会自动生成。使用`--check`可仅检查产物是否过期。安装器直接内嵌JSON，不复制另一套源码文字。

主页词条由`src/build_pages_site.py`读取，填充输出目录的app.js和HTML首次显示文字；site/app.js中的SITE_UI_COPY标记是模板，不手工填翻译。主页生成与游戏构建分别执行；输入和资产位置见[主页说明](../../project/pages-site.md)，自动准备与人工上传顺序见[新版发布流程](../../project/release-workflow.md)。

紧凑作者区、水印和反馈内容不放进本目录另行维护：它们从上层中英文“作者与授权”正文提取。提取格式与展示范围见[玩家文档维护](../../project/player-docs.md#接入与交付边界)。按钮名称、系统语言选项、更新状态和兼容性说明仍在对应双语界面资源中维护。

保留稳定键、格式占位、列表分隔和ImGui标识；不要从翻译正文重新生成键。游戏原始错误匹配字段供定位旧错误，不是可自由改写的显示文字。完整维护边界见[玩家文档维护](../../project/player-docs.md)。
