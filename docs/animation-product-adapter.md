# SP动画的产品访问适配

本说明对应研究分支新增的三个 `mount_animation_*` 头，供整合线程审查和使用。没有修改核心、旧控制头或运行准入，没有编译、运行测试、安装、附加游戏或调用游戏函数。原件由主管线程在用户许可下只读采集，本线程只离线读取。

## 本轮可直接复用的部分

- `mount_animation_reflection_adapter.hpp`：复用产品真实对象表身份、安全读写、类与世界校验、UE4SS导出，按反射解析 GetSocketTransform、DoesSocketExist、SetComponentTickEnabled、IsComponentTickEnabled。组件/世界空间来自真实具名枚举，返回结构按 Transform→Quat/Vector 的真实子字段解析。未知布局抛出错误，不退回猜测偏移。Tick支持绑定后快速读取；其他字段/取位仍反射核对，尚未完成全部元数据缓存优化。
- `mount_animation_product_access.hpp`：具体实现旧 Parts Access 与骨骼刷新 Access；含 PlayerPlane 父类和规范Pawn包检查、完整写区权限、代码回执、真实刷新模式枚举。仅限定 F-22A、Su-47、Su-57 Top Gun。静态元数据允许序列号0，但仍复核对象表；动态 Binding 沿用非零序列号要求。
- `mount_animation_publication_tracker.hpp`：给出了真实骨骼发布入口的包裹实现，原生跳板恰好执行一次。只在原生本帧实际发布之后，同步获取组件/世界挂点变换、检查几何端点并生成见证；不主动刷新动画、不另造动画时钟。

这些是产品访问实现，不是完整 Host 或自动启用补丁。旧 Host 的资源/布局资格仍须用主管实际注册的数据核对。不得把未接入的代码说成“三机已能玩”。

## 新证据修正了旧合同

2026-10-17 F-22A 单SP、8AAM已选且暂停的实采：

| 项目 | 真实值或行为 | 整合要求 |
|---|---|---|
| bNoSkeletonUpdate | 组件+0x9b0，mask=2 | 非当前SWP1/2确实置位；必须单独租约管理 |
| bPauseAnims | 同字节，mask=4 | 不能继续把mask2叫暂停动画 |
| MeshComponentUpdateFlag | +0x804，原生值0 | 实际枚举 AlwaysTickPoseAndRefreshBones=0；代码仍按具名枚举解析 |
| TransformSpace | RTS_World=0、RTS_Component=2 | 不采用“组件空间=1”的猜测 |
| GetSocketTransform | InSocketName@0/8、TransformSpace@8/1、ReturnValue@0x10/48 | 参数与返回类型继续实时反射复核 |
| Parts | request=1，phase=2，公共进度=1 | request1并非异常或未打开；旧 nativeFullyOpen 会误拒绝 |

原生 `0x1ce7f69` 读取 input+0x4c，即 Parts+0x58；request1推进公共进度，达到1时phase写2，request仍可为1。因此新 tracker 的端点门槛接受request1或2且phase2/进度1，拒绝request3收拢中的旧端点。

主管后续4500条只读连续轮询覆盖两次原生关闭/打开：request3→phase3→phase0，request1→phase1→phase2。SWP1/2始终NoSkeletonUpdate=2、骨骼缓冲不变、动画帧0；SWP3持续变化。这支持“当前provider更新、非当前provider停更”的判断；不证明回调调用顺序，也没有由轮询生成当帧见证。

三组+0x3f始终1。后补原生代码已确认 IsComponentTickEnabled 的wrapper在 `0x1b2ab4e` 调 `0x16840d0`，该getter完整8字节为 `80793f000f95c0c3`，语义就是 `byte[component+0x3f]!=0`。因此旧原型读取该字节的行为正确；**Tick=true不足以证明骨骼求值**，不能把非当前挂架停更归因于Tick读取错误。

