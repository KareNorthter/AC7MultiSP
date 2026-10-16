# 当前检查与历史回归

当前入口：`python -X utf8 tests/run.py --build-dir Temp/build-2.0`，须先用 src/build.py 完整构建。

- 新增来源拒绝检查：修改公共头、第三方源、产品清单，以及新增/缺失文件、旧回执、错误宏和越界均须拒绝。
- 当前纯规则：1/2/3 SP 出击与补给、默认 NO、返回、配弹、单挂限制、未购买拒绝、耗尽循环。
- 沿用冷却、指针、武器缓存恢复 C++ 检查，使用当前工具链和固定挂载宏；缓存测试只使用合成内存，不证明真实挂点/动画。
- 加载本轮核心/UI DLL、核对符号和标题，不启用游戏钩子。
- 所有产物在该构建目录内；通过回执绑定构建与测试源码。重跑前旧通过回执移入本次测试目录，失败不能沿用旧通过状态。

真实出击、补给、检查点、F6 暂停与换装、机型动画及长时稳定性由用户实玩验证。

以下为历史检查说明，不纳入 2.0 通过范围。旧 controller_test/native_hud_test/native_loadout_test 模拟仍对应双 SP 协议；不为凑齐“全绿”修改断言或伪装运行成功。旧 run_overlay_portable.cmd 已明确退役，其他旧入口仅供历史研究。

这些检查不启动 AC7，也不读取或改写游戏进程。

- `run_native.cmd`：需要 Visual Studio C++ 工具链。检查计时的负偏移、完成后的超出值、装填窗口、原生缓存恢复的零弹药与队列旋转，以及无效输入的明确拒绝。
- 同时检查独立指针的原始相对移动累计、绝对坐标输入、边界限制和窗口尺寸变化。不会真的移动电脑鼠标或调用游戏。
- `run_overlay.cmd`：在独立测试进程中创建隐藏的 DX11 窗口，验证 DX11 与三个 Win32 挂钩可以安装。面板保持关闭，独立指针没有取得输入控制，不启动 AC7，也不发送输入。
- `controller_test.py`：需要 Python 3 和 `lupa`，使用 Lua 5.4 运行实际发布的控制器，以模拟的 UE4SS 对象和内存文件检查选择、消耗、跳过耗尽 SP、原输入观察、F9 及错误 HUD。
- `native_hud_test.py`：运行实际原生 HUD 脚本，检查固定两行布局、缩放、字号变化、启停及部分排版失败后的恢复。额外模拟 400 次切换时游戏文字更新与模组更新交错，检查可见两行不会交换名称/弹药身份，并且布局槽不重建。
- `native_loadout_test.py`：从实际准备菜单资源提取的控件结构与接口模拟，运行真实原生准备适配器及草稿状态机。检查顺序选择、空 SP2、返回、连续按键、原生映射/修饰键、手柄、确认交接、禁止误保存、部分创建失败恢复及退出后不能重启旧菜单。

在源码根目录运行：

```powershell
python -m pip install --target .\tools\test-python lupa
python -X utf8 .\tests\controller_test.py
python -X utf8 .\tests\native_hud_test.py
python -X utf8 .\tests\native_loadout_test.py
.\tests\run_native.cmd
```

模拟环境只提供游戏线程计时接口，调用旧异步接口会失败。模拟控制器检查不能替代原生游戏输入、物理行为与渲染。`run_native.cmd`、`run_overlay.cmd` 是仍依赖 MSVC 的历史入口；便携 UI 检查使用 `run_overlay_portable.cmd`。这些文件不作为默认自动执行流程。
