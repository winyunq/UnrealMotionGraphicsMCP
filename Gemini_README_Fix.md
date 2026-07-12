# LiteRT-LM Unreal (v1.0)

High-performance, local LLM inference integration for Unreal Engine 5, powered by **[Google's LiteRT-LM](https://github.com/google-ai-edge/LiteRT-LM)**.

This plugin provides a clean, zero-dependency C++ API to run Google's LiteRT-LM models with ultra-low latency. Our core mission is to bridge Google's advanced on-device inference with Unreal's high-performance requirements.

## 🌟 Our Logic & Strategy

The **LiteRT-LM-Unreal** plugin is built on three pillars:

1.  **Encapsulation of LiteRT-LM**: We wrap Google's native capabilities for both **One-Shot Tasks** (stateless) and **Conversational Flows** (stateful).
2.  **Strategic Session Management**: Beyond raw inference, we implemented a **Session/API Key-like** mapping system. By managing `void* ctx` pointers, we give Unreal developers the power to assign, switch, and persist "memories" for individual Agents.
3.  **High-Performance Bridge**: We implement a C-style "firewall" DLL to digest heavy dependencies (Abseil, Protobuf), ensuring your Unreal project remains lightweight and stable.

---

## 🛠️ API Specification

### 1. Configuration & Lifecycle

#### `LiteRtLm_Config`

```cpp
struct LiteRtLm_Config {
    const char* model_path;      // Path to the .bin/model file
    const char* backend;         // "cpu", "gpu" (vulkan/webgpu)
    int max_num_tokens;          // Context window size (KV Cache pre-allocation)
    int num_threads;             // CPU threads (for CPU backend)
    bool enable_benchmark;       // Print performance logs
    bool optimize_shader;        // Enable shader optimization (Windows recommended)
    bool enable_streaming;       // Enable streaming chunk callbacks
};
```

| Function | Description |
| :--- | :--- |
| **`LiteRtLm_GetAutoConfig`** | Generates the best `LiteRtLm_Config` based on VRAM budget. |
| **`LiteRtLm_LoadModel`** | Initializes the engine and pre-allocates VRAM pools. |
| **`LiteRtLm_UnloadModel`** | Releases all GPU and memory resources. |

---

### 2. Inference & Session Management

| Function | Scenario & Strategy |
| :--- | :--- |
| **`LiteRtLm_GenerateOnce`** | **One-Shot Task**. Stateless inference; cache is destroyed immediately. |
| **`LiteRtLm_ChatWithPrompt`** | **Agent Startup**. Creates a persistent session for a given `void* ctx`. |
| **`LiteRtLm_ChatWithContext`** | **Conversational Flow**. Triggers Cache-Hit via `ctx` for incremental prefill. |

---

### 3. Usage Examples (UE5 C++ Style)

#### Example: Multi-turn Conversation (Cache-Hit)

```cpp
// 1. Setup
FLiteRtLmConfig Config;
Config.ModelPath = TEXT("D:/Models/gemma-2b-it.bin");
ULiteRtLmSubsystem::Get()->LoadModel(Config);

// 2. First Turn (Agent A)
// Passing 'this' as context creates a persistent GPU session for this object.
FLiteRtLmUnrealApi::ChatWithPrompt(this, "You are a guide.", "Hello!", OnChunk, OnDone);

// 3. Subsequent Turn (Agent A)
// Using the same 'this' pointer triggers a cache-hit. 
// Only the new message is processed, resulting in near-instant response.
FLiteRtLmUnrealApi::ChatWithContext(this, HistoryJson, OnChunk, OnDone);
```

---

## ⚡ Performance Guidelines

1.  **Session Mapping**: The plugin uses a `TMap<void*, Session*>` (internal) to track sessions. This works similarly to an **API Key** or **Session ID**, allowing the engine to recall the exact KV-cache state for each unique Agent.
2.  **Auto-VRAM Cleanup**: When VRAM is low, the system automatically releases the least recently used (LRU) sessions.
3.  **Low Latency Switching**: Switching between multiple active Agents takes <1ms on an RTX 4060.

---
*Open Sourced under MIT License. Developed by Winyunq Core Engineering.*
