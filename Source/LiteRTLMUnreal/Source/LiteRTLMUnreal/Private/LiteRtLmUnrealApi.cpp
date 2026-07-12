// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#include "LiteRtLmUnrealApi.h"

DEFINE_LOG_CATEGORY(LogLiteRtLm);
#include "LiteRtLmSubsystem.h"
#include "Engine/Engine.h"
#include "Internal/LiteRtLmWrapperLoader.h"
#include "Async/Async.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

// ============================================================
// Lifecycle
// ============================================================

FLiteRtLmConfig FLiteRtLmUnrealApi::GetAutoConfig()
{
    FLiteRtLmConfig Config;

    // Query actual available VRAM
    const int32 AvailableMB = ULiteRtLmSubsystem::QueryAvailableVramMB(4096);

    // Conservative: use at most 4GB, or whatever is available
    static constexpr int32 MaxDefaultVramMB = 4096;
    const int32 TargetVramMB = FMath::Min(AvailableMB, MaxDefaultVramMB);

    if (TargetVramMB < 2048)
    {
        Config.MaxNumTokens = 1024;
        Config.Backend = TEXT("cpu");
    }
    else
    {
        Config.MaxNumTokens = 2048;
        Config.Backend = TEXT("gpu");
    }

    Config.bEnableVision = true;
    // Note: bEnableAudio require CPU, so bEnableAudio and bEnableVision is 冲突的。
    Config.bEnableAudio = false;

    UE_LOG(LogLiteRtLm, Log, TEXT("AutoConfig: AvailableVRAM=%d MB, Target=%d MB, Backend=%s, Vision=1, Audio=0"),
        AvailableMB, TargetVramMB, *Config.Backend);

    return Config;
}

bool FLiteRtLmUnrealApi::LoadModel(const FLiteRtLmConfig& Config)
{
    if (ULiteRtLmSubsystem* Subsystem = GEngine->GetEngineSubsystem<ULiteRtLmSubsystem>())
    {
        return Subsystem->LoadModel(Config);
    }
    return false;
}

void FLiteRtLmUnrealApi::UnloadModel()
{
    if (ULiteRtLmSubsystem* Subsystem = GEngine->GetEngineSubsystem<ULiteRtLmSubsystem>())
    {
        Subsystem->UnloadModel();
    }
}

bool FLiteRtLmUnrealApi::IsModelLoaded()
{
    if (ULiteRtLmSubsystem* Subsystem = GEngine ? GEngine->GetEngineSubsystem<ULiteRtLmSubsystem>() : nullptr)
    {
        return Subsystem->IsModelLoaded();
    }
    return false;
}

// ============================================================
// Internal: C-style callback context
// ============================================================

struct FLiteRtLmCallbackContext
{
    FLiteRtLmChunkCallback OnChunk;
    FLiteRtLmDoneCallback OnDone;
    FString FullResponse;
    FString FullJsonResponse;
    FTCHARToUTF8 Utf8Constraint; // Keep UTF8 string alive during inference

    FLiteRtLmCallbackContext(FLiteRtLmChunkCallback InChunk, FLiteRtLmDoneCallback InDone, const FString& InConstraint)
        : OnChunk(InChunk), OnDone(InDone), FullResponse(TEXT("")), FullJsonResponse(TEXT("")), Utf8Constraint(*InConstraint)
    {}
};

static LiteRtLm_SamplingParams MapSamplingParams(const FLiteRtLmSamplingParams& InParams, const FLiteRtLmCallbackContext* Ctx)
{
    LiteRtLm_SamplingParams OutParams = {};
    OutParams.temperature = InParams.Temperature;
    OutParams.top_p = InParams.TopP;
    OutParams.top_k = InParams.TopK;
    OutParams.max_tokens = InParams.MaxTokens;
    OutParams.constraint_type = (int)InParams.ConstraintType;
    
    // Crucial: Only pass string if type is not None, otherwise pass nullptr
    if (InParams.ConstraintType != ELiteRtLmConstraintType::None)
    {
        OutParams.constraint_string = Ctx->Utf8Constraint.Get();
    }
    else
    {
        OutParams.constraint_string = nullptr;
    }
    return OutParams;
}

