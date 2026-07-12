// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#include "LiteRtLmBlueprintLibrary.h"

#include "LiteRtLmSubsystem.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/Engine.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace LiteRtLmBlueprintLibrary
{
    static FLiteRtLmConfig BuildConfig(
        const FString& ModelPath,
        bool bUseAutoConfig,
        const FString& Backend,
        int32 MaxNumTokens,
        int32 NumThreads,
        bool bEnableBenchmark,
        bool bOptimizeShader,
        bool bEnableVision,
        bool bEnableAudio,
        bool bEnableStreaming)
    {
        FLiteRtLmConfig Config = bUseAutoConfig ? FLiteRtLmUnrealApi::GetAutoConfig() : FLiteRtLmConfig();

        Config.ModelPath = ModelPath;
        if (!bUseAutoConfig)
        {
            Config.Backend = Backend;
            Config.MaxNumTokens = MaxNumTokens;
            Config.NumThreads = NumThreads;
        }

        Config.bEnableBenchmark = bEnableBenchmark;
        Config.bOptimizeShader = bOptimizeShader;
        Config.bEnableVision = bEnableVision;
        Config.bEnableAudio = bEnableAudio;
        Config.bEnableStreaming = bEnableStreaming;
        return Config;
    }

    static UObject* ResolveSessionOwner(UObject* WorldContextObject, UObject* SessionOwner)
    {
        if (IsValid(SessionOwner))
        {
            return SessionOwner;
        }

        if (IsValid(WorldContextObject))
        {
            return WorldContextObject;
        }

        return GetTransientPackage();
    }

    static void ExecuteError(const FLiteRtLmBlueprintDoneDelegate& OnDone, const FString& ErrorMessage)
    {
        if (!OnDone.IsBound())
        {
            return;
        }

        FLiteRtLmResult Result;
        Result.ErrorMsg = ErrorMessage;
        Result.bIsDone = true;
        OnDone.Execute(Result);
    }

    static TSharedPtr<FJsonObject> MakeTextMessage(const FString& Role, const FString& Content)
    {
        TSharedPtr<FJsonObject> Message = MakeShared<FJsonObject>();
        Message->SetStringField(TEXT("role"), Role);
        Message->SetStringField(TEXT("content"), Content);
        return Message;
    }

    static TSharedPtr<FJsonObject> MakeUserMessage(
        const FString& UserMessage,
        const FString& ImagePath,
        const FString& AudioPath)
    {
        TSharedPtr<FJsonObject> Message = MakeShared<FJsonObject>();
        Message->SetStringField(TEXT("role"), TEXT("user"));

        const bool bHasImage = !ImagePath.IsEmpty();
        const bool bHasAudio = !AudioPath.IsEmpty();
        if (!bHasImage && !bHasAudio)
        {
            Message->SetStringField(TEXT("content"), UserMessage);
            return Message;
        }

        TArray<TSharedPtr<FJsonValue>> ContentArray;

        if (bHasImage)
        {
            FString StandardPath = ImagePath;
            FPaths::NormalizeFilename(StandardPath);

            TSharedPtr<FJsonObject> ImageObject = MakeShared<FJsonObject>();
            ImageObject->SetStringField(TEXT("type"), TEXT("image"));
            ImageObject->SetStringField(TEXT("path"), StandardPath);
            ContentArray.Add(MakeShared<FJsonValueObject>(ImageObject));
        }

        if (bHasAudio)
        {
            FString StandardPath = AudioPath;
            FPaths::NormalizeFilename(StandardPath);

            TSharedPtr<FJsonObject> AudioObject = MakeShared<FJsonObject>();
            AudioObject->SetStringField(TEXT("type"), TEXT("audio"));
            AudioObject->SetStringField(TEXT("path"), StandardPath);
            ContentArray.Add(MakeShared<FJsonValueObject>(AudioObject));
        }

        FString TextToSend = UserMessage;
        if (bHasImage && !TextToSend.Contains(TEXT("<IMAGE>")))
        {
            TextToSend = TEXT("<IMAGE>\n") + TextToSend;
        }
        if (bHasAudio && !TextToSend.Contains(TEXT("<AUDIO>")))
        {
            TextToSend = TEXT("<AUDIO>\n") + TextToSend;
        }

        TSharedPtr<FJsonObject> TextObject = MakeShared<FJsonObject>();
        TextObject->SetStringField(TEXT("type"), TEXT("text"));
        TextObject->SetStringField(TEXT("text"), TextToSend);
        ContentArray.Add(MakeShared<FJsonValueObject>(TextObject));

        Message->SetArrayField(TEXT("content"), ContentArray);
        return Message;
    }

    static void SendMessages(
        UObject* WorldContextObject,
        UObject* SessionOwner,
        const TArray<TSharedPtr<FJsonObject>>& Messages,
        const FString& ToolsJson,
        FLiteRtLmBlueprintChunkDelegate OnChunk,
        FLiteRtLmBlueprintDoneDelegate OnDone,
        const FLiteRtLmSamplingParams& SamplingParams)
    {
        UObject* ResolvedSessionOwner = ResolveSessionOwner(WorldContextObject, SessionOwner);
        if (!ResolvedSessionOwner)
        {
            ExecuteError(OnDone, TEXT("LiteRT-LM session owner is not available."));
            return;
        }

        if (Messages.Num() == 0)
        {
            ExecuteError(OnDone, TEXT("LiteRT-LM request has no messages."));
            return;
        }

        FLiteRtLmChunkCallback NativeChunk;
        if (OnChunk.IsBound())
        {
            NativeChunk = FLiteRtLmChunkCallback::CreateLambda(
                [OnChunk](const FString& TextChunk) mutable
                {
                    OnChunk.ExecuteIfBound(TextChunk);
                });
        }

        FLiteRtLmDoneCallback NativeDone;
        if (OnDone.IsBound())
        {
            NativeDone = FLiteRtLmDoneCallback::CreateLambda(
                [OnDone](const FLiteRtLmResult& Result) mutable
                {
                    OnDone.ExecuteIfBound(Result);
                });
        }

        TArray<TSharedPtr<FJsonObject>> NormalizedMessages = FLiteRtLmUnrealApi::NormalizeMessages(Messages);
        if (NormalizedMessages.Num() == 0)
        {
            ExecuteError(OnDone, TEXT("LiteRT-LM request has no normalized messages."));
            return;
        }

        if (ULiteRtLmSubsystem* Subsystem = GEngine ? GEngine->GetEngineSubsystem<ULiteRtLmSubsystem>() : nullptr)
        {
            Subsystem->PrepareActiveAgent(ResolvedSessionOwner, ToolsJson);
        }

        const int32 LastMessageIndex = NormalizedMessages.Num() - 1;
        if (LastMessageIndex > 0)
        {
            TArray<TSharedPtr<FJsonObject>> HistoryMessages;
            for (int32 Index = 0; Index < LastMessageIndex; ++Index)
            {
                HistoryMessages.Add(NormalizedMessages[Index]);
            }
            FLiteRtLmUnrealApi::RestoreHistory(HistoryMessages);
        }

        FLiteRtLmUnrealApi::SendChatRequest(
            ResolvedSessionOwner,
            NormalizedMessages[LastMessageIndex],
            NativeChunk,
            NativeDone,
            SamplingParams);
    }

    static bool ParseMessagesJson(
        const FString& MessagesJson,
        TArray<TSharedPtr<FJsonObject>>& OutMessages,
        FString& OutError)
    {
        const FString Trimmed = MessagesJson.TrimStartAndEnd();
        if (Trimmed.IsEmpty())
        {
            OutError = TEXT("MessagesJson is empty.");
            return false;
        }

        if (Trimmed.StartsWith(TEXT("[")))
        {
            TArray<TSharedPtr<FJsonValue>> Values;
            TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Trimmed);
            if (!FJsonSerializer::Deserialize(Reader, Values))
            {
                OutError = TEXT("MessagesJson array could not be parsed.");
                return false;
            }

            for (const TSharedPtr<FJsonValue>& Value : Values)
            {
                TSharedPtr<FJsonObject> Object = Value.IsValid() ? Value->AsObject() : nullptr;
                if (!Object.IsValid())
                {
                    OutError = TEXT("MessagesJson array must contain only JSON objects.");
                    return false;
                }
                OutMessages.Add(Object);
            }
        }
        else
        {
            TSharedPtr<FJsonObject> Object;
            TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Trimmed);
            if (!FJsonSerializer::Deserialize(Reader, Object) || !Object.IsValid())
            {
                OutError = TEXT("MessagesJson object could not be parsed.");
                return false;
            }
            OutMessages.Add(Object);
        }

        if (OutMessages.Num() == 0)
        {
            OutError = TEXT("MessagesJson did not contain any messages.");
            return false;
        }

        return true;
    }
}

