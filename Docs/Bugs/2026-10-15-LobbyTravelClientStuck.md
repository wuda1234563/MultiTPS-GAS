# 大厅切图后客户端停留在大厅问题

学习资料：[多人地图 Travel 功能模块笔记](../Notes/MultiplayerTravel.md)；实现配置：[大厅实现笔记](../Notes/LobbySeamlessTravel.md)。

## 1. 问题概述

### 现象

使用 Unreal Editor 的 Listen Server 和一个客户端进行 PIE 测试时：

- 主机进入大厅后，达到人数阈值并进入对战地图；
- 另一个客户端停留在大厅地图；
- 客户端没有正常出现角色实体；
- 服务器日志显示已经执行了 `ServerTravel`。

### 预期

服务器达到大厅人数阈值后，主机和所有已连接客户端都应进入同一个对战地图，并在新地图中按 GameMode 配置生成角色。

## 2. 调试过程

### 第一步：确认问题属于哪一段流程

先把完整流程拆成四段，而不是直接修改角色生成代码：

```text
大厅是否正确启动
    → 服务器是否统计到足够人数
    → 服务器是否发起 ServerTravel
    → 客户端是否完成目标地图加载和角色生成
```

服务器日志中出现了：

```text
ProcessServerTravel: /Game/Maps/BlasterMap
```

这证明人数判断和服务器调用 `ServerTravel` 的代码已经执行，问题范围缩小到 Travel 过程、PIE 网络设置或目标地图初始化。

### 第二步：搜索关键日志，而不是只看屏幕表现

围绕这些关键词搜索日志：

- `ProcessServerTravel`
- `Seamless travel`
- `Travel Failure`
- `LoadMap`
- `Welcomed by server`
- `Initial Connect Diagnostics`
- `Failed to load`

日志中发现了直接原因：

```text
ProcessServerTravel: Seamless travel is disabled in PIE, set net.AllowPIESeamlessTravel=1 to enable.
```

这条警告比“客户端卡住”更接近根因。代码设置了 `bUseSeamlessTravel = true`，但 PIE 默认不允许 Seamless Travel，因此测试环境没有按预期执行无缝切图流程。

### 第三步：区分主机成功和客户端失败

日志还显示服务器能够加载：

```text
LoadMap: /Game/Maps/BlasterMap
Game class is 'BP_TpsGameMode_C'
```

所以目标地图本身可以被服务器加载，`BP_TpsGameMode` 也被识别。主机成功不能证明客户端完成了同一流程；多人问题必须分别观察服务器、主机客户端和模拟客户端的日志。

### 第四步：检查打包配置和 PIE 配置

进一步检查发现：

- 项目已经配置了 `TransitionMap`；
- 目标地图 `BlasterMap` 没有加入 `MapsToCook`；
- PIE 没有启用 `net.AllowPIESeamlessTravel`。

其中 PIE 变量决定本次测试是否采用无缝流程；`MapsToCook` 是本项目显式管理打包地图的措施，不是该次 PIE 故障的直接修复。非无缝 Travel 本身受引擎支持，不能认为“回退到非无缝”必然导致客户端失败：当前绝对 Travel URL 为 `/Game/Maps/BlasterMap`，不包含 `?listen`，日志还显示旧 NetDriver 关闭后客户端重连无响应。这说明回退后的监听连接路径也需要单独检查；本次通过启用预期的无缝流程解决，没有另行验证非无缝回退方案。

## 3. 修复内容

在 `Config/DefaultEngine.ini` 中加入：

```ini
[ConsoleVariables]
net.AllowPIESeamlessTravel=1
```

它允许 PIE 多客户端执行 Seamless Travel，使测试环境与 `ALobbyGameMode` 中的：

```cpp
bUseSeamlessTravel = true;
```

保持一致。

在 `Config/DefaultGame.ini` 中加入：

```ini
+MapsToCook=(FilePath="/Game/Maps/BlasterMap")
```

这样打包后的服务器和客户端才会包含目标地图。该配置不是解决当前 PIE 警告的替代品，而是保证后续非编辑器运行时不会因地图未打包而 Travel 失败。

## 4. 验证方法

修改配置后重启 Unreal Editor，再使用 Listen Server + 一个客户端测试：