/**
 * Parse tool_calls from the full JSON response collected during streaming.
 * The shared library returns chunks like: {"role":"assistant","tool_calls":[{"type":"function","function":{"name":"...","arguments":{...}}}]}
 * We parse tool_calls from the accumulated full_json if present.
 */
static TArray<TSharedPtr<FJsonObject>> ParseToolCallsFromJson(const FString& FullJsonAccumulated)
{
    TArray<TSharedPtr<FJsonObject>> ToolCalls;
    if (FullJsonAccumulated.IsEmpty()) return ToolCalls;

    // The full_json_chunk from the shared library may contain multiple JSON fragments.
    // The last chunk with tool_calls is the definitive one.
    // Try parsing the accumulated JSON as a single object first.
    TSharedPtr<FJsonObject> Parsed;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(FullJsonAccumulated);
    if (FJsonSerializer::Deserialize(Reader, Parsed) && Parsed.IsValid())
    {
        const TArray<TSharedPtr<FJsonValue>>* ToolCallsArr;
        if (Parsed->TryGetArrayField(TEXT("tool_calls"), ToolCallsArr))
        {
            for (const auto& TCVal : *ToolCallsArr)
            {
                TSharedPtr<FJsonObject> TC = TCVal->AsObject();
                if (TC.IsValid())
                {
                    // The shared library returns {"type":"function","function":{"name":"...","arguments":{...}}}
                    // We need to serialize arguments back to string for OpenAI compatibility
                    const TSharedPtr<FJsonObject>* FuncPtr;
                    if (TC->TryGetObjectField(TEXT("function"), FuncPtr) && FuncPtr)
                    {
                        const TSharedPtr<FJsonObject>* ArgsObjPtr;
                        if ((*FuncPtr)->TryGetObjectField(TEXT("arguments"), ArgsObjPtr) && ArgsObjPtr)
                        {
                            FString ArgsJson;
                            TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ArgsJson);
                            FJsonSerializer::Serialize((*ArgsObjPtr).ToSharedRef(), Writer);
                            (*FuncPtr)->SetStringField(TEXT("arguments"), ArgsJson);
                        }
                    }
                    // Add a unique ID if not present
                    if (!TC->HasField(TEXT("id")))
                    {
                        FString FuncName;
                        if (TC->TryGetObjectField(TEXT("function"), FuncPtr))
                        {
                            (*FuncPtr)->TryGetStringField(TEXT("name"), FuncName);
                        }
                        TC->SetStringField(TEXT("id"),
                            FString::Printf(TEXT("call_%s_%s"), *FuncName,
                                *FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8)));
                    }
                    ToolCalls.Add(TC);
                }
            }
        }
    }

    return ToolCalls;
}

