# MCP 并发、连接隔离与 Debug Console

## 执行模型

TCP 层允许多个客户端同时接入，但所有会修改或读取 UE Editor/UObject 状态的命令都会进入同一个 FIFO 队列，并在 Game Thread 上逐条执行。调用方会阻塞到自己的命令完成，因此不会出现两个 AI 同时改写编辑器状态的情况。

每条请求建议包含：

```json
{
  "command": "ping",
  "client_id": "stable-ai-client-id",
  "request_id": "unique-request-id",
  "params": {}
}
```

旧客户端不发送 `client_id` 时仍可使用，但会进入共享的 `legacy` 上下文，不具备连接级 target 隔离。

## 多 UE 实例与连接

UE 实例的缺省端口是 `0`：不尝试占用固定端口，而是直接由操作系统为每个编辑器分配唯一动态端口。实例会在用户级共享目录 `%LOCALAPPDATA%/UmgMcp/instances` 发布实际端点，因此一个 Python/Codex 前端能发现同时运行的不同项目。正常退出时记录会删除；Python 前端也会用 `server_info` 验证记录，自动忽略异常退出留下的失效记录。

可以把模型理解为“人、笔、纸”：AI 是人；连接会话和它持有的 Attention 是笔；UE 项目是纸。同一个项目可以同时存在多支笔，不同 AI 的笔分别保留自己的 target/graph/cursor 上下文，而底层 UObject 命令仍由全局 FIFO 顺序执行。

Python MCP 前端始终提供三个基础工具：

- `list_umg_mcp_servers`：读取并验证用户级 UE 实例发现记录，只返回当前可连接的实例。
- `connect_umg_mcp(host, port, target?, exclusive?)`：把当前 AI 会话绑定到指定 UE 实例，可同时绑定 target。
- `get_umg_mcp_connection`：查询当前端点、客户端 ID 和 UE 侧连接状态。

也可以通过环境变量指定初始连接：`UMG_MCP_HOST`、`UMG_MCP_PORT`、`UMG_MCP_CLIENT_ID`、`UMG_MCP_CLIENT_NAME`、`UMG_MCP_EXCLUSIVE`、`UMG_MCP_DISCOVERY_DIR`。

## Target 隔离

`connect` 会为 `client_id` 创建会话。未显式传入 target 时，会快照当前 UE 编辑器 target 作为该连接的缺省 target。每条后续命令执行前，桥接层都会恢复该连接自己的以下上下文：

- UMG asset 与 focused widget
- Blueprint graph、cursor node
- Animation
- Material/HLSL material

执行后再把变化写回该连接的会话。缺省 `exclusive=true`；一个连接成功采用 target 后，其他连接尝试采用同一 target 会收到 `code: "target_locked"`，而不会覆盖前者。`disconnect` 会释放该连接持有的租约。

底层协议还支持 `connect`、`disconnect`、`server_info` 和 `list_connections` 命令。

## Debug Console

在 UE 的 **Tools → UMG MCP Debug Console** 打开调试窗口；Widget Blueprint 工具栏也有 **MCP Debug** 按钮。

左侧可输入 AI 实际发送的完整 JSON，点击“按真实管线执行”。消息不会走测试旁路，而是进入与 TCP 请求相同的连接、target 恢复、租约验证和命令执行路径。窗口显示：

- 全局 FIFO 序号及 queued/completed/error 状态
- client ID、request ID、command
- 原始请求、完整响应与执行耗时
- 当前 server instance ID 和实际监听端口

第一次用 `debug-ui` 客户端模拟时，先执行面板提供的“连接模板”，再执行 Ping 或编辑命令。
