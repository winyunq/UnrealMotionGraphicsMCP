// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#include "FabServer/AIProviders/Local/UmgMcpLiteRtLmAiProvider.h"
#include "FabServer/UmgMcpSettings.h"
#include "FabServer/Tools/UmgMcpToolSchemaBuilder.h"
#include "FabServer/ChatSystem/UmgMcpTaskSubsystem.h"
#include "FabServer/ChatSystem/UmgMcpActiveMessageSubsystem.h"
#include "FabServer/Agent/UmgMcpAgent.h"
#include "FabServer/UmgMcpSettings.h"
#include "LiteRtLmUnrealApi.h"
#include "LiteRtLmSubsystem.h"
#include "Engine/Engine.h"
#include "Editor.h"
#include "Async/Async.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "Widgets/Text/STextBlock.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

#define LOCTEXT_NAMESPACE "FUmgMcpLiteRtLmAiProvider"

// ============================================================
// Lifecycle
// ============================================================

FUmgMcpLiteRtLmAiProvider::FUmgMcpLiteRtLmAiProvider(const FString& InModelFile)
    : ModelFile(InModelFile)
{
}

FUmgMcpLiteRtLmAiProvider::~FUmgMcpLiteRtLmAiProvider()
{
    UnloadModel();
}

void FUmgMcpLiteRtLmAiProvider::Stop()
{
    bStopRequested = true;
}

void FUmgMcpLiteRtLmAiProvider::UnloadModel()
{
    UE_LOG(LogTemp, Log, TEXT("[LiteRtLmProvider] UnloadModel called."));

    FLiteRtLmUnrealApi::UnloadModel();
}

// ============================================================
// EnsureModelLoadedAsync
// ============================================================

void FUmgMcpLiteRtLmAiProvider::EnsureModelLoadedAsync(TFunction<void(bool)> OnComplete)
{
    ULiteRtLmSubsystem* LiteRtLm = GEngine ? GEngine->GetEngineSubsystem<ULiteRtLmSubsystem>() : nullptr;
    if (!LiteRtLm)
    {
        if (OnComplete) OnComplete(false);
        return;
    }

    if (LiteRtLm->IsModelLoaded())
    {
        if (OnComplete) OnComplete(true);
        return;
    }

    // Resolve model path (relative → plugin Resources/Models directory)
    FString AbsPath = ModelFile;
    if (FPaths::IsRelative(ModelFile))
    {
        const FString& BaseDir = UUmgMcpSettings::PluginBaseDir;
        if (!BaseDir.IsEmpty())
        {
            AbsPath = FPaths::ConvertRelativePathToFull(
                FPaths::Combine(BaseDir, TEXT("Resources"), TEXT("Models"), ModelFile));
        }
    }

    if (UUmgMcpActiveMessageSubsystem* MessageSys =
            GEditor ? GEditor->GetEditorSubsystem<UUmgMcpActiveMessageSubsystem>() : nullptr)
    {
        MessageSys->UpdateActiveMessageMeta(
            TEXT("System"),
            LOCTEXT("LiteRtLmLoadingModel", "Loading local LiteRT-LM model (this may take a few seconds)..."),
            true);
    }

    // Build config with VRAM auto-detection (capped at 4GB)
    FLiteRtLmConfig Config = FLiteRtLmUnrealApi::GetAutoConfig();
    Config.ModelPath = AbsPath;
    Config.bEnableStreaming = true;

    if (const UUmgMcpSettings* Settings = GetDefault<UUmgMcpSettings>())
    {
        Config.MaxNumTokens = FMath::Max(512, Settings->LocalModelContextLength);
    }

    AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [Config, OnComplete]()
    {
        bool bSuccess = FLiteRtLmUnrealApi::LoadModel(Config);
        AsyncTask(ENamedThreads::GameThread, [bSuccess, OnComplete]()
        {
            if (OnComplete)
            {
                OnComplete(bSuccess);
            }
        });
    });
}

// ============================================================
// FetchModelList
// ============================================================

