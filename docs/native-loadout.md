# 原生任务准备双 SP 选择：0.1.7

返回[项目入口](../AGENTS.md)，操作见[使用说明](../AGENTS.md#使用说明)，模块关系见[架构](architecture.md)。参考图片位于[参考资料](../references/README.md)。

本轮实现是在 AC7 原有 `CampaignHangarWeapon` 页面中替换显示控件和分阶段处理输入，保留原生背景、机体场景、布局容器、字体、类别图标、性能条与普通兵装栏。不是 F8 窗口换皮，也没有新增覆盖整个屏幕的外部窗口。参考用户提供的 AC8 三张机体/SP1/SP2 截图。

## 使用流程

1. 正常选择机体，进入原来的特殊兵装页。标题改成“选择 SP WEAPON 1”。上下方向键、W/S、手柄十字键或左摇杆移动选择；决定键选择 SP1。
2. 同一页面进入“选择 SP WEAPON 2”。排除已选的 SP1，并在最后提供“无兵装 / NO WEAPON”。不能双挂的 UAV/ECM 组合仅提供空 SP2。决定键进入确认；返回键回到 SP1。
3. 确认页显示两种已选兵装。松开决定键后，再按一次原游戏决定键继续，原生菜单负责后续跳转。直到原生 `SetPlaneAndSWP` 确认成功，才保存双 SP 配置。此步骤比 AC8 参考流程多一次确认，是当前版本的明确取舍。

在 SP1 页按返回会结束本次双 SP 编辑、恢复原来单 SP 兵装页；再次使用游戏返回操作即可离开。不保存中途草稿。确认页返回交给原生菜单处理，也不提交本次草稿。

F8 不再在进入兵装页时自动弹出，仍可手动打开。F8 打开前释放原生页面的临时控件和输入控制；关闭后从已保存配置重新开始。F8“界面设置”可关闭“使用原生出击 SP 选择界面”，继续使用 F8 原有的鼠标、键盘及手柄配置方式。

## 已确认的原始结构

只读解包、解码本机 Steam build 9855922 的以下资源副本，没有修改游戏 PAK：

| 资源/控件 | 用途 |
|---|---|
| `CampaignHangarWeapon`，原生父类 `CampaignHangarWeaponWidget` | 原特殊兵装页；界面蓝图没有实现选择与出击跳转脚本 |
| `LightMenuBaseWidget` 中 `TitleText`、`DescriptionText`、`ItemText0…2` | 标题、说明和三个列表行 |
| `LightMenuItemIconCategoryWidget.ItemIconL0…2` | 原生候选武器类别图标 |
| `WeaponInformation.HangarAircraftWeaponNameLong` 内 `Name` | 当前武器名称，位于 HorizontalBox |
| `HangarSpecGroupR` 内 `SpecItem1…6.SpecBar` | `BarA/B/C` 为 Image；满宽由 `BaseImage3` 的末端坐标确定，原始资源为 160 逻辑单位 |
| `HangarAircraftAllWeaponsAmmo.WeaponsSP1…3` | 每个候选武器有 `WeaponsName/WeaponsAmmo`；复用前两组显示挂载方案 |
| `/Script/Nimbus.NimbusGameInstance:SetPlaneAndSWP` | 历史[反射记录](../evidence/native/自由飞行-ui-interfaces.txt)确认的机体/原生单 SP 提交入口 |

名称/说明控件复制自活跃原生模板；Canvas 与 HorizontalBox 的布局槽分别复制。原生菜单的 Canvas 列表文字保留对象身份、父级和布局槽，仅遮蔽绘制，供原生程序继续使用缓存引用与几何信息。右侧名称位于自动排宽的 HorizontalBox，单纯透明仍会占宽，因此该原文字放入折叠容器保留，结束时恢复原布局设置。自己的显示控件拥有独立文字状态。结束、取消、F8、换机体或进入任务时移除临时控件并恢复显示；已经交回原生菜单后，不会重新启用正在退出的菜单。

输入从 UE 的 `PlayerController` 读取，不通过 F8 的 Win32/ImGui 输入处理。决定/返回读取 `InputSettings.ActionMappings` 中实际的 `INP_MenuAccept/INP_MenuCancel`，保留修饰键要求。导航提供 AC7 常用的上下/W/S、十字键和左摇杆，包含延迟连移。按住一个决定键不会连选两种 SP，也不会跳过确认交接。

## 当前边界

- 尚未启动游戏验证创建、视觉、原生提交调用的真实时机。因此这属于可供实玩验证的第一版原生集成，不宣称全部交互已与 AC8 一致。
- 首版原生页支持键盘和手柄；没有新增鼠标命中/点选。需要鼠标操作时使用 F8。
- 原生武器说明图/预览视频、预览键和随选择更换机体挂载模型尚未联动。编辑阶段隐藏会显示旧武器的说明图与类型区，避免误导；其他原生背景与场景仍保留。此限制也与此前暂缓的双模型挂载目标分开记录。
- 右侧载弹量为基础载弹量按现有配弹比例计算的预计数，尚未把原生准备阶段的全部部件增益接入；实际任务以武器核心的实时弹药为准。双 SP 默认比例仍为 50%，沿用各机体已保存设置；F8 可调比例。
- 接入错误会记录 `NATIVE_LOADOUT_ERROR`，恢复原菜单，并在 F8 显示错误。不会冻结任务武器控制器，也不会暗中把 F8 自动弹出作为正常流程。关闭原生选择设置可明确使用原有入口。

## 离线验证与资料

实际发布的两个 Lua 模块通过控件树与原生接口模拟：25 个准备场景、9 种合法挂载组合；包括空 SP2、未购买阻止、UAV/ECM 单挂限制、输入重映射/修饰键、键盘/手柄、连续按住确认、延后交接、取消不保存、忽略原生预览/悬停写入、原生 SP1 参数保护、退出时不重启菜单、F8、换机体及部分创建失败恢复。准备适配器在模拟飞行中不扫描对象、不读取输入。此模拟不能证明游戏内渲染与 C++ 菜单的真实调用行为。

任务控制器、固定原生 HUD 和独立 F8 界面继续进行各自回归；证据位于[精选证据](../evidence/README.md)中的 `0.1.7-*`。原始资源的完整 JSON 仅放在项目 `Temp/loadout-json`（原副本已回收，可从自有游戏再生成），不随安装包分发。

参考：[UE4SS RegisterHook](https://docs.ue4ss.com/lua-api/global-functions/registerhook.html)（原生函数前/后回调与已加载函数要求）、[StaticConstructObject](https://docs.ue4ss.com/lua-api/global-functions/staticconstructobject.html)、[UAssetGUI](https://github.com/atenfyr/UAssetGUI)（离线资源结构读取）。具体控件路径来自本机资源与历史运行时记录，不从通用 UE 文档推测 AC7 菜单名称。
