# 界面文字资源

本目录供维护者和AI编辑，玩家说明仍是上层四个主题的中英文TXT。

| 文件 | 内容 |
|---|---|
| game.zh-CN.json、game.en.json | 游戏固定界面、选装页、通知及错误显示词条；每语言467项 |
| [游戏盘点](game-inventory.json) | 已迁移入口、原始消息匹配及外部数据排除范围 |
| installer.zh-CN.json、installer.en.json | EXE界面、确认、进度、错误与导出提示；每语言103项 |
| [安装器盘点](installer-inventory.json) | 对应源码入口、双语及格式核对范围 |

游戏资源改值后运行`python src/user_texts.py`，提交源JSON及生成的C++头/Lua表；构建也会自动生成。使用`--check`可仅检查产物是否过期。安装器直接内嵌JSON，不复制另一套源码文字。

保留稳定键、格式占位、列表分隔和ImGui标识；不要从翻译正文重新生成键。游戏原始错误匹配字段供定位旧错误，不是可自由改写的显示文字。完整维护边界见[玩家文档维护](../../project/player-docs.md)。