`bindTickReader(component,generation)` 在绑定时反射检查函数所属类、返回布局和真实原生入口，核对完整0x2b字节wrapper及8字节getter，再做一次反射返回值交叉核对。`tickFast(reader,generation)` 逐次核对对象/类/函数/参数身份、绑定代次和原生代码后直接读+0x3f，不遍历反射链、不调用ProcessEvent、不执行getter。Endpoint的tickReader由该绑定接口生成；Tracker已改用此路径。它只消除读取Tick的重复开销，其余骨架布尔字段和挂点反射仍需后续缓存优化。

**00bd24c的旧三个头不可直接原样接入产品。** 当时的 `RackRequest{paused,tick}` 无法表达 NoSkeletonUpdate，`Host::applyRack` 的“不碰NoSkeletonUpdate”约定与原生非当前挂架行为冲突，`nativeFullyOpen(request==2)` 过严。主管已在主工作区追加第三字段及request1/2门槛修订；研究树本次补纯记账入口时同步三字段合同，未另改旧Parts门槛，整合仍以主管已修订版本为准。旧骨骼刷新 Lease 只管理模式字段这一点可以保留，但不能代替另外三个字段的所有权与归还。

## 原生控制及归还

原生禁用入口 `0x7f0210` 在 `0x7f0297` 调 Tick 后，`0x7f029d` 直接置 NoSkeletonUpdate；另一分支 `0x7f02ae` 也置位。只拦截 Tick 无法维持骨骼更新。

产品分别核对 `{noSkeletonUpdate,pauseAnims,tickEnabled,refreshMode}`，先准备全部provider。当前限定三机优先采用较小接管范围：原生刷新模式已经是具名 AlwaysTickPoseAndRefreshBones，**只监视、不接管、不写该模式**；若原生改成其他模式立即视觉回退。原生Pause本轮始终false；遇到真实Pause=true请求则退出接管并归还，不能强行清除原生暂停。

NoSkeletonUpdate与Tick按需要接管；Pause仍保留独立状态和三字段归还合同，避免旧结构混用位掩码。所有本模块写入通过 `reflection.ownWrite` 绕过原生观察，归还逐项采用最新原生请求，不以当前强制值覆写原生意图；死亡对象丢弃，失败项保留重试。刷新模式未取得写所有权，因此退出时也不写回一个旧快照；不需要为“原生重复写同一模式”的不可观察意图造回执。旧模式Lease可供后续有完整观察能力的方案使用，当前三机无需调用其 acquireOrMaintain。

`aroundNativePublication` 不修改以上字段，也不负责维持租约。`rackState()` 可读三项真实状态；`writeComponentBool()` 分别写两个真实反射位并保留同字节其他位；`setTick()` 经过原生反射接口。原生禁用入口可用于完成后重维持，但必须在产品侧补齐观察与独占登记；本轮没有证明“所有外部直接字段写入均已被观察”，尤其是原生重复写入同一强制值的意图不能靠轮询还原。

共用Controller现已增加Pause写入保护，覆盖bind取得控制、maintain维持、restoreSwitch写回Parts及其内部maintain。每次接管写入前扫描全部所需挂架；实际Pause=true先记入对应latestNative，再停止控制并归还。扫描不在首个Pause处提前结束，避免归还其他架子时漏记暂停。模式或读取失败也在仍接管时尽力收集Pause后归还。

原生请求入口先采调用前的实际Pause，再登记本次changedFields明确声明的请求；因此本次Pause=false不会被调用前的旧true覆盖。本次原生Pause=true直接停止过滤并归还。release仅应用latestNative，不调用接管保护或stopControl；已进入归还重试时也不再次把上次恢复的true当作新原生请求，避免递归和丢失较新的明确请求。Host自己的applyRack写入仍必须用重入标记绕过原生观察。

