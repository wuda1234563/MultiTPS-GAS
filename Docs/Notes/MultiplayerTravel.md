# 多人地图 Travel 功能模块笔记

## 1. 学习目标

- 理解 Unreal 多人游戏中服务器、拥有客户端和模拟客户端的地图切换关系。
- 区分 `ServerTravel`、`ClientTravel`、Seamless Travel 和 Non-Seamless Travel。
- 理解大厅达到人数后由服务器统一进入对战地图的实现方式。
- 掌握 Transition Map、地图配置和多人 PIE 调试方法。

这套流程可以迁移到大厅进入比赛、回合结束进入下一回合、重新开始地图和服务器切换游戏阶段等功能。

## 2. 功能目标与验收标准

当前功能是：大厅达到配置的人数后，服务器权威地将所有已连接玩家带入对战地图。

- 人数未达到阈值时，所有玩家留在大厅。
- 达到阈值时，只由服务器发起一次地图切换。
- 主机和远程客户端进入同一张对战地图。
- 对战地图使用自己的 GameMode、Pawn 和 PlayerStart 配置生成角色。
- 客户端不能通过修改本地人数或地图路径直接开始比赛。
- 地图路径为空、地图不存在或 Travel 配置错误时，日志能够定位问题。

## 3. 课程图片中的两个 Travel API

### 3.1 `UWorld::ServerTravel`

`ServerTravel` 是服务器切换整个网络会话地图的入口，只应由服务器调用。

```text
服务器调用 ServerTravel
        ↓
服务器开始切换到新地图
        ↓
所有已连接客户端跟随服务器
```

它适合大厅满员后进入比赛，因为人数、地图和比赛开始决定都属于服务器权威状态。客户端只发送连接或准备请求，不能自行决定目标地图。

### 3.2 `APlayerController::ClientTravel`

`ClientTravel` 作用取决于调用者：

- 客户端调用：让该客户端连接或前往一个新的服务器地址。
- 服务器针对某个 `PlayerController` 调用：让指定玩家客户端前往目标地图或服务器。

课程图片中“服务器调用 `ClientTravel`”描述的是服务器向玩家连接发送 Travel 指令的方向。非无缝切图通过这类客户端通知处理跳转；无缝路径还会使用专门的客户端通知与无缝迁移流程。对于整场大厅切换，业务代码应使用 `ServerTravel`，让引擎协调连接，不能把这张图理解为两种模式都只靠相同的 `ClientTravel` 调用完成。

不要把每个客户端分别调用 `ClientTravel` 当作大厅整体切图方案，否则服务器和客户端可能进入不同地图，房间状态也会失去统一权威。

## 4. Seamless Travel 与 Non-Seamless Travel

### 4.1 Non-Seamless Travel

Non-Seamless Travel 会关闭旧世界的网络连接，再建立或恢复连接进入新地图：

```text
旧地图
  ↓
断开连接
  ↓
加载新地图
  ↓
重新连接
```

优点是流程简单，适合主菜单进入服务器、完全重启服务器或不需要保留会话状态的场景。缺点是客户端会经历明显的断线和重新连接，Session、PlayerController 和临时状态需要重新建立。

### 4.2 Seamless Travel

Seamless Travel 尽量保持同一个网络会话，避免一次完整的断线重连：

```text
旧地图
  ↓
过渡阶段
  ↓
新地图
```

它适合大厅进入比赛和回合之间切换。地图仍然需要加载，客户端仍可能看到加载过程；“无缝”表示网络会话和玩家迁移过程更连续，不表示不加载资源。

项目中的开关是：

```cpp
bUseSeamlessTravel = true;
```

在当前实现中，`ALobbyGameMode` 的 `PostLogin` 统计玩家数量，达到阈值后调用：

```cpp
GetWorld()->ServerTravel(MapPath, true);
```

第二个参数 `true` 表示使用绝对 URL，重新计算目标 URL，不能将它理解为“启用无缝切图”。是否无缝由出发地图的 GameMode 设置和引擎运行条件共同决定。绝对 URL 也会影响旧 URL 选项的继承，因此不能假定非无缝回退仍会保留原来的 `?listen`。目标路径由 `TSoftObjectPtr<UWorld>` 转换得到。

`OpenLevel` 常用于本地打开地图或初次以 `?listen` 建立主机；它不会像 `ServerTravel` 一样协调既有连接整体换图。初次连接服务器、首次加载世界、重新建立连接等情形仍可能需要非无缝流程。

