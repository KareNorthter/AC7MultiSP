# 发布资产

当前交付包为 **AC7DualSP-1.0.0.zip**，内含 GameFiles、微软离线运行库、TXT 安装说明及文件校验清单。用户按手动复制步骤安装，不需要命令行。

包由 `src/package-release.py` 生成；[发布回执](../evidence/1.0.0-release.json)记录文件及 ZIP 哈希。[版本说明](../docs/version-1.0.0.md)区分用户实测和离线核验。

ZIP 是本地产物，不入 Git，也未上传或公开发布。Git 保存当前源码、必要 DLL、加载器、许可证与核验记录。原始 SP2 音频不入 Git，从源码重建时需另行准备。

旧 `DualSP-0.1.7.zip` 与 [baseline](baseline-0.1.7.json)仅用于历史恢复，不是当前版本。其字节保持不变；旧包内文档按当时快照理解。根目录旧安装入口已退役，避免误装。
