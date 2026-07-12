# LiteRT-LM UE5 集成开发手册 (面向 AI/架构师)

## 1. 架构目标
本 DLL (litert_lm_wrapper.dll) 作为 **"ABI 防火墙"**，旨在消除虚幻引擎与底层大模型库（Abseil, Protobuf, Minja）之间的符号冲突。

## 2. 状态机模型与性能优化

### 2.1 KV Cache 的物理存在：`LiteRtLmConversation`
*   **核心逻辑**：每个 `Conversation` 句柄在底层对应一个 `Session` 及其关联的 **GPU 显存缓存 (KV Cache)**。
*   **多 Agent 切换 (零拷贝)**：
    *   在 UE5 侧，建议为每个活跃 NPC 持有一个 `Conversation` 句柄。
    *   由于句柄持久化，切换 NPC 推理时只需切换传入的句柄。底层自动命中 KV Cache，无需重新计算历史上下文。
*   **增量增量 Prefill**：
    *   使用 `AppendUserMessage` 或 `AppendMessageJson` 注入新消息。
    *   这些接口仅执行 Tokenization。当调用 `RunInference` 时，引擎会仅针对“新增部分”进行计算，极大缩短了首字延迟（TTFT）。

### 2.2 线程模型
*   `LiteRtLm_RunInference` 是**异步**的。
*   **回调限制**：回调函数是在底层推理线程触发的。**严禁在回调中直接调用 UE5 的 UI 或组件函数。**
*   **正确做法**：在回调中提取数据，通过 `AsyncTask(ENamedThreads::GameThread, ...)` 或 `FGraphEventRef` 将数据派发回主线程处理。

## 3. 约束解码 (Constrained Decoding) 集成
为了让 AI 稳定输出 JSON 或符合游戏逻辑的格式，请务必利用 `LiteRtLm_SamplingParams` 中的约束功能：
*   **Regex**：用于强制输出状态机指令（如 `[MOVE|ATTACK|IDLE]`）。
*   **JSON Schema**：用于 MCP 工具调用或复杂的结构化数据生成。
*   **Lark Grammar**：用于自定义的 DSL 或复杂逻辑树生成。

## 4. 显存管理建议
*   `LiteRtLm_CreateEngine` 是最耗费资源的操作。
*   建议在关卡初始化时创建一次 Engine，直到关卡结束才销毁。
*   `Conversation` 虽然轻量，但仍占用 KV Cache 显存。如果 Agent 数量巨大（超过 50 个），建议在 UE5 侧建立 LRU 池，通过 `DestroyConversation` 释放不活跃的上下文。

## 5. 符号隔离原则
**警告**：UE5 工程的 `Build.cs` 只能包含 `ThirdParty/LiteRtLm/Include`。
*   不要试图链接底层的 `libLiteRt.dll` 原生符号。
*   不要包含任何 `google` 或 `nlohmann` 命名空间的头文件。