## 5. 为什么需要 Transition Map

Transition Map 是地图切换过程中的轻量中间地图，通常只包含最小的世界设置和必要资源：

```text
LobbyMap → TransitionMap → BlasterMap
```

它的主要作用是：

1. 先让旧大厅地图退出运行；
2. 降低大厅和对战地图同时驻留造成的内存峰值；
3. 为新地图加载提供稳定的中间阶段。

在 UE 5.7 中，即使没有指定 Transition Map，引擎仍会创建临时空世界作为过渡；省略配置不代表完全取消过渡阶段，也不代表必须让两张大地图同时驻留。自建过渡地图用于控制该阶段的世界内容，应保持轻量。它不能保证所有旧资源都立即释放，持久对象引用仍会影响资源生命周期。项目当前配置为：

```ini
TransitionMap=/Game/Maps/TransitionMap.TransitionMap
```

本项目不把 Transition Map 当作比赛地图，不在其中配置正式比赛和角色生成逻辑。内存峰值是否降低需要用性能工具测量，不能仅靠切图成功判定。

## 6. 玩家状态和对象生命周期

Seamless Travel 不会自动保留所有对象：

- `PlayerController` 类型兼容时可以保留，目标 GameMode 要求其他控制器类型时可能替换；
- `PlayerState` 可参与迁移，也可能通过 `CopyProperties` 等流程复制到新实例；自定义状态不能仅凭启用 Seamless Travel 就认为会自动保留；
- Pawn 通常在新地图重新生成；
- 新地图会创建新的 GameMode 和 GameState；
- 需要跨地图保留的队伍、名字、分数和准备状态，应放在合适的 `PlayerState` 或会话状态中；
- 不应把运行时状态写回 DataAsset，也不应假设大厅中的普通 Actor 会自动存在于对战地图。

`GameInstance` 存在于各进程中，跨地图保留但不会自动网络复制；客户端自己的 GameInstance 不能作为服务器的权威比赛数据。普通 Actor 若确实需要保留，再研究 `GetSeamlessTravelActorList`，本功能未增加自定义保留列表。

## 7. 当前项目的实现映射

### C++

`ALobbyGameMode` 负责稳定规则：

- `MinimumPlayers`：开始比赛所需的最少玩家数；
- `MatchMap`：`TSoftObjectPtr<UWorld>`，由蓝图选择目标地图；
- `bTravelStarted`：防止多个玩家同时登录时重复 Travel；
- `PostLogin`：玩家登录后触发服务器人数检查；
- `TryStartMatchTravel`：校验权限、地图引用和人数，然后调用 `ServerTravel`。

### 蓝图

`BP_LobbyGameMode` 负责易变配置：

1. 将父类设置为 `ALobbyGameMode`；
2. 将 `MinimumPlayers` 设置为当前测试阈值；
3. 在 `MatchMap` 资源选择器中选择对战地图；
4. 在大厅地图 World Settings 中设置 GameMode Override。

### 地图与打包

- 大厅地图使用 `BP_LobbyGameMode`；
- 对战地图使用正确的对战 GameMode、Default Pawn Class 和 PlayerStart；
- Transition Map 在 `DefaultEngine.ini` 中配置；
- 大厅、Transition Map 和对战地图加入 Cook Maps；
- 当前对战地图 `/Game/Maps/BlasterMap` 已加入 `DefaultGame.ini` 的 `MapsToCook`。

## 8. 网络执行流程

```text
客户端连接大厅
    ↓
服务器创建 PlayerController 和 PlayerState
    ↓
服务器执行 ALobbyGameMode::PostLogin
    ↓
服务器统计 GetNumPlayers()
    ↓
人数达到 MinimumPlayers
    ↓
服务器设置 bTravelStarted
    ↓
服务器调用 ServerTravel
    ↓
服务器和所有客户端进入目标地图
    ↓
新地图 GameMode 生成 Pawn
```

关键边界：

- `GetNumPlayers()` 的判断在服务器 GameMode 中完成；
- `ServerTravel` 的最终结果由服务器决定；
- `GameState->PlayerArray` 更适合复制玩家列表和 UI 展示；
- 如果条件变成“准备人数达到阈值”，应由服务器根据 `PlayerState` 的准备状态统计，而不是直接统计连接人数。

### 人数统计的精确区别