static void Internal_LiteRtLmCallback(LiteRtLm_Result Result, void* UserPtr)
{
    // UE_LOG(LogLiteRtLm, Log, TEXT("Internal_LiteRtLmCallback invoked. bIsDone=%d"), Result.bIsDone);
    FLiteRtLmCallbackContext* Ctx = static_cast<FLiteRtLmCallbackContext*>(UserPtr);
    if (!Ctx) return;

    // UE_LOG(LogLiteRtLm, Log, TEXT("Callback: text=%s, json=%s, done=%d, err=%s"),
    //     Result.text_chunk ? UTF8_TO_TCHAR(Result.text_chunk) : TEXT("(null)"),
    //     Result.full_json_chunk ? TEXT("(has json)") : TEXT("(null)"),
    //     Result.bIsDone,
    //     Result.error_msg ? UTF8_TO_TCHAR(Result.error_msg) : TEXT("(none)"));

    if (Result.text_chunk)
    {
        FString Chunk = UTF8_TO_TCHAR(Result.text_chunk);
        Ctx->FullResponse += Chunk;

        if (Ctx->OnChunk.IsBound())
        {
            FLiteRtLmChunkCallback ChunkCopy = Ctx->OnChunk;
            AsyncTask(ENamedThreads::GameThread, [ChunkCopy, Chunk]() {
                ChunkCopy.ExecuteIfBound(Chunk);
            });
        }
    }

    // Accumulate full_json_chunk for tool_call parsing
    if (Result.full_json_chunk)
    {
        FString JsonChunk = UTF8_TO_TCHAR(Result.full_json_chunk);
        // Keep only the latest full JSON (each chunk is a complete message snapshot)
        Ctx->FullJsonResponse = JsonChunk;
    }

    if (Result.bIsDone)
    {
        FLiteRtLmResult FinalResult;
        FinalResult.FullText = Ctx->FullResponse;
        FinalResult.FullJson = Ctx->FullJsonResponse;
        FinalResult.ErrorMsg = Result.error_msg ? UTF8_TO_TCHAR(Result.error_msg) : TEXT("");
        FinalResult.TokensPerSec = Result.tokens_per_sec;
        FinalResult.bIsDone = true;

        // Parse tool_calls from the accumulated JSON
        FinalResult.ToolCalls = ParseToolCallsFromJson(Ctx->FullJsonResponse);

        // Clean up response text (remove any residual turn markers if the shared library leaks them)
        FinalResult.FullText.ReplaceInline(TEXT("<end_of_turn>"), TEXT(""));
        FinalResult.FullText.ReplaceInline(TEXT("<start_of_turn>"), TEXT(""));
        FinalResult.FullText.TrimStartAndEndInline();

        if (Ctx->OnDone.IsBound())
        {
            FLiteRtLmDoneCallback DoneCopy = Ctx->OnDone;
            AsyncTask(ENamedThreads::GameThread, [DoneCopy, FinalResult, Ctx]() {
                DoneCopy.ExecuteIfBound(FinalResult);
            });
        }
    }
}

// ============================================================
// Internal: Message normalization
// ============================================================

/**
 * Normalize messages for LiteRT-LM (Gemma has no system role).
 * - Merge system messages into the next user message
 * - Convert tool-result messages to user messages with structured JSON
 * - Pass through user/assistant messages with rich multi-modal content arrays
 */
