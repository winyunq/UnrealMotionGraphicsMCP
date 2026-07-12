// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/EngineSubsystem.h"
#include "LiteRtLmUnrealApi.h"
#include "LiteRtLmSubsystem.generated.h"

/**
 * Global Subsystem to manage LiteRT-LM resources.
 */
UCLASS()
class LITERTLMUNREAL_API ULiteRtLmSubsystem : public UEngineSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    bool LoadModel(const FLiteRtLmConfig& InConfig);
    void UnloadModel();

    // Session Management (Agent-level KV Cache Context Switch)
    
    /**
     * @brief       准备并激活指定 Agent 的上下文
     * 
     * @param       参数名称: AgentKey                      数据类型:        void*
     * @param       参数名称: ToolsJson                     数据类型:        const FString&
     * 
     * @return      是否准备成功                            数据类型:        bool
     **/
    bool PrepareActiveAgent(void* AgentKey, const FString& ToolsJson);

    /**
     * @brief       释放指定 Agent 的内存 KV 缓存
     * 
     * @param       参数名称: AgentKey                      数据类型:        void*
     **/
    void ReleaseAgentCache(void* AgentKey);

    /**
     * @brief       释放指定 Agent 的会话上下文及内存 KV 缓存 (兼容旧接口)
     * 
     * @param       参数名称: Ctx                           数据类型:        void*
     **/
    void ReleaseSession(void* Ctx);

    // Per-session message tracking (for incremental message sync)
    int32 GetSessionMsgCount(void* Ctx) const;
    void SetSessionMsgCount(void* Ctx, int32 Count);

    // Getters
    void* GetEngineHandle() const { return EngineHandle; }
    bool IsModelLoaded() const { return EngineHandle != nullptr; }
    bool IsVisionEnabled() const { return CurrentConfig.bEnableVision; }
    const FLiteRtLmConfig& GetCurrentConfig() const { return CurrentConfig; }

    /**
     * Query available GPU VRAM via DXGI (Windows only).
     * Returns available VRAM in MB, or DefaultMB if query fails.
     */
    static int32 QueryAvailableVramMB(int32 DefaultMB = 4096);

private:
    void* EngineHandle = nullptr;
    void* CurrentActiveAgentKey = nullptr;

    struct FLiteRtLmAgentCache
    {
        TArray<uint8> KVCacheData;
        int32 MsgCount = 0;
        FString ToolsJson;
    };

    TMap<void*, FLiteRtLmAgentCache> AgentCacheMap;

    UPROPERTY()
    FLiteRtLmConfig CurrentConfig;

    // LRU logic can be added here
};
