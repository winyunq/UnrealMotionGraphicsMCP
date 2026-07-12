// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#include "LiteRtLmComponent.h"
#include "LiteRtLmSubsystem.h"
#include "Engine/Engine.h"

ULiteRtLmComponent::ULiteRtLmComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    
    // Default sampling params
    SamplingParams.Temperature = 0.7f;
    SamplingParams.MaxTokens = 512;
}

void ULiteRtLmComponent::SendChatMessage(const FString& UserMessage)
{
    if (bIsProcessing) return;
    bIsProcessing = true;

    // Use 'this' or Owner() as context. Using 'this' is safer as it's unique per component.
    TSharedPtr<FJsonObject> UserMsg = MakeShared<FJsonObject>();
    UserMsg->SetStringField(TEXT("role"), TEXT("user"));
    UserMsg->SetStringField(TEXT("content"), UserMessage);

    FLiteRtLmUnrealApi::SendChatRequest(
        this,
        UserMsg,
        FLiteRtLmChunkCallback::CreateUObject(this, &ULiteRtLmComponent::HandleInternalChunk),
        FLiteRtLmDoneCallback::CreateUObject(this, &ULiteRtLmComponent::HandleInternalDone),
        SamplingParams
    );
}

void ULiteRtLmComponent::ResetConversation()
{
    if (ULiteRtLmSubsystem* Subsystem = GEngine->GetEngineSubsystem<ULiteRtLmSubsystem>())
    {
        Subsystem->ReleaseSession(this);
    }
}

void ULiteRtLmComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    ResetConversation();
    Super::EndPlay(EndPlayReason);
}

void ULiteRtLmComponent::HandleInternalChunk(const FString& Chunk)
{
    OnTextChunkReceived.Broadcast(Chunk);
}

void ULiteRtLmComponent::HandleInternalDone(const FLiteRtLmResult& Result)
{
    LastResult = Result;
    bIsProcessing = false;
    OnInferenceCompleted.Broadcast(Result);
}