TArray<TSharedPtr<FJsonObject>> FLiteRtLmUnrealApi::NormalizeMessages(
    const TArray<TSharedPtr<FJsonObject>>& InMessages)
{
    TArray<TSharedPtr<FJsonObject>> Result;
    FString SystemContent;

    for (const TSharedPtr<FJsonObject>& Msg : InMessages)
    {
        FString Role = Msg->GetStringField(TEXT("role"));
        
        // Content field can be either a string or an array of rich content items
        FString ContentStr;
        const TArray<TSharedPtr<FJsonValue>>* ContentArrayPtr = nullptr;
        bool bIsContentArray = Msg->TryGetArrayField(TEXT("content"), ContentArrayPtr);

        ULiteRtLmSubsystem* Subsystem = GEngine ? GEngine->GetEngineSubsystem<ULiteRtLmSubsystem>() : nullptr;
        const bool bVisionEnabled = Subsystem ? Subsystem->IsVisionEnabled() : false;

        // 当多模态视觉禁用时，强制将一切多模态数组“降维扁平化”为纯文本字符串，从虚幻客户端干净彻底地剥离和忽略一切图片数据
        if (bIsContentArray && !bVisionEnabled)
        {
            FString FlattenedText;
            if (ContentArrayPtr)
            {
                for (const auto& Val : *ContentArrayPtr)
                {
                    TSharedPtr<FJsonObject> Item = Val->AsObject();
                    if (Item.IsValid() && Item->GetStringField(TEXT("type")) == TEXT("text"))
                    {
                        FlattenedText += Item->GetStringField(TEXT("text"));
                    }
                }
            }
            ContentStr = FlattenedText;
            bIsContentArray = false;
            ContentArrayPtr = nullptr;
        }

        else if (!bIsContentArray)
        {
            ContentStr = Msg->GetStringField(TEXT("content"));
        }

        if (Role == TEXT("system"))
        {
            if (!bIsContentArray)
            {
                SystemContent += ContentStr + TEXT("\n\n");
            }
            continue;
        }

        TSharedPtr<FJsonObject> NewMsg = MakeShared<FJsonObject>();

        if (Role == TEXT("user"))
        {
            NewMsg->SetStringField(TEXT("role"), TEXT("user"));

            if (!SystemContent.IsEmpty())
            {
                if (bIsContentArray)
                {
                    TArray<TSharedPtr<FJsonValue>> MergedContent;
                    bool bMerged = false;
                    for (const auto& Val : *ContentArrayPtr)
                    {
                        TSharedPtr<FJsonObject> Item = Val->AsObject();
                        if (Item.IsValid() && Item->GetStringField(TEXT("type")) == TEXT("text"))
                        {
                            TSharedPtr<FJsonObject> NewTextItem = MakeShared<FJsonObject>();
                            NewTextItem->SetStringField(TEXT("type"), TEXT("text"));
                            NewTextItem->SetStringField(TEXT("text"), SystemContent + Item->GetStringField(TEXT("text")));
                            MergedContent.Add(MakeShared<FJsonValueObject>(NewTextItem));
                            bMerged = true;
                        }
                        else
                        {
                            MergedContent.Add(Val);
                        }
                    }
                    if (!bMerged)
                    {
                        TSharedPtr<FJsonObject> NewTextItem = MakeShared<FJsonObject>();
                        NewTextItem->SetStringField(TEXT("type"), TEXT("text"));
                        NewTextItem->SetStringField(TEXT("text"), SystemContent);
                        MergedContent.Insert(MakeShared<FJsonValueObject>(NewTextItem), 0);
                    }

                    // Robust auto-injection of <IMAGE> placeholders for multimodal alignment
                    bool bHasImage = false;
                    bool bHasImageTag = false;
                    int32 FirstTextIdx = -1;

                    for (int32 idx = 0; idx < MergedContent.Num(); ++idx)
                    {
                        TSharedPtr<FJsonObject> Item = MergedContent[idx]->AsObject();
                        if (Item.IsValid())
                        {
                            FString Type = Item->GetStringField(TEXT("type"));
                            if (Type == TEXT("image_url") || Type == TEXT("image"))
                            {
                                bHasImage = true;
                            }
                            else if (Type == TEXT("text"))
                            {
                                if (FirstTextIdx == -1)
                                {
                                    FirstTextIdx = idx;
                                }
                                FString TextVal = Item->GetStringField(TEXT("text"));
                                if (TextVal.Contains(TEXT("<IMAGE>")) || TextVal.Contains(TEXT("<image>")))
                                {
                                    bHasImageTag = true;
                                }
                            }
                        }
                    }

                    if (bHasImage && !bHasImageTag)
                    {
                        if (FirstTextIdx != -1)
                        {
                            TSharedPtr<FJsonObject> FirstTextObj = MergedContent[FirstTextIdx]->AsObject();
                            FString OriginalText = FirstTextObj->GetStringField(TEXT("text"));
                            FirstTextObj->SetStringField(TEXT("text"), TEXT("<IMAGE>\n") + OriginalText);
                        }
                        else
                        {
                            TSharedPtr<FJsonObject> NewTextItem = MakeShared<FJsonObject>();
                            NewTextItem->SetStringField(TEXT("type"), TEXT("text"));
                            NewTextItem->SetStringField(TEXT("text"), TEXT("<IMAGE>\n"));
                            MergedContent.Insert(MakeShared<FJsonValueObject>(NewTextItem), 0);
                        }
                    }

                    NewMsg->SetArrayField(TEXT("content"), MergedContent);
                }
                else
                {
                    NewMsg->SetStringField(TEXT("content"), SystemContent + ContentStr);
                }
                SystemContent.Empty();
            }
            else
            {
                if (bIsContentArray)
                {
                    TArray<TSharedPtr<FJsonValue>> MultimodalContent = *ContentArrayPtr;
                    
                    bool bHasImage = false;
                    bool bHasImageTag = false;
                    int32 FirstTextIdx = -1;

                    for (int32 idx = 0; idx < MultimodalContent.Num(); ++idx)
                    {
                        TSharedPtr<FJsonObject> Item = MultimodalContent[idx]->AsObject();
                        if (Item.IsValid())
                        {
                            FString Type = Item->GetStringField(TEXT("type"));
                            if (Type == TEXT("image_url") || Type == TEXT("image"))
                            {
                                bHasImage = true;
                            }
                            else if (Type == TEXT("text"))
                            {
                                if (FirstTextIdx == -1)
                                {
                                    FirstTextIdx = idx;
                                }
                                FString TextVal = Item->GetStringField(TEXT("text"));
                                if (TextVal.Contains(TEXT("<IMAGE>")) || TextVal.Contains(TEXT("<image>")))
                                {
                                    bHasImageTag = true;
                                }
                            }
                        }
                    }

                    if (bHasImage && !bHasImageTag)
                    {
                        if (FirstTextIdx != -1)
                        {
                            TSharedPtr<FJsonObject> FirstTextObj = MultimodalContent[FirstTextIdx]->AsObject();
                            FString OriginalText = FirstTextObj->GetStringField(TEXT("text"));
                            FirstTextObj->SetStringField(TEXT("text"), TEXT("<IMAGE>\n") + OriginalText);
                        }
                        else
                        {
                            TSharedPtr<FJsonObject> NewTextItem = MakeShared<FJsonObject>();
                            NewTextItem->SetStringField(TEXT("type"), TEXT("text"));
                            NewTextItem->SetStringField(TEXT("text"), TEXT("<IMAGE>\n"));
                            MultimodalContent.Insert(MakeShared<FJsonValueObject>(NewTextItem), 0);
                        }
                    }

                    NewMsg->SetArrayField(TEXT("content"), MultimodalContent);
                }
                else
                {
                    NewMsg->SetStringField(TEXT("content"), ContentStr);
                }
            }
            Result.Add(NewMsg);
            continue;
        }

        if (Role == TEXT("tool"))
        {
            FString ToolName;
            Msg->TryGetStringField(TEXT("name"), ToolName);
            FString ToolCallId;
            Msg->TryGetStringField(TEXT("tool_call_id"), ToolCallId);

            NewMsg->SetStringField(TEXT("role"), TEXT("tool"));

            TSharedPtr<FJsonObject> ToolResponseObj = MakeShared<FJsonObject>();
            ToolResponseObj->SetStringField(TEXT("name"), ToolName);

            // 尝试解析为 JSON 对象嵌套，以保证高精度的 JSON 结构体投递，若非 JSON 则保留字符串
            TSharedPtr<FJsonObject> ContentJsonObj;
            TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ContentStr);
            if (FJsonSerializer::Deserialize(Reader, ContentJsonObj) && ContentJsonObj.IsValid())
            {
                ToolResponseObj->SetObjectField(TEXT("response"), ContentJsonObj);
            }
            else
            {
                ToolResponseObj->SetStringField(TEXT("response"), ContentStr);
            }

            TSharedPtr<FJsonObject> ToolResponseWrapper = MakeShared<FJsonObject>();
            ToolResponseWrapper->SetObjectField(TEXT("tool_response"), ToolResponseObj);

            TArray<TSharedPtr<FJsonValue>> ContentArray;
            ContentArray.Add(MakeShared<FJsonValueObject>(ToolResponseWrapper));
            NewMsg->SetArrayField(TEXT("content"), ContentArray);

            Result.Add(NewMsg);
            continue;
        }

        if (Role == TEXT("assistant"))
        {
            // If assistant message has tool_calls, serialize them as part of content
            const TArray<TSharedPtr<FJsonValue>>* ToolCallsArr;
            if (Msg->TryGetArrayField(TEXT("tool_calls"), ToolCallsArr) && ToolCallsArr && ToolCallsArr->Num() > 0)
            {
                FString TextContent = bIsContentArray ? TEXT("") : ContentStr;
                if (bIsContentArray)
                {
                    for (const auto& Val : *ContentArrayPtr)
                    {
                        TSharedPtr<FJsonObject> Item = Val->AsObject();
                        if (Item.IsValid() && Item->GetStringField(TEXT("type")) == TEXT("text"))
                        {
                            TextContent += Item->GetStringField(TEXT("text"));
                        }
                    }
                }

                for (const auto& TCVal : *ToolCallsArr)
                {
                    TSharedPtr<FJsonObject> TC = TCVal->AsObject();
                    if (!TC) continue;
                    const TSharedPtr<FJsonObject>* FuncPtr;
                    if (TC->TryGetObjectField(TEXT("function"), FuncPtr))
                    {
                        FString FuncName, FuncArgs;
                        (*FuncPtr)->TryGetStringField(TEXT("name"), FuncName);
                        (*FuncPtr)->TryGetStringField(TEXT("arguments"), FuncArgs);
                        TextContent += FString::Printf(
                            TEXT("\n```tool_code\n%s(%s)\n```"),
                            *FuncName, *FuncArgs);
                    }
                }
                NewMsg->SetStringField(TEXT("role"), TEXT("assistant"));
                NewMsg->SetStringField(TEXT("content"), TextContent);
                Result.Add(NewMsg);
                continue;
            }
        }

        // Default: pass through
        NewMsg->SetStringField(TEXT("role"), Role);
        if (bIsContentArray)
        {
            NewMsg->SetArrayField(TEXT("content"), *ContentArrayPtr);
        }
        else
        {
            NewMsg->SetStringField(TEXT("content"), ContentStr);
        }
        Result.Add(NewMsg);
    }

    // Any remaining system content (no user message followed)
    if (!SystemContent.IsEmpty())
    {
        TSharedPtr<FJsonObject> SysAsUser = MakeShared<FJsonObject>();
        SysAsUser->SetStringField(TEXT("role"), TEXT("user"));
        SysAsUser->SetStringField(TEXT("content"), SystemContent.TrimEnd());
        Result.Insert(SysAsUser, 0);
    }

    return Result;
}