1. 第一个玩家进入大厅，确认尚未达到人数阈值。
2. 第二个玩家进入大厅，确认服务器只打印一次人数达到阈值的日志。
3. 搜索日志，确认不再出现 `Seamless travel is disabled in PIE`。
4. 确认服务器、主机客户端和模拟客户端都加载 `BlasterMap`。
5. 确认新地图的 GameMode、Default Pawn Class 和 PlayerStart 正确，两个玩家都生成角色。
6. 进行打包测试时，再确认 `BlasterMap` 已进入 Cook Maps。

## 5. 这次调试中容易走错的方向

- 看到客户端没有角色就先修改 Pawn 或 PlayerController。实际上客户端甚至没有完成正确的地图 Travel，角色生成只是后续现象。
- 只看主机画面。主机成功只说明服务器和本地世界加载成功，不能证明远程客户端收到了并完成了 Travel。
- 看到 `ServerTravel` 被调用就认为流程成功。调用成功和客户端完成目标地图加载是两个不同检查点。
- 把 PIE 日志警告当成普通提示。`Seamless travel is disabled in PIE` 已经明确给出了修复方向。
- 只修复 PIE 配置，不检查 Cook Maps。这样编辑器测试可能通过，但打包后的地图仍可能加载失败。

## 6. 可复用的多人网络 Bug 调试方法

### 6.1 先画出权威链路

对每个网络功能先回答：

1. 谁拥有状态？
2. 谁触发请求？
3. 谁验证请求？
4. 谁产生最终结果？
5. 结果通过什么复制或 RPC 到达其他端？

本案例中，人数和切图决定由服务器拥有；客户端只是连接大厅；服务器调用 `ServerTravel`；所有客户端跟随服务器切换。

### 6.2 用日志验证每个边界

不要只打印“开始切图”。至少要分别确认：

- 人数统计值和阈值；
- `ServerTravel` 的目标路径；
- 是否启用 Seamless Travel；
- 目标地图是否真正 `LoadMap`；
- 每个客户端是否收到 Browse/Travel；
- 新地图是否创建了正确 GameMode；
- Pawn 是否成功生成。

### 6.3 先查最早的异常

日志里后面的“没有角色”通常是前面 Travel 或连接失败的结果。调试时按时间顺序寻找最早的 Warning、Error 或 Failure，而不是从最后一个可见现象倒推代码。

### 6.4 区分环境问题和业务代码问题

同一段代码可能在独立服务器、编辑器 PIE 和打包版本中受到不同设置影响。遇到多人 Travel 问题，应分别检查：

- PIE 专用控制台变量；
- Listen Server 与 Dedicated Server 拓扑；
- 地图是否加入 Cook Maps；
- 地图路径是否正确；
- 目标地图 GameMode 和 Pawn 配置；
- Transition Map 是否存在且路径正确。

### 6.5 修复后重复验证原场景和边界场景

至少覆盖：

- 未达到人数阈值；
- 达到阈值后只触发一次；
- 主机和远程客户端同时进入目标地图；
- 目标地图路径为空；
- 客户端在切图前断开；
- 打包运行时目标地图存在。

## 7. 当前结论与后续注意事项

### 修复后证据

2026-10-15，项目作者确认原问题解决。修复后的运行日志包含：

```text
LogConfig: Set CVar [[net.AllowPIESeamlessTravel:1]]
LogWorld: SeamlessTravel to: /Game/Maps/BlasterMap
LogWorld: ----SeamlessTravel finished in 2.21 seconds ------
LogWorld: ----SeamlessTravel finished in 5.01 seconds ------
```

两条完成日志来自同次多世界运行，不据此将每条直接指定给某个角色或客户端。结论依据是日志确认流程生效，加上项目作者确认两端行为恢复；打包和非无缝回退未被这次测试覆盖。

- 本次问题的直接根因是 PIE 默认禁用 Seamless Travel，而项目逻辑要求使用 Seamless Travel。
- `net.AllowPIESeamlessTravel=1` 解决编辑器多人测试环境不一致的问题。
- `MapsToCook` 配置保证打包版本包含目标地图。
- 客户端没有角色是 Travel 未正确完成或目标地图初始化未完成后的表现，不能单独作为 Pawn 配置错误的证据。
- 如果未来仍出现客户端卡住，应优先检查客户端日志中的 `TravelFailure`、`Browse`、`LoadMap` 和连接诊断，再检查新地图的 GameMode 与 Pawn 配置。