FLiteRtLmConfig ULiteRtLmBlueprintLibrary::GetLiteRtLmAutoConfig()
{
    return FLiteRtLmUnrealApi::GetAutoConfig();
}

FLiteRtLmConfig ULiteRtLmBlueprintLibrary::MakeLiteRtLmConfig(
    const FString& ModelPath,
    const FString& Backend,
    int32 MaxNumTokens,
    int32 NumThreads,
    bool bEnableBenchmark,
    bool bOptimizeShader,
    bool bEnableVision,
    bool bEnableAudio,
    bool bEnableStreaming)
{
    return LiteRtLmBlueprintLibrary::BuildConfig(
        ModelPath,
        false,
        Backend,
        MaxNumTokens,
        NumThreads,
        bEnableBenchmark,
        bOptimizeShader,
        bEnableVision,
        bEnableAudio,
        bEnableStreaming);
}

FLiteRtLmSamplingParams ULiteRtLmBlueprintLibrary::MakeLiteRtLmSamplingParams(
    float Temperature,
    float TopP,
    int32 TopK,
    int32 MaxTokens,
    ELiteRtLmConstraintType ConstraintType,
    const FString& ConstraintString)
{
    FLiteRtLmSamplingParams Params;
    Params.Temperature = Temperature;
    Params.TopP = TopP;
    Params.TopK = TopK;
    Params.MaxTokens = MaxTokens;
    Params.ConstraintType = ConstraintType;
    Params.ConstraintString = ConstraintString;
    return Params;
}