void FUmgMcpLiteRtLmAiProvider::FetchModelList(
    TFunction<void(bool, const TArray<FUmgMcpModelMetadata>&, const FString&)> OnComplete)
{
    TArray<FUmgMcpModelMetadata> Meta;
    Meta.Add({ ModelFile, FPaths::GetBaseFilename(ModelFile), 2048, true });
    if (OnComplete) OnComplete(true, Meta, TEXT(""));
}

// ============================================================
// Send
// ============================================================

void FUmgMcpLiteRtLmAiProvider::Send(const TArray<TSharedPtr<FJsonObject>>& Messages)
{
    bStopRequested = false;

    // ---- Subsystems ----
    UUmgMcpTaskSubsystem* TaskSys = GEditor ? GEditor->GetEditorSubsystem<UUmgMcpTaskSubsystem>() : nullptr;
    UUmgMcpActiveMessageSubsystem* MessageSys =
        GEditor ? GEditor->GetEditorSubsystem<UUmgMcpActiveMessageSubsystem>() : nullptr;
    TSharedPtr<FUmgMcpAgent> CapturedAgent = TaskSys ? TaskSys->GetActiveChatAgent() : nullptr;
    void* AgentPtr = CapturedAgent.Get();

    // ---- 1. Build ToolsJson from Agent's ActiveAllowList (Address-based Cache) ----
    FString ToolsJson;
    TSharedPtr<FMCPAllowList> ActiveList = CapturedAgent.IsValid() ? CapturedAgent->GetActiveAllowList() : nullptr;

    if (ActiveList.IsValid())
    {
        if (ActiveList.Get() == LastAllowListPtr)
        {
            ToolsJson = LastCompiledToolsJson;
        }
        else
        {
            FString UnusedSysInstruction;
            TSharedPtr<FJsonObject> ToolSchema = FUmgMcpToolSchemaBuilder::BuildToolSchema(UnusedSysInstruction, CapturedAgent);
            if (ToolSchema.IsValid())
            {
                const TArray<TSharedPtr<FJsonValue>>* FuncDecls;
                if (ToolSchema->TryGetArrayField(TEXT("function_declarations"), FuncDecls) && FuncDecls->Num() > 0)
                {
                    // Convert Gemini function_declarations to DLL tools format:
                    // [{"name":"...", "parameters":{...}}]
                    TArray<TSharedPtr<FJsonValue>> DllTools;
                    for (const auto& DeclVal : *FuncDecls)
                    {
                        TSharedPtr<FJsonObject> Decl = DeclVal->AsObject();
                        if (!Decl.IsValid()) continue;

                        TSharedPtr<FJsonObject> DllTool = MakeShared<FJsonObject>();
                        FString Name;
                        Decl->TryGetStringField(TEXT("name"), Name);
                        DllTool->SetStringField(TEXT("name"), Name);

                        const TSharedPtr<FJsonObject>* ParamsPtr;
                        if (Decl->TryGetObjectField(TEXT("parameters"), ParamsPtr) && ParamsPtr)
                        {
                            DllTool->SetObjectField(TEXT("parameters"), *ParamsPtr);
                        }

                        DllTools.Add(MakeShared<FJsonValueObject>(DllTool));
                    }

                    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ToolsJson);
                    FJsonSerializer::Serialize(DllTools, Writer);
                    
                    LastAllowListPtr = ActiveList.Get();
                    LastCompiledToolsJson = ToolsJson;
                }
            }
        }
    }

    // ---- 2. Ensure model loaded ----
    TWeakPtr<FUmgMcpLiteRtLmAiProvider, ESPMode::ThreadSafe> WeakSelfProvider = this->AsShared();

    EnsureModelLoadedAsync([this, WeakSelfProvider, Messages, CapturedAgent, MessageSys, AgentPtr, ToolsJson](bool bSuccess)
    {
        TSharedPtr<FUmgMcpLiteRtLmAiProvider, ESPMode::ThreadSafe> StrongThis = WeakSelfProvider.Pin();
        if (!StrongThis.IsValid() || StrongThis->bStopRequested) return;

        if (!bSuccess)
        {
            if (MessageSys && CapturedAgent.IsValid())
            {
                MessageSys->ShowActiveErrorMessage(
                    LOCTEXT("LiteRtLmModelNotLoaded", "LiteRT-LM model not loaded. Check Settings.").ToString(),
                    CapturedAgent);
            }
            return;
        }

        if (MessageSys && CapturedAgent.IsValid())
        {
            MessageSys->UpdateActiveMessageMeta(
                CapturedAgent->Name, LOCTEXT("LiteRtLmInfer", "Inferencing..."), true);
        }

        // ---- 3. Sampling params ----
        FLiteRtLmSamplingParams Params;
        Params.MaxTokens   = 2048; // Default to 2048 for safety
        Params.Temperature = 0.7f;
        Params.TopP        = 0.9f;
        Params.TopK        = 40;

        if (const UUmgMcpSettings* Settings = GetDefault<UUmgMcpSettings>())
        {
            // Cap at a reasonable value for local inference
            Params.MaxTokens = FMath::Clamp(Settings->LocalModelContextLength / 4, 256, 4096);
        }

        UE_LOG(LogTemp, Log, TEXT("[LiteRtLmProvider] Sending request with MaxTokens=%d"), Params.MaxTokens);

        // ---- 4. Streaming UI ----
        TSharedPtr<STextBlock> StreamingTextBlock =
            SNew(STextBlock).AutoWrapText(true).ColorAndOpacity(FLinearColor(0.8f, 0.8f, 0.8f, 0.9f));
        if (MessageSys) MessageSys->AddCustomWidgetToActiveMessage(StreamingTextBlock.ToSharedRef());

        // Chunk callback: update streaming widget
        FLiteRtLmChunkCallback OnChunk = FLiteRtLmChunkCallback::CreateLambda(
            [StreamingTextBlock, AccumulatedText = MakeShared<FString>()](const FString& Chunk)
            {
                *AccumulatedText += Chunk;
                FString TextCopy = *AccumulatedText;
                AsyncTask(ENamedThreads::GameThread, [StreamingTextBlock, TextCopy]()
                {
                    if (StreamingTextBlock.IsValid())
                        StreamingTextBlock->SetText(FText::FromString(TextCopy));
                });
            });

        // Done callback: parse response and deliver to Agent
        FLiteRtLmDoneCallback OnDone = FLiteRtLmDoneCallback::CreateLambda(
            [WeakSelfProvider, CapturedAgent, StreamingTextBlock, MessageSys]
            (const FLiteRtLmResult& Result)
            {
                // Remove streaming widget
                if (MessageSys && StreamingTextBlock.IsValid())
                    MessageSys->RemoveCustomWidgetFromActiveMessage(StreamingTextBlock.ToSharedRef());

                TSharedPtr<FUmgMcpLiteRtLmAiProvider, ESPMode::ThreadSafe> DoneThis = WeakSelfProvider.Pin();
                if (!DoneThis.IsValid() || DoneThis->bStopRequested) return;

                FUmgMcpAiResponse AiRes;
                AiRes.bSuccess = Result.ErrorMsg.IsEmpty();
                AiRes.ResponseText = Result.FullText;
                AiRes.ToolCalls = Result.ToolCalls;

                if (!AiRes.bSuccess)
                {
                    if (MessageSys && CapturedAgent.IsValid())
                        MessageSys->ShowActiveErrorMessage(Result.ErrorMsg, CapturedAgent);
                    return;
                }

                if (CapturedAgent.IsValid())
                    CapturedAgent->Receive(AiRes);
            });

        // ---- 5. Send via API ----
        FLiteRtLmUnrealApi::SendChatRequest(
            AgentPtr, Messages, ToolsJson, OnChunk, OnDone, Params);
    });
}

#undef LOCTEXT_NAMESPACE