// ============================================================
// RestoreHistory
// ============================================================

void FLiteRtLmUnrealApi::RestoreHistory(const TArray<TSharedPtr<FJsonObject>>& HistoryMessages)
{
    for (int32 i = 0; i < HistoryMessages.Num(); ++i)
    {
        const TSharedPtr<FJsonObject>& Msg = HistoryMessages[i];
        if (!Msg.IsValid()) continue;

        FString Role = Msg->GetStringField(TEXT("role"));

        TSharedPtr<FJsonObject> MsgObj = MakeShared<FJsonObject>();
        MsgObj->SetStringField(TEXT("role"), Role);

        // 自适应拼装：若为纯文本则输出简单 String，以完美对齐底座 AppendHistoryMessage 的高兼容格式
        bool bHasRichContent = false;
        const TArray<TSharedPtr<FJsonValue>>* ContentArray = nullptr;
        if (Msg->TryGetArrayField(TEXT("content"), ContentArray) && ContentArray)
        {
            for (const auto& Val : *ContentArray)
            {
                TSharedPtr<FJsonObject> Item = Val->AsObject();
                if (Item.IsValid())
                {
                    FString Type = Item->GetStringField(TEXT("type"));
                    if (Type != TEXT("text"))
                    {
                        bHasRichContent = true;
                        break;
                    }
                }
            }
        }

        if (bHasRichContent && ContentArray)
        {
            MsgObj->SetArrayField(TEXT("content"), *ContentArray);
        }
        else
        {
            FString PlainTextContent;
            if (ContentArray)
            {
                for (const auto& Val : *ContentArray)
                {
                    TSharedPtr<FJsonObject> Item = Val->AsObject();
                    if (Item.IsValid() && Item->GetStringField(TEXT("type")) == TEXT("text"))
                    {
                        PlainTextContent += Item->GetStringField(TEXT("text"));
                    }
                }
            }
            else
            {
                PlainTextContent = Msg->GetStringField(TEXT("content"));
            }
            MsgObj->SetStringField(TEXT("content"), PlainTextContent);
        }

        FString MsgJson;
        TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&MsgJson);
        FJsonSerializer::Serialize(MsgObj.ToSharedRef(), Writer);

        UE_LOG(LogLiteRtLm, Log, TEXT("AppendHistoryMessage [%d] (JSON): %s"), i, *MsgJson.Left(200));
        FLiteRtLmWrapperLoader::AppendHistoryMessage(TCHAR_TO_UTF8(*MsgJson));
    }
}

