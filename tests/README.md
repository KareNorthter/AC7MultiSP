# 检查入口与覆盖边界

本目录保存现有离线检查及历史模拟。检查入口是`tests/run.py`，需要一个已有完整构建目录，通过`--build-dir`指定；本页只说明实现，不表示本轮已运行或获得执行授权。当前版本和实际验证状态统一见[项目入口](../README.md)，操作规范见[开发规范](../docs/project/workflow.md)。

## 成套入口实际执行什么

| 文件/阶段 | 检查内容 | 不能证明的事项 |
| --- | --- | --- |
| `release_pipeline_test.py` | 来源增删改、旧回执/错误宏/越界拒绝；同一来源生成表与Lua清单一致；资源成员白名单 | 游戏加载、动画或全部Mod兼容 |
| `current_runtime_test.py` | 直接运行产品Lua的纯规则，含1/2/3 SP选择、配弹比例、购买筛选、组合轮切和布局规则 | 原生对象、真实输入、发射及渲染 |
| `runtime_services_test.py` | 配置保存与失败保护、内存桥接、页面具名接口、预览/管理/换机状态和提示等；使用Lua与模拟依赖，临时文件留Temp | 引擎异步对象真实就绪、暂停菜单实际行为、实际页面或模型表现 |
| `reload_timer_test.cpp` | 装填计时边界、负延迟、窗口与恢复规则 | 原生武器实体生成及长时发射 |
| `panel_pointer_test.cpp` | 指针/快捷键纯规则；版本比较、离线清单、重复字段及下载URL边界 | 操作系统真实消息链、在线清单、网络后台或浏览器启动 |
| `weapon_group_test.cpp` | 用合成内存核对捕获/恢复、持续对象数组、引用搬迁与失败保头、挂载/碰撞和生命周期守卫 | 游戏分配器、实际对象寿命、持续伤害/干扰、骨架/碰撞与动画归还 |
| 本次`DualSP.dll`、`DualSPUI.dll` | 在独立检查进程加载DLL并核对导出、UI源码显示点和二进制标识 | 游戏钩子安装、真实游戏接口、输入或帧率 |

入口先核对`build-receipt.json`与当前来源、工具链指纹，再依次运行三个Python检查，编译并运行三个C++检查，最后加载两份DLL。Python依赖来自`tools/test-python`；C++编译器和MinHook对象取自该构建的回执与目录，不由旧独立脚本另选工具链。

每轮输出在构建目录的`tests-时间戳/`，日志按检查项保存；若已有`test-receipt.json`，先移入本轮输出目录为`previous-test-receipt.json`。仅全部完成并复核来源与检查源码未变化后才写新的通过回执，失败不能继续引用旧通过状态。入口不会启动/注入AC7，不会调用游戏钩子，也不会启动更新后台、联网或打开浏览器。

## 保留的其他检查与旧入口

以下文件不由当前`run.py`调用，不能计入它的通过范围，也不按文件存在就推定仍适合当前协议。

| 文件 | 保留用途与限制 |
| --- | --- |
| `controller_test.py`、`native_hud_test.py`、`native_loadout_test.py`、`native_loadout_fixture.lua` | 旧双SP控制器/HUD/选装模拟，协议已过期，供追溯历史结构。不能为了“全绿”修改断言或当成现行实测 |
| `mount_plan_test.py` | 使用已提取坐标的旧离线布局规划检查，依赖`evidence/weapon-model-layout.json`；不代表当前全部机型或游戏内几何 |
| `ui_binary_test.py` | 硬编码旧`dist/DualSP/bin/`和旧证据路径的独立二进制检查；会写历史回执，不作为当前候选入口 |
| `run_native.cmd` | 旧MSVC入口，调用计时、武器组和指针检查，输出根`build/`；不是当前便携工具链成套检查流程 |
| `run_overlay.cmd`、`overlay_hook_test.cpp` | 旧MSVC独立DX11窗口/钩子试验；不启动AC7，但不属于当前成套入口的DLL加载核对 |
| `run_overlay_portable.cmd` | 已退役，只提示使用`tests/run.py`并返回失败；原说明中“便携UI检查使用它”的文字已失效 |

真实出击、补给、检查点、F6/原生暂停、换机后的新玩家、实际键鼠/手柄消息、机型动画、在途兵装、第三方自有行为、长期稳定性与Present帧时间均需分别取证或用户实机验证。自动检查、离线资源回读、用户反馈和发布核验是不同证据，不互相替代。

研究、构建及旧工具的实际位置见[开发与恢复](../docs/engineering/development.md)和[工具说明](../tools/README.md)；故障判断与计量边界见[工程经验](../docs/engineering/engineering-lessons.md)。本页不继续积累每轮候选版本的交接文字；旧全文可从`archive/20261023/pre-governance-main`恢复。
