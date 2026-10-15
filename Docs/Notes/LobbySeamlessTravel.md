# 大厅无缝切换对战地图实现笔记

相关学习资料：[多人地图 Travel 功能模块笔记](MultiplayerTravel.md)；相关故障：[PIE 客户端停留大厅](../Bugs/2026-10-15-LobbyTravelClientStuck.md)。

## 1. 学习目标

- 理解大厅由服务器统计人数并决定何时开始比赛。
- 区分 `ServerTravel`、`ClientTravel`、Seamless Travel 和 Non-Seamless Travel。
- 掌握 Unreal 多人地图切换时的服务器权威边界。

## 2. 需求与验收标准

`ALobbyGameMode` 在服务器上统计已登录玩家。当人数达到 `MinimumPlayers` 时，服务器只发起一次 `ServerTravel`，所有连接客户端进入 `MatchMap` 指定的对战地图。

- 未达到人数时保持大厅地图。
- 达到人数后主机和客户端进入同一张对战地图。
- 客户端不能通过本地人数或地图路径直接触发切图。
- 切图触发只执行一次；目标路径为空时记录错误且不切图。

## 3. 核心概念

- `ServerTravel` 由服务器切换整个网络会话的地图，适合大厅整体进入比赛。
- `ClientTravel` 只针对一个 `PlayerController`，适合客户端连接或单个玩家跳转，不是本功能入口。
- Seamless Travel 保持多人会话并减少完整断线重连；目标地图仍然需要加载。
- Transition Map 是轻量的过渡世界；UE 5.7 未指定资源时会创建临时空世界。项目可配置自己的小地图，不应把“没有手动配置”理解为没有过渡阶段。

## 4. 类型与职责设计

- `ALobbyGameMode`：服务器权威地统计人数并调用 `ServerTravel`。
- `BP_LobbyGameMode`：在编辑器中设置 `MinimumPlayers` 和 `MatchMap`。
- 大厅地图：World Settings 中使用 `BP_LobbyGameMode`。
- 对战地图：使用自己的 GameMode 和玩家生成配置。

## 5. 配置步骤

1. 在 Unreal Editor 中创建 `BP_LobbyGameMode`，父类选择 `ALobbyGameMode`。
2. 设置 `MinimumPlayers = 2`，便于双客户端 PIE 验证。
3. 在 `MatchMap` 资源选择器中选择正式对战地图，例如 `/Game/Maps/MatchMap`。
4. 在大厅地图的 World Settings 中设置 GameMode Override 为 `BP_LobbyGameMode`。
5. 创建正式对战地图，并确认 `MatchMap` 引用的是该地图。
6. 如地图资源较大，创建小型 Transition Map，并在项目地图设置中配置 Transition Map。
7. 确认大厅、Transition Map（如果使用）和对战地图加入 Cook Maps。

## 6. 运行时流程

1. 客户端连接大厅，服务器创建玩家控制器。
2. 服务器执行 `ALobbyGameMode::PostLogin`。
3. `TryStartMatchTravel` 检查权限、重复触发标记、地图路径和当前玩家数。
4. 达到阈值后设置 `bTravelStarted`，将 `MatchMap` 转换为长包名，再调用 `GetWorld()->ServerTravel(MapPath, true)`。
5. Seamless Travel 让连接的客户端跟随服务器进入对战地图。

## 7. 网络行为

- 服务器拥有玩家数量和切图决定权。
- `PostLogin` 和 `ServerTravel` 的关键逻辑只在服务器执行。
- `PlayerController`、`PlayerState` 的跨图保留由 Unreal 的 Seamless Travel 规则管理；Pawn、GameMode 和 GameState 通常在新地图重新创建。
- 需要跨地图保留的队伍、名字和分数等状态应放在合适的 `PlayerState` 或会话系统中。
- 当前实现面向单个 Listen Server 房间，不包含在线匹配服务、Dedicated Server 编排或跨服务器转移。

## 8. 验证与调试

使用 Listen Server + PIE，启动两个客户端：

- 第一个玩家登录后仍停留在大厅。
- 第二个玩家登录后服务器日志出现一次人数阈值和目标地图日志。
- 主机和客户端都加载对战地图。
- 未配置 `MatchMap` 时不会切图，并出现错误日志。
- 检查日志确认没有第二次 `ServerTravel`。
- 配置 Transition Map 后观察大厅资源释放和对战地图加载过程。

PIE 多客户端测试时，如果日志出现 `Seamless travel is disabled in PIE`，需要确认项目配置中的 `net.AllowPIESeamlessTravel=1` 已生效；也可以在 PIE 控制台执行 `Net.AllowPIESeamlessTravel 1` 后重新测试。若目标地图用于打包运行，还必须将目标地图加入 `MapsToCook`。

## 9. 已知限制与后续扩展

- 当前人数是 GameMode 看到的已登录玩家数，不是在线服务的匹配队列人数。
- 玩家在切图前断开时，具体行为由 Unreal 的 Travel 生命周期处理；正式项目可增加准备状态、倒计时和取消规则。
- 后续可将房间规则迁移到 DataAsset，并接入在线 Session、Dedicated Server 和队伍状态管理。

## 10. 已验证结果

2026-10-15，项目作者确认修复 PIE 开关后主机和客户端可以正常完成切图。修复后日志记录了 CVar 生效和 `SeamlessTravel finished`。打包、丢包、高延迟、断线和 Travel 失败后的恢复尚未验证；当前防重复标记在发起切图后不会自行恢复。