Pause保护最初提交ca5a420基于旧两字段Controller；后续新增纯记账入口时已同步NoSkeletonUpdate第三字段、changedFields=7及bit2记录，与主管合同一致。整合时按差异加入保护与记账入口，保留主工作区其他修订。Host收到Controller已退出或归还失败后仍须完成自身release：清理owner登记，或保留待重试组件，不允许重新prepare覆盖未归还所有权。本轮没有编译或运行验证。

### 原生清除NoSkeletonUpdate的实际位置

对称启用不是在 `0x7f0210` 旁边找到的独立函数，而是在产品既有初始化例程的挂架循环中：

| 项目 | 已核对内容 |
|---|---|
| 入口及参数 | `0x882e40`，Win64 RCX为激活器，EDX为配置编号；入口把EDX保存在原rsp+0x10 |
| 配置比较 | `0x883415` 比较循环编号r15d与保存的配置r13d，`0x88341b` 得到r14b=(编号相等) |
| 类型门 | `0x88346b..0x883497` 通过原生类继承检查后才进入骨架字段赋值 |
| 清位与重新赋值 | `0x8834a1` 清+0x9b0的bit1；`0x8834af..0x8834b4` 写入 `2*(r14b==0)`；因此当前组为false，其余组为true |
| 观察边界 | 必须看完至`0x8834c0`的整段，不能在清位中间把所有组误记为false；本段只报告NoSkeletonUpdate对应changedFields位2，不报告Pause |
| 逻辑例程范围 | `[0x882e40,0x883a52)`，0xc12字节；由5个连续.pdata片段及完整反汇编控制流闭合核对，结束是恢复栈后的尾调用，不能把第一个unwind片段末尾0x882eb0当函数返回 |

本段赋值的完整39字节为：

```text
4584f68bc60f94c083a3b0090000fd83a310020000fd83e00103c00983b0090000098310020000
```

当前F-22A只读原件 `Temp/f22a-socket-native-20261017-1144/mount-882e40-800.bin` 已覆盖上述赋值，SHA256为 `88ada28452481d9fa0b7347d1a9d17285cd0c12ee4ef4ddf9690f842e9e2fe8a`。整段0xc12字节来自既有 `Temp/su30sm-fixed-mount-prototype/Temp/native-current/mount-87f000-9000.bin`，前0x800与本轮原件逐字节相同；抽取段SHA256为 `9ee46f9696a89bcde9804e4b4bd7ab3cf7897af8fbe56f1bb132262abed19583`。本轮.pdata原件为 `Temp/f22a-vtable-native-extended-20261017-1144/runtime-functions.bin`。

这是实际切换时另一个置位/清位来源，不能只观察7f0210。产品可以在既有初始化绑定中按真实循环组身份和配置参数登记原生意图，但须保证该原生类型门和对应组件确实匹配，并在本模块重新强制清位之前完成记录；不能用已被本模块清过的最终位值反推原生请求。没有证明独立启用helper或C++返回类型，本轮不新增猜测函数指针、不在函数中间安装钩子。

实际调用边界已在本轮 `Temp/f22a-native-20261017-1144/mount-81e810-2200.bin` 核对：产品现有 `initialize()` 调 `0x81e810(plane,index)`；其 `0x81e831` 读取 `plane+0x11a8` 为激活器，`0x81e841` 传配置EDX，`0x81e843` 的 `e8f8450600` 调 `0x882e40`，返回点 `0x81e848`。无需再调用初始化一次；在产品已有那一次 `0x81e810` **正常返回后立即观察**即可。

当前产品调用次序为 `fixedMountBeforeRebuild → syncPlaneWeaponSelection → 0x81e810 → ravenAfterRebuild → fixedMountAfterRebuild`。推荐在实际Init调用的RAII范围尾部、ravenAfterRebuild之前收集原生值；如果放在fixedMountAfterRebuild顶部，则必须将禁止强制的范围连续保持到那里，且在restoreSwitch/maintain之前完成记账。不能先结束禁止范围、触发其他维护，再读已经被清过的NoSkeletonUpdate。