FString ULiteRtLmBlueprintLibrary::ResolveLiteRtLmProjectModelPath(const FString& ModelFileName)
{
    FString Normalized = ModelFileName;
    FPaths::NormalizeFilename(Normalized);

    if (Normalized.IsEmpty())
    {
        return TEXT("");
    }

    if (!FPaths::IsRelative(Normalized))
    {
        return FPaths::ConvertRelativePathToFull(Normalized);
    }

    if (Normalized.StartsWith(TEXT("Content/")))
    {
        return FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / Normalized);
    }

    return FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir() / TEXT("Models") / Normalized);
}

int32 ULiteRtLmBlueprintLibrary::QueryLiteRtLmAvailableVramMB(int32 DefaultMB)
{
    return ULiteRtLmSubsystem::QueryAvailableVramMB(DefaultMB);
}

bool ULiteRtLmBlueprintLibrary::LoadLiteRtLmModel(const FLiteRtLmConfig& Config)
{
    return FLiteRtLmUnrealApi::LoadModel(Config);
}

bool ULiteRtLmBlueprintLibrary::LoadLiteRtLmModelFromPath(
    const FString& ModelPath,
    bool bUseAutoConfig,
    const FString& Backend,
    int32 MaxNumTokens,
    int32 NumThreads,
    bool bEnableBenchmark,
    bool bOptimizeShader,
    bool bEnableVision,
    bool bEnableAudio,
    bool bEnableStreaming)
{
    return FLiteRtLmUnrealApi::LoadModel(
        LiteRtLmBlueprintLibrary::BuildConfig(
            ModelPath,
            bUseAutoConfig,
            Backend,
            MaxNumTokens,
            NumThreads,
            bEnableBenchmark,
            bOptimizeShader,
            bEnableVision,
            bEnableAudio,
            bEnableStreaming));
}

bool ULiteRtLmBlueprintLibrary::LoadLiteRtLmProjectModel(
    const FString& ModelFileName,
    bool bUseAutoConfig,
    const FString& Backend,
    int32 MaxNumTokens,
    int32 NumThreads,
    bool bEnableBenchmark,
    bool bOptimizeShader,
    bool bEnableVision,
    bool bEnableAudio,
    bool bEnableStreaming)
{
    return LoadLiteRtLmModelFromPath(
        ResolveLiteRtLmProjectModelPath(ModelFileName),
        bUseAutoConfig,
        Backend,
        MaxNumTokens,
        NumThreads,
        bEnableBenchmark,
        bOptimizeShader,
        bEnableVision,
        bEnableAudio,
        bEnableStreaming);
}

void ULiteRtLmBlueprintLibrary::UnloadLiteRtLmModel()
{
    FLiteRtLmUnrealApi::UnloadModel();
}

