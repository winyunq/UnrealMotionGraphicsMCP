// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#include "LiteRtLmSubsystem.h"
#include "Engine/Engine.h"
#include "Internal/LiteRtLmWrapperLoader.h"
#include "Misc/CoreDelegates.h"
#include <string>
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <dxgi1_4.h>
#include "Windows/HideWindowsPlatformTypes.h"
#pragma comment(lib, "dxgi.lib")
#endif

void ULiteRtLmSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
}

void ULiteRtLmSubsystem::Deinitialize()
{
    UnloadModel();
    Super::Deinitialize();
}

bool ULiteRtLmSubsystem::LoadModel(const FLiteRtLmConfig& InConfig)
{
    if (IsModelLoaded()) UnloadModel();

    if (!FLiteRtLmWrapperLoader::LoadDll())
    {
        UE_LOG(LogLiteRtLm, Error, TEXT("Failed to load shared library."));
        return false;
    }

    if (!FLiteRtLmWrapperLoader::CreateEngine)
    {
        UE_LOG(LogLiteRtLm, Error, TEXT("CreateEngine function pointer is null. DLL not loaded?"));
        return false;
    }

    CurrentConfig = InConfig;

    // 物理注册注入底座大脑的 MCP 具体 JSON 字符串信息或提示为空
    if (CurrentConfig.ToolsJson.IsEmpty())
    {
        UE_LOG(LogLiteRtLm, Log, TEXT("[LiteRtLmSubsystem] 本地底座模型初始化装载！未指定任何活跃工具 (注册 MCP 工具列表为空)。"));
    }
    else
    {
        TArray<TSharedPtr<FJsonValue>> JsonArray;
        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(CurrentConfig.ToolsJson);
        if (FJsonSerializer::Deserialize(Reader, JsonArray) && JsonArray.Num() > 0)
        {
            FString ToolsLogString;
            TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> PrettyWriter = TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&ToolsLogString);
            FJsonSerializer::Serialize(JsonArray, PrettyWriter);

            TArray<FString> LogLines;
            ToolsLogString.ParseIntoArrayLines(LogLines, false);

            UE_LOG(LogLiteRtLm, Log, TEXT("==================== [LiteRtLmProvider] 注册 MCP 具体 JSON 字符串信息开始 ===================="));
            for (const FString& Line : LogLines)
            {
                UE_LOG(LogLiteRtLm, Log, TEXT("%s"), *Line);
            }
            UE_LOG(LogLiteRtLm, Log, TEXT("==================== [LiteRtLmProvider] 注册 MCP 具体 JSON 字符串信息结束 ===================="));
        }
        else
        {
            UE_LOG(LogLiteRtLm, Warning, TEXT("[LiteRtLmSubsystem] 物理注册 MCP 工具失败：ToolsJson 格式不合规。 Raw: %s"), *CurrentConfig.ToolsJson);
        }
    }

    // 固化内存生命周期至全局静态空间，规避 TStringConversion 无法默认构造与赋值的问题
    // 确保底层 DLL 异步多线程在整个 Engine 生命周期内 safe 读取字符指针，彻底防止野指针崩溃
    static std::string StaticModelPath;
    static std::string StaticBackend;
    static std::string StaticToolsJson;
    
    StaticModelPath = TCHAR_TO_UTF8(*CurrentConfig.ModelPath);
    StaticBackend   = TCHAR_TO_UTF8(*CurrentConfig.Backend);
    StaticToolsJson = TCHAR_TO_UTF8(*CurrentConfig.ToolsJson);

    // 零初始化 C 风格结构体，防范内存垃圾，一次性绑定开关配置
    LiteRtLm_Config CConfig = {};
    CConfig.model_path      = StaticModelPath.c_str();
    CConfig.backend         = StaticBackend.c_str();
    CConfig.max_num_tokens  = CurrentConfig.MaxNumTokens;
    CConfig.num_threads     = CurrentConfig.NumThreads;
    CConfig.bEnableBenchmark = CurrentConfig.bEnableBenchmark ? 1 : 0;
    CConfig.bOptimizeShader  = CurrentConfig.bOptimizeShader ? 1 : 0;
    CConfig.bEnableVision   = CurrentConfig.bEnableVision ? 1 : 0;
    CConfig.bEnableAudio    = CurrentConfig.bEnableAudio ? 1 : 0;
    CConfig.prefill_chunk_size = CurrentConfig.PrefillChunkSize;
    CConfig.tools_json      = StaticToolsJson.empty() ? nullptr : StaticToolsJson.c_str();
    CConfig.bShareConstantTensors = CurrentConfig.bShareConstantTensors ? 1 : 0;
    CConfig.bEnableHostMappedPointer = CurrentConfig.bEnableHostMappedPointer ? 1 : 0;

    UE_LOG(LogLiteRtLm, Log, TEXT("Loading model: Path=%s, Backend=%s, MaxTokens=%d, ChunkSize=%d, Threads=%d, Vision=%d, Audio=%d"),
        *CurrentConfig.ModelPath, *CurrentConfig.Backend,
        CurrentConfig.MaxNumTokens, CurrentConfig.PrefillChunkSize, CurrentConfig.NumThreads,
        CurrentConfig.bEnableVision, CurrentConfig.bEnableAudio);

    EngineHandle = FLiteRtLmWrapperLoader::CreateEngine(CConfig);

    // [注释] 原有的"Vision 失败后降级纯文本重试"逻辑已被禁用。
    // 原因：CreateEngine 在 GPU backend + 纯文本模式下会卡死，不返回。
    // LiteRT-LM 插件层不应承担重试职责；失败应直接上报，
    // 由上层 UmgMcpAiSubsystem 的 Candidate Cascade 容灾机制处理。
    // 若未来底层 DLL 确认纯文本 fallback 不会卡死，可在此恢复。
    //
    // if (!EngineHandle && (CConfig.bEnableVision || CConfig.bEnableAudio))
    // {
    //     UE_LOG(LogLiteRtLm, Warning, TEXT("Failed to load model with multimodal options. Retrying with pure text mode fallback..."));
    //     CConfig.bEnableVision = 0;
    //     CConfig.bEnableAudio  = 0;
    //     EngineHandle = FLiteRtLmWrapperLoader::CreateEngine(CConfig);
    //     if (EngineHandle)
    //     {
    //         CurrentConfig.bEnableVision = false;
    //         CurrentConfig.bEnableAudio  = false;
    //     }
    // }

    if (!EngineHandle)
    {
        UE_LOG(LogLiteRtLm, Error, TEXT("CreateEngine returned nullptr. Model path may be invalid or GPU backend unavailable."));
        return false;
    }

    UE_LOG(LogLiteRtLm, Log, TEXT("Model loaded successfully. EngineHandle=%p, Vision=%d, Audio=%d"), 
        EngineHandle, CurrentConfig.bEnableVision ? 1 : 0, CurrentConfig.bEnableAudio ? 1 : 0);
    return true;
}