当前类继承 `AGameMode`，UE 5.7 的 `GetNumPlayers()` 返回 `NumPlayers + NumTravellingPlayers`，分别表示当前玩家计数和仍处于迁移流程的玩家计数；旁观者有独立计数。因此它不是简单遍历所有 PlayerController，也不等于“已完成地图加载并准备好参战”的人数。

`GameState->PlayerArray.Num()` 数的是数组中的 `PlayerState` 条目，可能包括旁观者或机器人对应的状态；客户端上的数组还受复制时序影响。GameState 和 PlayerState 可在客户端用于展示，GameMode 仅存在于服务器。两种统计在普通双人大厅常常相同，但不能在迁移、旁观和准备状态场景中互换。

### 地图软引用

```cpp
TSoftObjectPtr<UWorld> MatchMap;
const FString MapPath = MatchMap.ToSoftObjectPath().GetLongPackageName();
```

软引用提供编辑器地图选择器且不会因为读取路径就加载世界，无需 `LoadSynchronous()`。应使用 `IsNull()` 判断是否配置，不能用“资源尚未加载时为 false”的 `IsValid()` 拒绝有效软引用。长包名为 `/Game/Maps/BlasterMap`，避免把对象路径 `/Game/Maps/BlasterMap.BlasterMap` 当作 Travel URL。非空路径不证明资源存在或已经 Cook；本项目用显式 Cook Maps 清单保证发行资源包含地图。

### 生命周期边界

首次正常登录会走 `PostLogin`；已有玩家通过 Seamless Travel 进入新 GameMode 时通常走 `HandleSeamlessTravelPlayer`、`HandleStartingNewPlayer` 等迁移入口，不会重新对每人触发普通 `PostLogin`。当前大厅作为普通登录入口可用；以后增加“比赛无缝返回大厅后再次开局”时，需要额外覆盖迁入玩家检查，不能直接认为现有钩子已支持循环比赛。

## 9. 配置清单

### GameMode

- 大厅使用 `BP_LobbyGameMode`；
- `MinimumPlayers` 已设置；
- `MatchMap` 已选择有效地图；
- 对战地图使用自己的 GameMode。

### 地图

- 大厅地图存在有效的 PlayerStart；
- 对战地图存在足够的 PlayerStart；
- 对战地图的 Default Pawn Class 不为空；
- 对战地图的 PlayerController 和 Pawn 蓝图引用正确；
- Transition Map 路径存在且拼写正确。

### PIE 与打包

```ini
[ConsoleVariables]
net.AllowPIESeamlessTravel=1
```

该变量用于允许 PIE 多客户端测试 Seamless Travel。打包运行时仍必须确认目标地图被加入 `MapsToCook`。

## 10. 验证与调试方法

### PIE 验证

使用 Listen Server + 一个客户端：

1. 第一个玩家进入大厅，确认人数不足时不切图；
2. 第二个玩家进入大厅，确认服务器只发起一次 Travel；
3. 确认主机和远程客户端都进入对战地图；
4. 确认新地图生成两个角色；
5. 将 `MatchMap` 留空，确认日志报错且不会执行 Travel；
6. 模拟玩家在切图前断开，确认没有重复 Travel 或无效状态。

### 日志关键词

按时间顺序搜索：

- `ProcessServerTravel`：服务器是否发起 Travel；
- `Seamless travel`：PIE 是否禁用了无缝切图；
- `Browse`：客户端是否收到目标地址；
- `LoadMap`：服务器和客户端是否真正加载目标地图；
- `Game class is`：目标地图使用了哪个 GameMode；
- `TravelFailure`：Travel 是否失败；
- `Initial Connect Diagnostics`：连接是否停在无响应状态；
- `Failed to load`：地图或依赖资源是否找不到。

### 调试顺序

```text
确认权威者
  → 确认服务器状态和请求
  → 确认 Travel 指令到达
  → 确认目标地图加载
  → 确认目标 GameMode
  → 确认 Pawn 生成
  → 最后检查角色表现和 UI
```

先找最早的 Warning、Error 或 Failure。后面的“没有角色”可能只是地图没有加载完成的结果，不能直接证明 Pawn 类有问题。

## 11. 常见错误

- 把 `ClientTravel` 当成所有客户端整体换图的主要入口；
- 只设置 `bUseSeamlessTravel`，却忘记 PIE 的 `net.AllowPIESeamlessTravel`；
- 只在 PIE 中验证，没有把目标地图加入 Cook Maps；
- 对战地图仍然使用大厅 GameMode，导致角色或比赛规则不正确；
- 只观察主机画面，不查看远程客户端日志；
- 用 `GameState->PlayerArray.Num()` 代替准备人数统计；
- 认为 Seamless Travel 会自动保留所有 Pawn 和 Actor；
- 地图软引用为空，或资源移动后蓝图引用没有重新保存。