// ============================================================
// SendChatRequest
// ============================================================

void FLiteRtLmUnrealApi::SendChatRequest(
    void* SessionKey,
    const TSharedPtr<FJsonObject>& LastMessage,
    FLiteRtLmChunkCallback OnChunk,
    FLiteRtLmDoneCallback OnDone,
    const FLiteRtLmSamplingParams& Params)
{
    ULiteRtLmSubsystem* Subsystem = GEngine ? GEngine->GetEngineSubsystem<ULiteRtLmSubsystem>() : nullptr;
    if (!Subsystem || !SessionKey)
    {
        if (OnDone.IsBound())
        {
            FLiteRtLmResult ErrResult;
            ErrResult.ErrorMsg = TEXT("LiteRT-LM Subsystem not available.");
            ErrResult.bIsDone = true;
            OnDone.ExecuteIfBound(ErrResult);
        }
        return;
    }

    if (!LastMessage.IsValid()) return;

    FString LastRole = LastMessage->GetStringField(TEXT("role"));

    if (LastRole == TEXT("assistant"))
    {
        // Edge case: last message is assistant – append it and ask for continuation
        FString AsJson;
        TSharedRef<TJsonWriter<>> W = TJsonWriterFactory<>::Create(&AsJson);
        TSharedPtr<FJsonObject> MsgObj = MakeShared<FJsonObject>();
        MsgObj->SetStringField(TEXT("role"), TEXT("assistant"));

        TArray<TSharedPtr<FJsonValue>> TextContentArray;
        TSharedPtr<FJsonObject> TextObj = MakeShared<FJsonObject>();
        TextObj->SetStringField(TEXT("type"), TEXT("text"));
        TextObj->SetStringField(TEXT("text"), LastMessage->GetStringField(TEXT("content")));
        TextContentArray.Add(MakeShared<FJsonValueObject>(TextObj));
        MsgObj->SetArrayField(TEXT("content"), TextContentArray);

        FJsonSerializer::Serialize(MsgObj.ToSharedRef(), W);
        FLiteRtLmWrapperLoader::AppendUserMessage(TCHAR_TO_UTF8(*AsJson));

        // Format Continue prompt as clean JSON
        FString ContinueJson;
        TSharedRef<TJsonWriter<>> WContinue = TJsonWriterFactory<>::Create(&ContinueJson);
        TSharedPtr<FJsonObject> ContinueObj = MakeShared<FJsonObject>();
        ContinueObj->SetStringField(TEXT("role"), TEXT("user"));

        TArray<TSharedPtr<FJsonValue>> ContinueContentArray;
        TSharedPtr<FJsonObject> ContinueTextObj = MakeShared<FJsonObject>();
        ContinueTextObj->SetStringField(TEXT("type"), TEXT("text"));
        ContinueTextObj->SetStringField(TEXT("text"), TEXT("Please continue."));
        ContinueContentArray.Add(MakeShared<FJsonValueObject>(ContinueTextObj));
        ContinueObj->SetArrayField(TEXT("content"), ContinueContentArray);

        FJsonSerializer::Serialize(ContinueObj.ToSharedRef(), WContinue);
        FLiteRtLmWrapperLoader::AppendUserMessage(TCHAR_TO_UTF8(*ContinueJson));
    }
    else
    {
        TSharedPtr<FJsonObject> MsgObj = MakeShared<FJsonObject>();
        MsgObj->SetStringField(TEXT("role"), LastRole);

        const TArray<TSharedPtr<FJsonValue>>* ContentArray = nullptr;
        if (LastMessage->TryGetArrayField(TEXT("content"), ContentArray))
        {
            MsgObj->SetArrayField(TEXT("content"), *ContentArray);
        }
        else
        {
            // 对齐 main.cpp，将纯文本也封装成带 type: 'text' 的 Typed Content 数组
            TArray<TSharedPtr<FJsonValue>> TextContentArray;
            TSharedPtr<FJsonObject> TextObj = MakeShared<FJsonObject>();
            TextObj->SetStringField(TEXT("type"), TEXT("text"));
            TextObj->SetStringField(TEXT("text"), LastMessage->GetStringField(TEXT("content")));
            TextContentArray.Add(MakeShared<FJsonValueObject>(TextObj));
            MsgObj->SetArrayField(TEXT("content"), TextContentArray);
        }

        FString UserJson;
        TSharedRef<TJsonWriter<>> W = TJsonWriterFactory<>::Create(&UserJson);
        FJsonSerializer::Serialize(MsgObj.ToSharedRef(), W);

        UE_LOG(LogLiteRtLm, Log, TEXT("AppendUserMessage (JSON): %s"), *UserJson.Left(200));
        FLiteRtLmWrapperLoader::AppendUserMessage(TCHAR_TO_UTF8(*UserJson));
    }

    // Run inference on a background thread
    void* EngineHandle = Subsystem->GetEngineHandle();
    UE_LOG(LogLiteRtLm, Log, TEXT("RunInference: Engine=%p, MaxTokens=%d, Temp=%.2f"),
        EngineHandle, Params.MaxTokens, Params.Temperature);

    FLiteRtLmCallbackContext* CallbackCtx = new FLiteRtLmCallbackContext(OnChunk, OnDone, Params.ConstraintString);
    LiteRtLm_SamplingParams CParams = MapSamplingParams(Params, CallbackCtx);

    AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [EngineHandle, CParams, CallbackCtx]()
    {
        const double StartTime = FPlatformTime::Seconds();
        UE_LOG(LogLiteRtLm, Log, TEXT("AsyncTask started on thread %d"), FPlatformTLS::GetCurrentThreadId());

        if (!FLiteRtLmWrapperLoader::RunInference)
        {
            UE_LOG(LogLiteRtLm, Error, TEXT("RunInference function pointer is NULL!"));
            return;
        }

        UE_LOG(LogLiteRtLm, Log, TEXT("Calling DLL RunInference... (Tokens=%d, Temp=%.2f)"), 
            CParams.max_tokens, CParams.temperature);
            
        FLiteRtLmWrapperLoader::RunInference(CParams, Internal_LiteRtLmCallback, CallbackCtx);
        
        const double AfterRunTime = FPlatformTime::Seconds();
        UE_LOG(LogLiteRtLm, Log, TEXT("DLL RunInference returned in %.2f ms."), (AfterRunTime - StartTime) * 1000.0);

        if (FLiteRtLmWrapperLoader::WaitUntilDone && EngineHandle)
        {
            UE_LOG(LogLiteRtLm, Log, TEXT("Entering WaitUntilDone loop..."));
            int Result = FLiteRtLmWrapperLoader::WaitUntilDone(EngineHandle, 600); // 10 min timeout
            const double EndTime = FPlatformTime::Seconds();
            UE_LOG(LogLiteRtLm, Log, TEXT("WaitUntilDone finished in %.2f ms with result: %d"), 
                (EndTime - AfterRunTime) * 1000.0, Result);
        }
        else
        {
            UE_LOG(LogLiteRtLm, Warning, TEXT("WaitUntilDone not available. EngineHandle=%p"), EngineHandle);
        }

        // 安全释放回调上下文，此时底座推理已彻底停工，主线程已安全读取，物理杜绝抢跑和 Double Free
        delete CallbackCtx;
    });
}

// ============================================================
// Session Management
// ============================================================

void FLiteRtLmUnrealApi::ReleaseSession(void* SessionKey)
{
    if (ULiteRtLmSubsystem* Subsystem = GEngine ? GEngine->GetEngineSubsystem<ULiteRtLmSubsystem>() : nullptr)
    {
        Subsystem->ReleaseAgentCache(SessionKey);
    }
}
