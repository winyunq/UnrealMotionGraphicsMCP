# LiteRT-LM UE5 插件集成 API 规范 (v3.0)

## 0. 设计理念
本 DLL 作为一个 **"原子化 C 桥接层"**，主要解决 C++ ABI 兼容性问题。它将复杂的 `absl`, `json`, `litert` 依赖隐藏在 `void*` 句柄后，为虚幻引擎提供纯 C 接口。

## 1. 数据结构 (Data Structures)

### 1.1 采样参数 `LiteRtLm_SamplingParams`
```cpp
typedef struct {
    float temperature;      // 采样温度
    float top_p;            // Top-P 采样
    int top_k;              // Top-K 采样
    int max_tokens;         // 本次生成最大 Token 数
    int constraint_type;    // 约束类型 (0:无, 1:Regex, 2:JSON, 3:Lark)
    const char* constraint_string; // 约束内容
} LiteRtLm_SamplingParams;
```

### 1.2 回调结果 `LiteRtLm_Result`
```cpp
typedef struct {
    const char* text_chunk;     // [临时] 文本片段 (打字机效果)
    const char* full_json_chunk; // [临时] 完整 JSON 片段 (包含 MCP/Channels)
    const char* error_msg;      // 错误信息
    int bIsDone;                // 是否结束 (1:是, 0:否)
    float tokens_per_sec;       // 生成速度 (tokens/s)
} LiteRtLm_Result;
```
> **注意**：`text_chunk` 和 `full_json_chunk` 仅在回调期间有效，UE5 侧必须立即拷贝数据。

---

## 2. 核心 API (Core APIs)

### 2.1 生命周期管理
*   **`LiteRtLm_CreateEngine(model_path, backend)`**
    *   加载模型权重。`backend` 支持 "gpu", "cpu" 或多后端配置如 "gpu,gpu,cpu"。
*   **`LiteRtLm_CreateConversation(engine_ptr)`**
    *   创建基础会话。
*   **`LiteRtLm_CreateConversationWithConfig(engine_ptr, json_preface, bEnableConstrainedDecoding)`**
    *   创建高级会话。`json_preface` 可定义 System Prompt、MCP 工具等。

### 2.2 消息同步 (增量 Prefill)
*   **`LiteRtLm_AppendUserMessage(conv_ptr, text)`**
*   **`LiteRtLm_AppendMessageJson(conv_ptr, json_msg)`**
    *   支持多模态输入（图片、音频）及 MCP 工具结果同步。
    *   格式示例：`{"role": "user", "content": [{"type": "image", "path": "..."}, {"type": "text", "text": "..."}]}`

### 2.3 推理控制
*   **`LiteRtLm_RunInference(conv_ptr, params, callback, user_ptr)`**
    *   触发增量计算并启动流式生成。
*   **`LiteRtLm_StopMessage(conv_ptr)`**
    *   立即中断当前会话的生成过程。

---

## 3. 多模态与 MCP 集成建议

1.  **图片阅读**：使用 `AppendMessageJson` 传入包含图片路径或 Base64 数据的 JSON。
2.  **MCP 工具调用**：在回调中检查 `full_json_chunk`，如果包含 `tool_calls` 字段，UE5 侧解析参数并执行本地逻辑，随后通过 `AppendMessageJson` 回传结果。
3.  **多后端优化**：对于多模态任务，建议创建 Engine 时指定 `gpu,gpu,cpu`，让 Vision 和 Audio 处理分摊到硬件。
