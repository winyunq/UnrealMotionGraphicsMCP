// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "LiteRtLmUnrealApi.h"
#include "LiteRtLmBlueprintLibrary.generated.h"

DECLARE_DYNAMIC_DELEGATE_OneParam(FLiteRtLmBlueprintChunkDelegate, const FString&, TextChunk);
DECLARE_DYNAMIC_DELEGATE_OneParam(FLiteRtLmBlueprintDoneDelegate, const FLiteRtLmResult&, Result);

/**
 * Blueprint-facing helpers for common LiteRT-LM workflows.
 *
 * The underlying C++ API remains FLiteRtLmUnrealApi. This library keeps
 * Blueprint graphs small for load, chat, multimodal, JSON, and session cleanup.
 */
UCLASS()
class LITERTLMUNREAL_API ULiteRtLmBlueprintLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintPure, Category = "LiteRT-LM|Config", meta = (DisplayName = "Get LiteRT-LM Auto Config", Keywords = "LiteRT LM auto config vram gpu cpu"))
    static FLiteRtLmConfig GetLiteRtLmAutoConfig();

    UFUNCTION(BlueprintPure, Category = "LiteRT-LM|Config", meta = (DisplayName = "Make LiteRT-LM Config", AdvancedDisplay = "Backend,MaxNumTokens,NumThreads,bEnableBenchmark,bOptimizeShader,bEnableVision,bEnableAudio,bEnableStreaming", Keywords = "LiteRT LM config model backend"))
    static FLiteRtLmConfig MakeLiteRtLmConfig(
        const FString& ModelPath,
        const FString& Backend = TEXT("gpu"),
        int32 MaxNumTokens = 2048,
        int32 NumThreads = 8,
        bool bEnableBenchmark = false,
        bool bOptimizeShader = true,
        bool bEnableVision = false,
        bool bEnableAudio = false,
        bool bEnableStreaming = true);

    UFUNCTION(BlueprintPure, Category = "LiteRT-LM|Config", meta = (DisplayName = "Make LiteRT-LM Sampling Params", AdvancedDisplay = "ConstraintType,ConstraintString", Keywords = "LiteRT LM sampling temperature top p k max tokens constrained"))
    static FLiteRtLmSamplingParams MakeLiteRtLmSamplingParams(
        float Temperature = 0.7f,
        float TopP = 0.9f,
        int32 TopK = 40,
        int32 MaxTokens = 512,
        ELiteRtLmConstraintType ConstraintType = ELiteRtLmConstraintType::None,
        const FString& ConstraintString = TEXT(""));

    UFUNCTION(BlueprintPure, Category = "LiteRT-LM|Model", meta = (DisplayName = "Resolve LiteRT-LM Model Path", Keywords = "LiteRT LM content models path"))
    static FString ResolveLiteRtLmProjectModelPath(const FString& ModelFileName);

    UFUNCTION(BlueprintPure, Category = "LiteRT-LM|Hardware", meta = (DisplayName = "Query LiteRT-LM Available VRAM", Keywords = "LiteRT LM vram gpu memory"))
    static int32 QueryLiteRtLmAvailableVramMB(int32 DefaultMB = 4096);

    UFUNCTION(BlueprintCallable, Category = "LiteRT-LM|Lifecycle", meta = (DisplayName = "Load LiteRT-LM Model", Keywords = "LiteRT LM load model config"))
    static bool LoadLiteRtLmModel(const FLiteRtLmConfig& Config);

    UFUNCTION(BlueprintCallable, Category = "LiteRT-LM|Lifecycle", meta = (DisplayName = "Load LiteRT-LM Model", AdvancedDisplay = "Backend,MaxNumTokens,NumThreads,bEnableBenchmark,bOptimizeShader,bEnableVision,bEnableAudio,bEnableStreaming", Keywords = "LiteRT LM load model path auto config"))
    static bool LoadLiteRtLmModelFromPath(
        const FString& ModelPath,
        bool bUseAutoConfig = true,
        const FString& Backend = TEXT("gpu"),
        int32 MaxNumTokens = 2048,
        int32 NumThreads = 8,
        bool bEnableBenchmark = false,
        bool bOptimizeShader = true,
        bool bEnableVision = false,
        bool bEnableAudio = false,
        bool bEnableStreaming = true);

    UFUNCTION(BlueprintCallable, Category = "LiteRT-LM|Lifecycle", meta = (DisplayName = "Load LiteRT-LM Model", AdvancedDisplay = "Backend,MaxNumTokens,NumThreads,bEnableBenchmark,bOptimizeShader,bEnableVision,bEnableAudio,bEnableStreaming", Keywords = "LiteRT LM load project content models"))
    static bool LoadLiteRtLmProjectModel(
        const FString& ModelFileName,
        bool bUseAutoConfig = true,
        const FString& Backend = TEXT("gpu"),
        int32 MaxNumTokens = 2048,
        int32 NumThreads = 8,
        bool bEnableBenchmark = false,
        bool bOptimizeShader = true,
        bool bEnableVision = false,
        bool bEnableAudio = false,
        bool bEnableStreaming = true);

    UFUNCTION(BlueprintCallable, Category = "LiteRT-LM|Lifecycle", meta = (DisplayName = "Unload LiteRT-LM Model", Keywords = "LiteRT LM unload release model gpu"))
    static void UnloadLiteRtLmModel();

    UFUNCTION(BlueprintPure, Category = "LiteRT-LM|Lifecycle", meta = (DisplayName = "Is LiteRT-LM Model Loaded", Keywords = "LiteRT LM loaded ready"))
    static bool IsLiteRtLmModelLoaded();

    UFUNCTION(BlueprintCallable, Category = "LiteRT-LM|Inference", meta = (WorldContext = "WorldContextObject", DisplayName = "Send LiteRT-LM Chat Request", AdvancedDisplay = "SessionOwner,SamplingParams", Keywords = "LiteRT LM chat text simple streaming"))
    static void SendLiteRtLmTextChat(
        UObject* WorldContextObject,
        const FString& UserMessage,
        UObject* SessionOwner,
        FLiteRtLmBlueprintChunkDelegate OnChunk,
        FLiteRtLmBlueprintDoneDelegate OnDone,
        FLiteRtLmSamplingParams SamplingParams);

    UFUNCTION(BlueprintCallable, Category = "LiteRT-LM|Inference", meta = (WorldContext = "WorldContextObject", DisplayName = "Send LiteRT-LM Chat Request", AdvancedDisplay = "SystemPrompt,SessionOwner,SamplingParams", Keywords = "LiteRT LM chat text system prompt streaming"))
    static void SendLiteRtLmTextChatWithSystemPrompt(
        UObject* WorldContextObject,
        const FString& UserMessage,
        const FString& SystemPrompt,
        UObject* SessionOwner,
        FLiteRtLmBlueprintChunkDelegate OnChunk,
        FLiteRtLmBlueprintDoneDelegate OnDone,
        FLiteRtLmSamplingParams SamplingParams);

    UFUNCTION(BlueprintCallable, Category = "LiteRT-LM|Inference", meta = (WorldContext = "WorldContextObject", DisplayName = "Send LiteRT-LM Chat Request", AdvancedDisplay = "ImagePath,AudioPath,SystemPrompt,SessionOwner,SamplingParams", Keywords = "LiteRT LM chat multimodal image audio streaming"))
    static void SendLiteRtLmMultimodalChat(
        UObject* WorldContextObject,
        const FString& UserMessage,
        const FString& ImagePath,
        const FString& AudioPath,
        const FString& SystemPrompt,
        UObject* SessionOwner,
        FLiteRtLmBlueprintChunkDelegate OnChunk,
        FLiteRtLmBlueprintDoneDelegate OnDone,
        FLiteRtLmSamplingParams SamplingParams);

    UFUNCTION(BlueprintCallable, Category = "LiteRT-LM|Inference", meta = (WorldContext = "WorldContextObject", DisplayName = "Send LiteRT-LM Chat Request", AdvancedDisplay = "ToolsJson,SessionOwner,SamplingParams", Keywords = "LiteRT LM chat json messages history tools mcp openai"))
    static void SendLiteRtLmJsonChat(
        UObject* WorldContextObject,
        const FString& MessagesJson,
        const FString& ToolsJson,
        UObject* SessionOwner,
        FLiteRtLmBlueprintChunkDelegate OnChunk,
        FLiteRtLmBlueprintDoneDelegate OnDone,
        FLiteRtLmSamplingParams SamplingParams);

    UFUNCTION(BlueprintCallable, Category = "LiteRT-LM|Inference", meta = (WorldContext = "WorldContextObject", DisplayName = "Release LiteRT-LM Session", Keywords = "LiteRT LM session reset cache conversation"))
    static void ReleaseLiteRtLmSession(UObject* WorldContextObject, UObject* SessionOwner);
};