## 12. 后续扩展

- 用 `PlayerState` 增加准备状态、队伍和选择角色信息；
- 用倒计时和取消规则避免玩家刚好掉线时立即开始比赛；
- 将房间规则迁移到 DataAsset；
- 接入 Online Session、Dedicated Server 和匹配队列；
- 对大地图使用更轻量的 Transition Map，并分析加载时间和内存峰值；
- 增加多人 PIE 自动化测试和网络延迟、丢包测试。

## 13. 蓝图、GAS 与失败边界

- 大厅蓝图只设置默认人数、地图和框架类；人数检查由 C++ 的 `PostLogin → TryStartMatchTravel → ServerTravel` 完成，不需要 Tick 或逐客户端的切图节点。
- 本功能未接入 GAS，不新增 Ability、Effect、Tag、Attribute、AbilityTask、Montage 或 Cue。未来 ASC 若挂在 Pawn 上，需要研究重生后的重新初始化；挂在 PlayerState 上也要按迁移生命周期重新绑定 Avatar，不能假设指针永久有效。
- 当前没有客户端请求切图的自定义 RPC，因此没有可供客户端直接填写地图地址的接口；防重复标记也不是为网络重传手工去重，而是防止业务回调多次发起切图。
- `ServerTravel` 的返回值与异步 Travel 完成不是同一件事。当前代码设置标记后不恢复，尚未实现同步拒绝、异步失败后的重试或退回大厅；不要把该功能描述为已经具备失败恢复能力。
- 切图开始后断线不会自动撤销已经发出的 Travel。延迟、丢包、重复输入、死亡和取消等清理场景尚未全部验证，本模块也未绑定额外委托或预测状态。

## 14. 验证结果、复盘与可复用提示词

### 验证记录

- 2026-10-15：项目作者确认 Listen Server 加一个客户端能够完成大厅进入对战地图，原客户端停留大厅问题已解决。
- 同日修复后日志出现 `Set CVar [[net.AllowPIESeamlessTravel:1]]`、`SeamlessTravel to: /Game/Maps/BlasterMap` 和 `SeamlessTravel finished`，支持无缝流程已实际执行。
- 未完成的独立验收：打包运行、Dedicated Server、高延迟/丢包、切图中断线、失败恢复、自定义状态迁移和资源内存测量；前文检查项是测试方法，不代表均已通过。
- 相关实现与配置见 [大厅实现笔记](LobbySeamlessTravel.md)，故障证据与复现见 [PIE 客户端停留大厅 Bug](../Bugs/2026-10-15-LobbyTravelClientStuck.md)。
- 仓库当前 `.gitignore` 排除 `Content/`；地图和蓝图由项目作者本地管理。本次 Git 同步仅覆盖源码、配置和文档，不包含这些二进制资产，其他机器需要按配置步骤提供它们。

### 可复用步骤

确定同一服务器换图还是连接其他服务器 → 明确权威者与触发条件 → 选择 Travel API 和模式 → 配置地图与 GameMode → 检查对象迁移 → 配置 PIE/Cook → 分别验证主机与远端 → 记录失败恢复边界。

### 可复用提示词

- 实现同类功能：基于现有 GameMode 做服务器权威的多人地图切换，使用蓝图配置人数和地图软引用，明确普通登录与无缝迁入的区别，保持最小改动并记录未覆盖场景。
- 新增变体：为下一回合替换目标地图，检查 GameMode、Pawn、PlayerStart、状态迁移、Cook 清单与重复触发，不复用旧世界对象指针。
- 排障：根据服务器和客户端日志重建 Travel 时间线，先确认运行模式与最早相关异常，再检查目标地图、连接和 Pawn，不在缺少证据时修改角色代码。

### UE 5.7 源码核对入口

- `Engine/Source/Runtime/Engine/Private/GameModeBase.cpp`：`ProcessServerTravel` 的 PIE 开关判断、客户端通知和无缝分支，以及玩家迁移函数。
- `Engine/Source/Runtime/Engine/Private/World.cpp`：`FSeamlessTravelHandler` 对空 TransitionMap 创建临时 World 的流程。
- `Engine/Source/Runtime/Engine/Private/GameMode.cpp`：`AGameMode::GetNumPlayers()` 的具体计数。
