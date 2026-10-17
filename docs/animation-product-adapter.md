# SP动画的产品访问适配

本说明对应研究分支新增的三个 `mount_animation_*` 头，供整合线程审查和使用。没有修改核心、旧控制头或运行准入，没有编译、运行测试、安装、附加游戏或调用游戏函数。原件由主管线程在用户许可下只读采集，本线程只离线读取。

## 本轮可直接复用的部分

- `mount_animation_reflection_adapter.hpp`：复用产品真实对象表身份、安全读写、类与世界校验、UE4SS导出，按反射解析 GetSocketTransform、DoesSocketExist、SetComponentTickEnabled、IsComponentTickEnabled。组件/世界空间来自真实具名枚举，返回结构按 Transform→Quat/Vector 的真实子字段解析。未知布局抛出错误，不退回猜测偏移。每次调用重新校验，当前实现尚未做元数据缓存优化。
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

主管后续4500条只读连续轮询覆盖两次原生关闭/打开：request3→phase3→phase0，request1→phase1→phase2。SWP1/2始终NoSkeletonUpdate=2、骨骼缓冲不变、动画帧0；SWP3持续变化。这支持“当前provider更新、非当前provider停更”的判断；不证明回调调用顺序，也没有由轮询生成当帧见证。三组+0x3f原始字节始终1，不能直接称为TickEnabled；适配器以真实 IsComponentTickEnabled 回读。

**00bd24c的旧三个头不可直接原样接入产品。** 当时的 `RackRequest{paused,tick}` 无法表达 NoSkeletonUpdate，`Host::applyRack` 的“不碰NoSkeletonUpdate”约定与原生非当前挂架行为冲突，`nativeFullyOpen(request==2)` 过严。主管已报告在主工作区追加了第三字段及request1/2门槛修订；整合时以该修订版为准。本轮按主管要求只新增文件，没有覆盖研究树的旧头。旧骨骼刷新 Lease 只管理模式字段这一点可以保留，但不能代替另外三个字段的所有权与归还。

## 原生控制及归还

原生禁用入口 `0x7f0210` 在 `0x7f0297` 调 Tick 后，`0x7f029d` 直接置 NoSkeletonUpdate；另一分支 `0x7f02ae` 也置位。只拦截 Tick 无法维持骨骼更新。

产品应把 `{noSkeletonUpdate,pauseAnims,tickEnabled,refreshMode}` 分别记录为最新原生请求，先准备全部provider，再取得控制权；保持时才分别清除NoSkeletonUpdate、清除Pause、启用Tick、维持具名刷新模式。所有本模块写入须通过 `reflection.ownWrite` 绕过原生观察。归还时逐项还原最新原生请求，不以当前强制值覆写原生意图；死亡对象丢弃，失败项保留重试。

`aroundNativePublication` 不修改以上字段，也不负责维持租约。`rackState()` 可读三项真实状态；`writeComponentBool()` 分别写两个真实反射位并保留同字节其他位；`setTick()` 经过原生反射接口。原生禁用入口可用于完成后重维持，但必须在产品侧补齐观察与独占登记；本轮没有证明“所有外部直接字段写入均已被观察”，尤其是原生重复写入同一强制值的意图不能靠轮询还原。

## 同帧发布的具体接线

已核对SWP三组真实虚表：slot0x890→`0x16e5de0`，slot0x3d0→`0x16eafd0`。前者0xbc字节有自然ret，不把 `.pdata` 的unwind片段当完整函数。

