// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#ifndef LITERT_LM_WRAPPER_H
#define LITERT_LM_WRAPPER_H

#include <stddef.h>

#ifdef _WIN32
  #define DLL_EXPORT __declspec(dllexport)
#else
  #define DLL_EXPORT __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

    // --- 数据结构 ---

typedef struct {
    const char* model_path;      
    const char* backend;         
    const char* tools_json;      

    int max_num_tokens;          
    int num_threads;             
    int prefill_chunk_size;      

    unsigned int bEnableBenchmark         : 1;
    unsigned int bOptimizeShader          : 1;
    unsigned int bEnableVision            : 1;
    unsigned int bEnableAudio             : 1;
    unsigned int bShareConstantTensors    : 1;
    unsigned int bEnableHostMappedPointer : 1;
} LiteRtLm_Config;

    typedef struct {
        float temperature;  // 采样温度
        float top_p;        // Top-P 采样
        int top_k;          // Top-K 采样
        int max_tokens;     // 本次生成最大 Token 数

        /**
         * @brief 强制约束类型 (Constrained Decoding)
         * 0: 无约束, 1: Regex, 2: JSON Schema, 3: Lark Grammar
         */
         int constraint_type;

        /**
         * @brief 约束字符串内容 (如正则表达式、JSON Schema 或 Lark 语法)
         */
         const char* constraint_string;
    } LiteRtLm_SamplingParams;

    typedef struct {
        /**
         * @brief 文本片段指针。
         * @warning 此指针仅在回调函数执行期间有效！
         */
         const char* text_chunk; 

        /**
         * @brief 完整的 JSON 响应片段。
         * 包含模型返回的所有原始信息（如 tool_calls, channels, content 等）。
         */
         const char* full_json_chunk;

        const char* error_msg;
        int bIsDone;
        float tokens_per_sec;   // 实时生成速度
    } LiteRtLm_Result;

    typedef void (*LiteRtLmCallback)(LiteRtLm_Result result, void* user_ptr);

    // --- 0. 硬件与探测接口 ---
    
    // 返回支持的后端列表（如 "cpu,gpu"）
    DLL_EXPORT const char* LiteRtLm_GetAvailableBackends();

    // --- 1. 引擎生命周期与单会话状态接口 ---
    
    // 创建引擎，DLL 内部将自动隐式创建唯一的全局对话状态与活跃 KV 缓存
    DLL_EXPORT void* LiteRtLm_CreateEngine(LiteRtLm_Config config);

    // 销毁引擎，同时自动隐式销毁内部唯一的对话状态
    DLL_EXPORT void LiteRtLm_DestroyEngine(void* engine_ptr);

    // 向全局会话追加用户消息 (支持多模态 JSON 格式)
    // 示例: {"role": "user", "content": [{"type": "text", "text": "..."}, {"type": "image", "path": "..."}]}
    DLL_EXPORT void LiteRtLm_AppendUserMessage(const char* json_msg);

    /**
     * @brief 向全局会话追加历史消息 (立即执行 Prefill 建立 KV Cache)
     * 
     * 此函数用于同步已有的对话历史。它会立即触发引擎的预填充(Prefill)逻辑以建立 KV Cache，
     * 但不会触发生成过程。参数为包含 role 的完整 JSON 消息。
     * 
     * Demo (恢复历史并提问):
     * @code
     * // 1. 恢复历史 (立即建立 KV Cache)
     * LiteRtLm_AppendHistoryMessage("{\"role\": \"system\", \"content\": \"你是一个助手\"}");
     * LiteRtLm_AppendHistoryMessage("{\"role\": \"user\", \"content\": \"你好\"}");
     * LiteRtLm_AppendHistoryMessage("{\"role\": \"assistant\", \"content\": \"你好！有什么可以帮你的？\"}");
     * 
     * // 2. 准备新问题 (覆盖式准备，不立即执行)
     * LiteRtLm_AppendUserMessage("{\"role\": \"user\", \"content\": \"今天天气怎么样？\"}");
     * 
     * // 3. 触发推理 (基于历史缓存秒回)
     * LiteRtLm_RunInference(params, callback, user_ptr);
     * @endcode
     */
    DLL_EXPORT void LiteRtLm_AppendHistoryMessage(const char* json_msg);

    // [核心] 触发全局会话增量推理
    DLL_EXPORT void LiteRtLm_RunInference(
        LiteRtLm_SamplingParams params,
        LiteRtLmCallback callback, 
        void* user_ptr
    );

    // 中断当前全局推理任务
    DLL_EXPORT void LiteRtLm_StopMessage();

    /**
     * @brief 阻塞等待引擎完成所有异步任务（推理生成）。
     * 
     * RunInference 是非阻塞的（仅提交任务到 GPU/CPU 队列），
     * 必须在 RunInference 之后调用此函数驱动回调分发。
     * 
     * @param engine_ptr  CreateEngine 返回的引擎指针
     * @param timeout_sec 超时秒数 (<=0 使用引擎默认超时 10 分钟)
     * @return 0=成功, 1=超时, -1=错误
     */
    DLL_EXPORT int LiteRtLm_WaitUntilDone(void* engine_ptr, int timeout_sec);

    // --- 2. GPU 物理 KV 缓存直接锁显存导出与恢复接口 ---

    /**
     * @brief 导出当前的全局物理 KV 缓存数据到 CPU 内存备份包
     * 
     * @param data_ptr 外部接收缓冲区的指针。为 nullptr 时，仅通过 out_size 返回物理大包所需的字节数。
     * @param out_size 传入时为外部缓冲区的最大承载大小，返回时为实际写入/所需的字节数。
     * @return 0=成功, 其他=失败错误码
     */
    DLL_EXPORT int LiteRtLm_GetKVCache(void* data_ptr, size_t* out_size);

    /**
     * @brief 从 CPU 内存恢复或擦写当前的全局物理 KV Cache 缓存
     * 
     * @param data_ptr CPU 中备份的 KV 缓存物理包二进制指针。当为 nullptr 且 size 为 0 时，等价于重置/清空当前的活跃物理 KV 缓存。
     * @param size 备份包的实际字节数大小。
     * @return 0=成功, 其他=失败错误码
     */
    DLL_EXPORT int LiteRtLm_SetKVCache(const void* data_ptr, size_t size);

#ifdef __cplusplus
}
#endif

#endif
