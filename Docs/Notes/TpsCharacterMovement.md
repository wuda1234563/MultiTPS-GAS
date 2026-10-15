# TpsCharacter 移动实现笔记

## 1. 学习目标

- 使用 Enhanced Input 将移动、视角和跳跃动作绑定到 C++ 角色。
- 理解本地输入、角色移动组件和服务器权威之间的关系。
- 实现相机水平朝向下的前后左右移动。

## 2. 类型与职责

- `ATpsCharacter`：绑定 `IA_Move`、`IA_Turn`、`IA_Look` 和 `IA_Jump`。移动读取二维输入并调用 `AddMovementInput`；`IA_Turn` 和 `IA_Look` 分别交给 `Turn`/`LookUp`；跳跃直接绑定 `ACharacter::Jump` 和 `ACharacter::StopJumping`。
- `ATpsPlayerController`：在本地控制器的 `BeginPlay` 中向 Enhanced Input 子系统添加 `IMC_Default`。
- `IMC_Default`：现有输入映射资产，位置为 `/Game/Input/IMC_Default`。
- `IA_Move`：现有移动动作资产，位置为 `/Game/Input/Actions/IA_Move`。

## 3. 运行流程

1. PlayerController 将 `IMC_Default` 添加到本地玩家的 Enhanced Input 子系统。
2. 角色的 `SetupPlayerInputComponent` 将 `MoveAction`、`TurnAction`、`LookAction` 绑定到 `Triggered` 事件，将 `JumpAction` 绑定到 `Started`/`Completed` 事件。
3. `Move` 读取二维输入值。
4. 仅保留控制器旋转的 Yaw，计算水平前方向和右方向。
5. 使用 Y 分量调用前后移动，使用 X 分量调用左右移动。
6. `CharacterMovementComponent` 负责实际移动和网络同步。`Turn` 调用 `AddControllerYawInput`，`LookUp` 调用 `AddControllerPitchInput`。

## 4. 网络行为

- 输入由拥有角色的本地客户端产生。
- `AddMovementInput` 不直接修改服务器权威位置；角色移动组件通过 Unreal 内置网络移动流程同步移动结果。
- 服务器最终验证角色位置和移动状态。
- 本功能没有新增自定义移动 RPC。

## 5. 配置清单

- 在 `BP_TpsCharacter` 默认值中设置 `MoveAction` 为 `/Game/Input/Actions/IA_Move`。
- `TurnAction` 设置为 `/Game/Input/Actions/IA_Turn`，`LookAction` 设置为 `/Game/Input/Actions/IA_Look`，`JumpAction` 设置为 `/Game/Input/Actions/IA_Jump`。
- 创建 `BP_TpsPlayerController` 后，将其 `DefaultMappingContext` 设置为 `/Game/Input/IMC_Default`。
- 在实际使用的 GameMode 中设置 `BP_TpsCharacter` 和 `BP_TpsPlayerController`。

## 6. 常见错误

- `MoveAction` 未设置：输入组件存在，但不会绑定移动事件。
- `IMC_Default` 未添加：不会产生 Enhanced Input 事件。
- GameMode 仍使用旧 Pawn 或旧 PlayerController：新类不会参与运行。
- 摄像机俯仰导致角色向上移动：检查 `Move` 中是否只保留 Yaw。

## 7. 当前限制与验证

当前已完成 `ATpsCharacter` 的 Enhanced Input 移动绑定和 `ATpsPlayerController` 的本地 Mapping Context 注册。按项目约定，不安排 PIE 验证，由项目作者自行验证。