初始化批量观察使用新增 `rememberNativeRequest(host,rack,wanted,changedFields)`。它只更新对应lease的最新原生请求，不读组件、不写字段、不触发归还，也不返回强制请求；普通 `nativeRequest` 遇到Pause会主动归还，**不能拿来充当初始化阶段的纯记账函数**。具体顺序：

1. 整个原生初始化期间抑制本模块所有维持/接管写入；Tick和7f0210钩子仍执行原函数，但改用rememberNativeRequest分别记录位1和位2，不触发maintain。仅外层remember跳过maintain并不构成通用保护，其他ensure/发布回调若能写控制字段也须遵守该范围。
2. 正常返回后，禁止范围仍有效。先核对同一Binding/generation及飞机当前三架引用，再一次性读取全部prepared骨架的原生状态存入本地数组；Su-57 TG静态第三组不读骨架字段。
3. 数组全部读取成功后逐架调用rememberNativeRequest报告changedFields=4，仅记NoSkeletonUpdate；读取到真实Pause=true则追加位0。Tick由真实setter钩子记录明确参数，不用未经赋值的当前强制值覆盖旧Tick意图，也不把所有字段一概标成7。
4. 全部记完才结束禁止范围，然后进入原有restoreSwitch/maintain。此时统一Pause保护即使要求退出，也已掌握其他挂架最新NoSkeletonUpdate，不会先恢复旧值再污染后续读取。异常/读失败走回退并保留未归还项，不继续维持一个未完成记账的绑定。

主管已确认本机C runtime的全局 `::read` 会遮住产品匿名命名空间模板；主工作区使用独立 `mountAnimationRead<T>` 包装解决。合入适配器时保留该编译兼容修订，不用研究树旧的 `::read<T>` 调用覆盖它；本轮未自动回拷主核心或运行编译。

## 同帧发布的具体接线

已核对SWP三组真实虚表：slot0x890→`0x16e5de0`，slot0x3d0→`0x16eafd0`。前者0xbc字节有自然ret，不把 `.pdata` 的unwind片段当完整函数。

1. 原生发布入口在+0x828有待发布标志时清标志。双缓冲开启时把旧+0x748索引发布到+0x74c，翻转+0x748；单缓冲则两索引清0，随后完成通知。
2. 原生动画更新 `0x16fb6d6/0x16fb6dc` 把全局 `module+0x3ac5520` 的低32位写到组件+0xd24；`0x16fa02b` 从该字段与同一全局比较。本地递增计数不能替代此值。
3. 原生挂点变换 `0x16eb137` 调 `0x16e79d0`；该函数先尝试MasterPose路径，不能把 `0x16e7a7e` 的master缓冲误当自己的缓冲。无MasterPose分支 `0x16e7f75` 才以本组件+0x74c取已发布缓冲；Tracker实时反射检查 MasterPoseComponent 的弱句柄为空，拒绝主从骨架取位，并检查这一实际分支的代码签名。
4. 产品只给该入口安装自己的钩子，包裹签名为 `void(void* component)`；安装前调用 `beforeOwnHook(publishRva,publishCode)`，启用钩子后用实际detour和trampoline调用 `afterOwnHook`。钩子按该组件真实身份查 Endpoint，调用 `aroundNativePublication`；未登记组件直接走原函数。
5. Endpoint须来自已核对的整机几何注册，包括同Binding代次、具体provider、真实Mesh/Skeleton/AnimClass/AnimInstance身份、真实FName挂点、组件空间端点与净空允许的容差。Tracker会反射回读这四份资源引用，静态资源和AnimInstance的serial0仍按对象表复核。该类不替几何研究授权这些数据。

