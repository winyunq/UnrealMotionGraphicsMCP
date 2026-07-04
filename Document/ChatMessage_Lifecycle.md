# ChatMessage 生命周期（前端展示视角）

## 目标
本文只描述前端消息展示中的“当前活跃 ChatMessage”模型，不混入窗口架构、目录分层或总流程节点。

## 核心概念
- 后端负责产生状态变化，前端只负责订阅、渲染和事件转发。
- ChatMessage 是“活着的消息”状态机：它可以在流式输出期间持续增量更新。
- `ChatList` 只是滚动容器和历史承载面，不是消息概念本体。
- `StartNewChatMessage` 的语义不是“创建一个固定死的对象”，而是“结束当前活跃消息并开始下一条消息”。
- 如果当前没有活跃消息，调用 `StartNewChatMessage` 应当是无操作。

## 生命周期规则
1. 新消息开始时，系统进入一个新的活跃 ChatMessage。
2. 流式内容、状态、错误、完成态都更新到当前活跃消息。
3. 当后端明确切换到下一条消息时，前端收到 `StartNewChatMessage` 信号。
4. 如果已有活跃消息，则将其归档为只读历史，并标记为 complete。
5. 归档后的历史消息不应再被当作当前可编辑对象直接访问。
6. 新消息到来后，再建立新的活跃消息承载后续内容。

## 消息类型
- 用户消息：由输入框触发，进入历史展示。
- Agent 消息：由后端响应流驱动，通常是活跃状态的主要承载对象。
- Task 消息：由 TaskSubsystem 事件触发，作为单独的 UI 节点展示，不与普通 assistant turn 混用。
- MCP 消息：由工具执行/审批回调触发，按请求序列展示，并在审查点展开详细信息。

## 与前端交互的约束
- 前端只接收后端事件，不主动推进业务流。
- 所有按钮、重试、审批、展开、复制、打开目标等操作都只通知后端。
- 前端可以维护当前活跃消息的 UI 状态，但历史消息一旦归档即只读。
- 不同 agent 的消息切换，由后端在切换时触发 `StartNewChatMessage`，前端不自行推断。

## 与 ChatList 的关系
- ChatList 是滚动容器和历史承载面，但它不负责决定后端业务流转。
- ChatList 只维护当前活跃消息的 UI 状态和历史列表的渲染。
- `StartNewChatMessage` 是少数显式暴露的边界信号，用来收束当前消息并准备下一条消息。

## 代码对应
- `SUmgMcpChatList::StartNewChatMessage`
- `SUmgMcpChatList::SetConversationState`
- `TheChatMessageGroupDisplayInList::FinalizeAsHistory`
- `SUmgMcpChatWindow::ExecuteQuestionRequest`
- `SUmgMcpChatWindow::OnAIResponse`