void ULiteRtLmSubsystem::UnloadModel()
{
    if (!EngineHandle) return;

    CurrentActiveAgentKey = nullptr;

    if (FLiteRtLmWrapperLoader::DestroyEngine)
    {
        FLiteRtLmWrapperLoader::DestroyEngine(EngineHandle);
    }
    EngineHandle = nullptr;

    UE_LOG(LogLiteRtLm, Log, TEXT("[LiteRtLmSubsystem] 本地大模型引擎实例已销毁 (已完成 DestroyEngine)"));
    
    // 立即执行高保真 DXGI 显存硬件遥测，并在日志中输出释放后的确切显存占用，提供无可辩驳的物理数据支持
    QueryAvailableVramMB(0);
}

bool ULiteRtLmSubsystem::PrepareActiveAgent(void* AgentKey, const FString& ToolsJson)
{
    if (!IsModelLoaded()) return false;

    /// 1. 检查 Agent 是否需要因为 ToolsJson 变化而重置其物理缓存并重新载入模型
    FLiteRtLmAgentCache& TargetCache = AgentCacheMap.FindOrAdd(AgentKey);
    if (TargetCache.ToolsJson != ToolsJson)
    {
        UE_LOG(LogLiteRtLm, Warning, TEXT("[KV Cache] ToolsJson changed for Agent %p. Resetting cache and reloading model for MCP tools binding."), AgentKey);
        TargetCache.KVCacheData.Empty();
        TargetCache.MsgCount = 0;
        TargetCache.ToolsJson = ToolsJson;
        
        /// 备份当前活跃 Agent 的显存大包数据，防止重载模型时丢失
        /*
        if (CurrentActiveAgentKey != nullptr && FLiteRtLmWrapperLoader::GetKVCache)
        {
            size_t CacheSize = 0;
            int32 R1 = FLiteRtLmWrapperLoader::GetKVCache(nullptr, &CacheSize);
            if (R1 == 0 && CacheSize > 0)
            {
                FLiteRtLmAgentCache& ActiveCache = AgentCacheMap.FindOrAdd(CurrentActiveAgentKey);
                ActiveCache.KVCacheData.SetNumUninitialized(CacheSize);
                int32 R2 = FLiteRtLmWrapperLoader::GetKVCache(ActiveCache.KVCacheData.GetData(), &CacheSize);
                if (R2 == 0)
                {
                    UE_LOG(LogLiteRtLm, Log, TEXT("[KV Cache] Successfully backed up %d bytes for Agent %p before model reload"), CacheSize, CurrentActiveAgentKey);
                }
            }
        }
        */

        /// 如果刚好是当前的活跃 Agent，也必须在显存中干净重置它
        /*
        if (CurrentActiveAgentKey == AgentKey)
        {
            if (FLiteRtLmWrapperLoader::SetKVCache)
            {
                FLiteRtLmWrapperLoader::SetKVCache(nullptr, 0);
            }
        }
        */

        // 重新载入模型以应用更新后的 tools_json 注册工具到 C 引擎实例中
        FLiteRtLmConfig ModelConfigCopy = CurrentConfig;
        ModelConfigCopy.ToolsJson = ToolsJson;
        LoadModel(ModelConfigCopy);
    }

    /// 2. 如果检测到切换 Agent 对话，才进行 KV 缓存大包的导出与物理还原
    if (CurrentActiveAgentKey != AgentKey)
    {
        /// 备份当前活跃 Agent 的显存大包数据
        /*
        if (CurrentActiveAgentKey != nullptr && FLiteRtLmWrapperLoader::GetKVCache)
        {
            size_t CacheSize = 0;
            int32 R1 = FLiteRtLmWrapperLoader::GetKVCache(nullptr, &CacheSize);
            if (R1 == 0 && CacheSize > 0)
            {
                FLiteRtLmAgentCache& ActiveCache = AgentCacheMap.FindOrAdd(CurrentActiveAgentKey);
                ActiveCache.KVCacheData.SetNumUninitialized(CacheSize);
                int32 R2 = FLiteRtLmWrapperLoader::GetKVCache(ActiveCache.KVCacheData.GetData(), &CacheSize);
                if (R2 == 0)
                {
                    UE_LOG(LogLiteRtLm, Log, TEXT("[KV Cache] Successfully backed up %d bytes for Agent %p on switch"), CacheSize, CurrentActiveAgentKey);
                }
                else
                {
                    UE_LOG(LogLiteRtLm, Error, TEXT("[KV Cache] Failed to backup KV cache for Agent %p on switch, Code=%d"), CurrentActiveAgentKey, R2);
                }
            }
        }
        */

        /// 物理还原或擦写目标 Agent 的 GPU 显存
        /*
        if (FLiteRtLmWrapperLoader::SetKVCache)
        {
            if (TargetCache.KVCacheData.Num() > 0)
            {
                int32 R3 = FLiteRtLmWrapperLoader::SetKVCache(TargetCache.KVCacheData.GetData(), TargetCache.KVCacheData.Num());
                if (R3 == 0)
                {
                    UE_LOG(LogLiteRtLm, Log, TEXT("[KV Cache] Successfully restored %d bytes for Agent %p"), TargetCache.KVCacheData.Num(), AgentKey);
                }
                else
                {
                    UE_LOG(LogLiteRtLm, Error, TEXT("[KV Cache] Failed to restore KV cache for Agent %p, Code=%d"), AgentKey, R3);
                }
            }
            else
            {
                /// 目标 Agent 没有历史缓存备份，执行显存大包零擦除
                int32 R4 = FLiteRtLmWrapperLoader::SetKVCache(nullptr, 0);
                UE_LOG(LogLiteRtLm, Log, TEXT("[KV Cache] Cleared active KV cache for new Agent %p, Code=%d"), AgentKey, R4);
            }
        }
        */

        /// 切换当前驻留在 GPU 中的活跃 Agent 指针标识
        CurrentActiveAgentKey = AgentKey;
    }

    return true;
}