1. 原生发布入口在+0x828有待发布标志时清标志。双缓冲开启时把旧+0x748索引发布到+0x74c，翻转+0x748；单缓冲则两索引清0，随后完成通知。
2. 原生动画更新 `0x16fb6d6/0x16fb6dc` 把全局 `module+0x3ac5520` 的低32位写到组件+0xd24；`0x16fa02b` 从该字段与同一全局比较。本地递增计数不能替代此值。
3. 原生挂点变换 `0x16eb137` 调 `0x16e79d0`；该函数先尝试MasterPose路径，不能把 `0x16e7a7e` 的master缓冲误当自己的缓冲。无MasterPose分支 `0x16e7f75` 才以本组件+0x74c取已发布缓冲；Tracker实时反射检查 MasterPoseComponent 的弱句柄为空，拒绝主从骨架取位，并检查这一实际分支的代码签名。
4. 产品只给该入口安装自己的钩子，包裹签名为 `void(void* component)`；安装前调用 `beforeOwnHook(publishRva,publishCode)`，启用钩子后用实际detour和trampoline调用 `afterOwnHook`。钩子按该组件真实身份查 Endpoint，调用 `aroundNativePublication`；未登记组件直接走原函数。
5. Endpoint须来自已核对的整机几何注册，包括同Binding代次、具体provider、真实Mesh/Skeleton/AnimClass/AnimInstance身份、真实FName挂点、组件空间端点与净空允许的容差。Tracker会反射回读这四份资源引用，静态资源和AnimInstance的serial0仍按对象表复核。该类不替几何研究授权这些数据。

Tracker只在游戏线程、真实自身钩子回执、原生pending=1、原生帧相符、三项调度状态允许、刷新模式正确、Parts实际处于打开端点时观察。原生返回后验证索引确实发布；两次取位之间复核帧/索引/组件世界矩阵/Parts不变，并比较实际组件空间端点。成功token为真实原生帧+1（只避开空token），包含Binding代次。失败不产生见证，不延用上次结果。

成功结果仅在该钩子尾部同步消费：它证明这一取位窗口使用本帧发布缓冲。**不能直接保存见证供之后发射或跨帧显示使用。** 发射时如需复用，必须在实际发射取位入口再次验证同一帧/缓冲/Binding/Parts，或让发射直接沿用该同步取位结果；当前尚未接入该消费路径。也没有证明隐藏provider一定每帧到达发布入口：收不到合法发布时保持不放行，不能用手动调用刷新函数补证据。

## 包含和生命周期

先在 `weapon_bridge.cpp` 匿名命名空间外包含 `common_bay_animation.hpp`、`native_common_bay_parts.hpp`、`owned_bone_refresh.hpp`，然后在现有 `mount_*` 身份、世界、线程帮助函数已定义后，依次包含本轮三个适配头。头内复用产品现有标准库和Windows声明，不在匿名命名空间里重新包含标准库。

Parts已经被产品钩住的 `0x1ce6d70` 也需要安装前/后的自身回执，否则代码资格检查会拒绝。回执必须来自本产品实际安装，不从“发现一个跳转”自动授权。`registerOwner()` 只接受已完成资源绑定的完整Binding；重建前先归还，再 `forgetOwner()` 并以新generation重新登记。访问器不自动管理全局字段独占。

## 仍需整合线程完成

- 修订旧控制合同，接入四项独立租约和原生观察；不能只把Pause的数字改成4。
- 将几何全7组合注册、具体挂点端点/容差、body/架子资源身份接入真实Host。
- 接入发布钩子和同步显示/发射消费；完成之后才有可提交用户实测的三机候选。
- 按授权编译和实机检查。此分支仅静态分析、源码与文档，不能代替这些结果。

CFA-44的具名骨显隐接口、X-02S三机构共进度、ASF-X的LASM域与FALKEN鼻TLS域不由本轮适配器自动开放。旧object-index中找到函数名只证明历史对象存在；未核对参数/时序前仍不能执行。

历史索引已离线核对 HideBoneByName、UnHideBoneByName、IsBoneHiddenByName 均归属 SkinnedMeshComponent。这可供CFA-44骨支路显隐租约继续研究；本轮没有把历史地址存成新实例句柄，没有猜PBO_None数字，也没有调用这些函数。
