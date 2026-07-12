// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "LiteRtLmUnrealApi.h"
#include "LiteRtLmComponent.generated.h"

// Dynamic multicast delegates for Blueprint support
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FLiteRtLmComponentChunkSignature, const FString&, TextChunk);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FLiteRtLmComponentDoneSignature, const FLiteRtLmResult&, Result);

/**
 * High-level component to add AI "Brain" capabilities to any Actor.
 * Automatically manages sessions using the Owner Actor as context.
 */
UCLASS(ClassGroup=(LiteRT), meta=(BlueprintSpawnableComponent))
class LITERTLMUNREAL_API ULiteRtLmComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    ULiteRtLmComponent();

    /** The personality or instructions for this NPC. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LiteRT|Config")
    FString SystemPrompt = TEXT("You are a helpful NPC.");

    /** Sampling parameters for this specific NPC. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LiteRT|Config")
    FLiteRtLmSamplingParams SamplingParams;

    /** Called when a new text chunk is received (Streaming). */
    UPROPERTY(BlueprintAssignable, Category = "LiteRT|Events")
    FLiteRtLmComponentChunkSignature OnTextChunkReceived;

    /** Called when the full response is ready. */
    UPROPERTY(BlueprintAssignable, Category = "LiteRT|Events")
    FLiteRtLmComponentDoneSignature OnInferenceCompleted;

    /** 
     * Send a message to this NPC. 
     * Uses the Owner Actor as the session context for automatic KV Cache management.
     */
    UFUNCTION(BlueprintCallable, Category = "LiteRT|Inference")
    void SendChatMessage(const FString& UserMessage);

    /** Clear this NPC's conversation history. */
    UFUNCTION(BlueprintCallable, Category = "LiteRT|Inference")
    void ResetConversation();

    /** Get the last inference performance data. */
    UFUNCTION(BlueprintPure, Category = "LiteRT|Stats")
    FLiteRtLmResult GetLastResult() const { return LastResult; }

protected:
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    FLiteRtLmResult LastResult;
    bool bIsProcessing = false;

    // Internal glue
    void HandleInternalChunk(const FString& Chunk);
    void HandleInternalDone(const FLiteRtLmResult& Result);
};