void ULiteRtLmSubsystem::ReleaseAgentCache(void* AgentKey)
{
    if (CurrentActiveAgentKey == AgentKey)
    {
        /// 释放当前的活跃显存缓存
        /*
        if (FLiteRtLmWrapperLoader::SetKVCache)
        {
            FLiteRtLmWrapperLoader::SetKVCache(nullptr, 0);
        }
        */
        CurrentActiveAgentKey = nullptr;
    }

    AgentCacheMap.Remove(AgentKey);
}

void ULiteRtLmSubsystem::ReleaseSession(void* Ctx)
{
    ReleaseAgentCache(Ctx);
}

int32 ULiteRtLmSubsystem::GetSessionMsgCount(void* Ctx) const
{
    const FLiteRtLmAgentCache* Found = AgentCacheMap.Find(Ctx);
    return Found ? Found->MsgCount : 0;
}

void ULiteRtLmSubsystem::SetSessionMsgCount(void* Ctx, int32 Count)
{
    FLiteRtLmAgentCache& Cache = AgentCacheMap.FindOrAdd(Ctx);
    Cache.MsgCount = Count;
}

int32 ULiteRtLmSubsystem::QueryAvailableVramMB(int32 DefaultMB)
{
#if PLATFORM_WINDOWS
    IDXGIFactory4* DxgiFactory = nullptr;
    HRESULT Hr = CreateDXGIFactory1(__uuidof(IDXGIFactory4), (void**)&DxgiFactory);
    if (FAILED(Hr) || !DxgiFactory)
    {
        UE_LOG(LogLiteRtLm, Warning, TEXT("DXGI query failed, using default %d MB"), DefaultMB);
        return DefaultMB;
    }

    IDXGIAdapter3* Adapter = nullptr;
    Hr = DxgiFactory->EnumAdapters(0, reinterpret_cast<IDXGIAdapter**>(&Adapter));
    if (FAILED(Hr) || !Adapter)
    {
        DxgiFactory->Release();
        return DefaultMB;
    }

    // Re-query as IDXGIAdapter3 for QueryVideoMemoryInfo
    IDXGIAdapter3* Adapter3 = nullptr;
    Hr = Adapter->QueryInterface(__uuidof(IDXGIAdapter3), (void**)&Adapter3);
    Adapter->Release();
    if (FAILED(Hr) || !Adapter3)
    {
        DxgiFactory->Release();
        return DefaultMB;
    }

    DXGI_QUERY_VIDEO_MEMORY_INFO MemInfo = {};
    Hr = Adapter3->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &MemInfo);
    Adapter3->Release();
    DxgiFactory->Release();

    if (FAILED(Hr))
    {
        return DefaultMB;
    }

    const int64 AvailableBytes = static_cast<int64>(MemInfo.Budget) - static_cast<int64>(MemInfo.CurrentUsage);
    const int32 AvailableMB = static_cast<int32>(FMath::Max(AvailableBytes, (int64)0) / (1024 * 1024));

    UE_LOG(LogLiteRtLm, Log, TEXT("DXGI VRAM: Budget=%llu MB, Used=%llu MB, Available=%d MB"),
        MemInfo.Budget / (1024 * 1024), MemInfo.CurrentUsage / (1024 * 1024), AvailableMB);

    return AvailableMB;
#else
    return DefaultMB;
#endif
}