bool ULiteRtLmBlueprintLibrary::IsLiteRtLmModelLoaded()
{
    return FLiteRtLmUnrealApi::IsModelLoaded();
}

void ULiteRtLmBlueprintLibrary::SendLiteRtLmTextChat(
    UObject* WorldContextObject,
    const FString& UserMessage,
    UObject* SessionOwner,
    FLiteRtLmBlueprintChunkDelegate OnChunk,
    FLiteRtLmBlueprintDoneDelegate OnDone,
    FLiteRtLmSamplingParams SamplingParams)
{
    TArray<TSharedPtr<FJsonObject>> Messages;
    Messages.Add(LiteRtLmBlueprintLibrary::MakeUserMessage(UserMessage, TEXT(""), TEXT("")));

    LiteRtLmBlueprintLibrary::SendMessages(
        WorldContextObject,
        SessionOwner,
        Messages,
        TEXT(""),
        OnChunk,
        OnDone,
        SamplingParams);
}

void ULiteRtLmBlueprintLibrary::SendLiteRtLmTextChatWithSystemPrompt(
    UObject* WorldContextObject,
    const FString& UserMessage,
    const FString& SystemPrompt,
    UObject* SessionOwner,
    FLiteRtLmBlueprintChunkDelegate OnChunk,
    FLiteRtLmBlueprintDoneDelegate OnDone,
    FLiteRtLmSamplingParams SamplingParams)
{
    TArray<TSharedPtr<FJsonObject>> Messages;
    if (!SystemPrompt.IsEmpty())
    {
        Messages.Add(LiteRtLmBlueprintLibrary::MakeTextMessage(TEXT("system"), SystemPrompt));
    }
    Messages.Add(LiteRtLmBlueprintLibrary::MakeUserMessage(UserMessage, TEXT(""), TEXT("")));

    LiteRtLmBlueprintLibrary::SendMessages(
        WorldContextObject,
        SessionOwner,
        Messages,
        TEXT(""),
        OnChunk,
        OnDone,
        SamplingParams);
}

void ULiteRtLmBlueprintLibrary::SendLiteRtLmMultimodalChat(
    UObject* WorldContextObject,
    const FString& UserMessage,
    const FString& ImagePath,
    const FString& AudioPath,
    const FString& SystemPrompt,
    UObject* SessionOwner,
    FLiteRtLmBlueprintChunkDelegate OnChunk,
    FLiteRtLmBlueprintDoneDelegate OnDone,
    FLiteRtLmSamplingParams SamplingParams)
{
    TArray<TSharedPtr<FJsonObject>> Messages;
    if (!SystemPrompt.IsEmpty())
    {
        Messages.Add(LiteRtLmBlueprintLibrary::MakeTextMessage(TEXT("system"), SystemPrompt));
    }
    Messages.Add(LiteRtLmBlueprintLibrary::MakeUserMessage(UserMessage, ImagePath, AudioPath));

    LiteRtLmBlueprintLibrary::SendMessages(
        WorldContextObject,
        SessionOwner,
        Messages,
        TEXT(""),
        OnChunk,
        OnDone,
        SamplingParams);
}

void ULiteRtLmBlueprintLibrary::SendLiteRtLmJsonChat(
    UObject* WorldContextObject,
    const FString& MessagesJson,
    const FString& ToolsJson,
    UObject* SessionOwner,
    FLiteRtLmBlueprintChunkDelegate OnChunk,
    FLiteRtLmBlueprintDoneDelegate OnDone,
    FLiteRtLmSamplingParams SamplingParams)
{
    TArray<TSharedPtr<FJsonObject>> Messages;
    FString ParseError;
    if (!LiteRtLmBlueprintLibrary::ParseMessagesJson(MessagesJson, Messages, ParseError))
    {
        LiteRtLmBlueprintLibrary::ExecuteError(OnDone, ParseError);
        return;
    }

    LiteRtLmBlueprintLibrary::SendMessages(
        WorldContextObject,
        SessionOwner,
        Messages,
        ToolsJson,
        OnChunk,
        OnDone,
        SamplingParams);
}

void ULiteRtLmBlueprintLibrary::ReleaseLiteRtLmSession(UObject* WorldContextObject, UObject* SessionOwner)
{
    if (UObject* ResolvedSessionOwner = LiteRtLmBlueprintLibrary::ResolveSessionOwner(WorldContextObject, SessionOwner))
    {
        FLiteRtLmUnrealApi::ReleaseSession(ResolvedSessionOwner);
    }
}