Tracker只在游戏线程、真实自身钩子回执、原生pending=1、原生帧相符、三项调度状态允许、刷新模式正确、Parts实际处于打开端点时观察。原生返回后验证索引确实发布；两次取位之间复核帧/索引/组件世界矩阵/Parts不变，并比较实际组件空间端点。成功token为真实原生帧+1（只避开空token），包含Binding代次。失败不产生见证，不延用上次结果。

成功结果仅在该钩子尾部同步消费：它证明这一取位窗口使用本帧发布缓冲。**不能直接保存见证供之后发射或跨帧显示使用。** 发射时如需复用，必须在实际发射取位入口再次验证同一帧/缓冲/Binding/Parts，或让发射直接沿用该同步取位结果；当前尚未接入该消费路径。也没有证明隐藏provider一定每帧到达发布入口：收不到合法发布时保持不放行，不能用手动调用刷新函数补证据。

### 严格零偏差端点的适用边界

将实际发布缓存的有效10个float与离线第100键做严格一致比较，可以替代允许偏差的端点接纳条件；比较不相等时回退即可，不能为了命中率私自扩大epsilon。不过“第100键”必须已转成与缓存相同的组件空间、单位和父链结果，不能把原始局部动画轨直接对比ComponentSpace缓存；也不能只比较当前采样快照与其自身。

Su-47本轮每组发布缓存有6个骨骼。应覆盖静态净空证明涉及的全部变形骨骼/门盖/导轨，或另有完整证据证明其他骨骼必然同步；只验证某个Socket绑定骨骼的10个float不够，该骨骼可能在开、关两端相同而其他机构仍不同。Socket须按完整原生查找优先级确定实际对象（Mesh覆盖Skeleton），并核对名字/绑定骨名、局部位移/旋转/比例；未序列化比例使用实际CDO默认值，不猜零或一。

严格比较也不替代资源身份、无MasterPose、原生当帧发布、Parts门控、父组件世界变换稳定及发射入口重验。四元数符号、插值/解压/父链计算的float舍入可能使几何相同的有效姿态被拒绝，这是保守回退，不构成放宽依据。本轮仅评估边界，没有实现零偏差消费路径或运行验证。

## 包含和生命周期

先在 `weapon_bridge.cpp` 匿名命名空间外包含 `common_bay_animation.hpp`、`native_common_bay_parts.hpp`、`owned_bone_refresh.hpp`，然后在现有 `mount_*` 身份、世界、线程帮助函数已定义后，依次包含本轮三个适配头。头内复用产品现有标准库和Windows声明，不在匿名命名空间里重新包含标准库。

Parts已经被产品钩住的 `0x1ce6d70` 也需要安装前/后的自身回执，否则代码资格检查会拒绝。回执必须来自本产品实际安装，不从“发现一个跳转”自动授权。`registerOwner()` 只接受已完成资源绑定的完整Binding；重建前先归还，再 `forgetOwner()` 并以新generation重新登记。访问器不自动管理全局字段独占。

## 仍需整合线程完成

- 接入三字段独立归还及原生观察，刷新模式仅监视；真实Pause请求触发退出，不能只把Pause的数字改成4。
- 将几何全7组合注册、具体挂点端点/容差、body/架子资源身份接入真实Host。
- 接入发布钩子和同步显示/发射消费；完成之后才有可提交用户实测的三机候选。
- 按授权编译和实机检查。此分支仅静态分析、源码与文档，不能代替这些结果。

CFA-44的具名骨显隐接口、X-02S三机构共进度、ASF-X的LASM域与FALKEN鼻TLS域不由本轮适配器自动开放。旧object-index中找到函数名只证明历史对象存在；未核对参数/时序前仍不能执行。

历史索引已离线核对 HideBoneByName、UnHideBoneByName、IsBoneHiddenByName 均归属 SkinnedMeshComponent。这可供CFA-44骨支路显隐租约继续研究；本轮没有把历史地址存成新实例句柄，没有猜PBO_None数字，也没有调用这些函数。
