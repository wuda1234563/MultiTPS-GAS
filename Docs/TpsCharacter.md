# TpsCharacter 摄像机与移动

## 功能目标与范围

`ATpsCharacter` 提供第三人称摄像机和基于 Enhanced Input 的相机相对移动。机械臂负责摄像机距离和控制器旋转；角色读取 `IA_Move` 的二维输入并调用 `AddMovementInput`，并分别使用 `IA_Turn` 和 `IA_Look` 处理左右与上下视角。

## 组件与职责

- `USpringArmComponent`：挂接到角色根组件，默认长度为 `300.0f`，使用 Pawn 控制器旋转。
- `UCameraComponent`：挂接到机械臂末端，不重复应用控制器旋转。
- `ATpsCharacter`：绑定 `IA_Move`、`IA_Turn`、`IA_Look` 和 `IA_Jump`，仅使用控制器 Yaw 计算水平前后左右方向；角色启用朝向移动方向，关闭控制器直接驱动角色 Yaw。
- `ATpsPlayerController`：在本地控制器的 `BeginPlay` 中向 Enhanced Input 子系统添加 `IMC_Default`。

## 配置清单

- `MoveAction` 设置为 `/Game/Input/Actions/IA_Move`。
- `TurnAction` 设置为 `/Game/Input/Actions/IA_Turn`。
- `LookAction` 设置为 `/Game/Input/Actions/IA_Look`。
- `JumpAction` 设置为 `/Game/Input/Actions/IA_Jump`。
- `DefaultMappingContext` 设置为 `/Game/Input/IMC_Default`。
- `bOrientRotationToMovement` 设置为 `true`，`bUseControllerRotationYaw` 设置为 `false`，让角色转向移动方向以配合 Idle/Walk/Run 前进动画。
- GameMode 使用对应的角色和 PlayerController 蓝图。

## 网络行为

输入由拥有角色的本地客户端产生。`AddMovementInput` 交给 Character Movement Component 处理，使用 Unreal 内置网络移动流程同步；服务器仍是位置和移动结果的最终权威。本功能不新增自定义移动 RPC。

## 验证与限制

由项目作者在 Unreal Editor 中自行验证 W/A/S/D、摄像机旋转后的移动方向以及 GameMode 配置。本次不安排 PIE 验证。
