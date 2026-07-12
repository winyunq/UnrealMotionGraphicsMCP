// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#include "Bridge/UmgMcpBridge.h"
#include "Bridge/UmgMcpJsonCompat.h"
#include "Bridge/UmgMcpConfig.h"
#include "UmgMcp.h"
#include "Bridge/MCPServerRunnable.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "HAL/RunnableThread.h"
#include "Interfaces/IPv4/IPv4Address.h"
#include "Interfaces/IPv4/IPv4Endpoint.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/DirectionalLight.h"
#include "Engine/PointLight.h"
#include "Engine/SpotLight.h"
#include "Camera/CameraActor.h"
#include "Editor.h"
#include "EditorAssetLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "JsonObjectConverter.h"
#include "Misc/ScopeLock.h"
#include "HAL/PlatformProcess.h"
#include "GameFramework/Actor.h"
#include "Engine/Selection.h"
#include "Kismet/GameplayStatics.h"
#include "Async/Async.h"
// Add Blueprint related includes
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Factories/BlueprintFactory.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_Event.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
// UE5.5 correct includes
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "UObject/Field.h"
#include "UObject/FieldPath.h"
// Blueprint Graph specific includes
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "K2Node_CallFunction.h"
#include "K2Node_InputAction.h"
#include "K2Node_Self.h"
#include "GameFramework/InputSettings.h"
#include "EditorSubsystem.h"
#include "Subsystems/EditorActorSubsystem.h"
// Include our new command handler classes
#include "Bridge/UmgMcpCommonUtils.h"
#include "FileManage/UmgMcpFileTransformationCommands.h"
#include "Animation/UmgMcpSequencerCommands.h"
#include "Material/UmgMcpMaterialCommands.h" // Add Material Commands
#include "Blueprint/UmgBlueprintFunctionSubsystem.h"
#include "FileManage/UmgAttentionSubsystem.h"
#include "Material/UmgMcpMaterialSubsystem.h"
#include "Misc/Guid.h"
#include "Misc/DateTime.h"
#include "HAL/PlatformTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformMisc.h"

namespace
{
bool IsTargetIndependentCommand(const FString& CommandType)
{
    return CommandType == TEXT("ping") ||
        CommandType == TEXT("get_widget_schema") ||
        CommandType == TEXT("get_last_edited_umg_asset") ||
        CommandType == TEXT("get_recently_edited_umg_assets") ||
        CommandType == TEXT("list_assets");
}
}

UUmgMcpBridge::UUmgMcpBridge()
{
    bCommandQueueProcessing = false;
    ServerRunnable = nullptr;
    NextSequence = 0;
    ServerInstanceId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
    AttentionCommands = MakeShared<FUmgMcpAttentionCommands>();
    WidgetCommands = MakeShared<FUmgMcpWidgetCommands>();
    FileTransformationCommands = MakeShared<FUmgMcpFileTransformationCommands>();
    EditorCommands = MakeShared<FUmgMcpEditorCommands>();
    BlueprintCommands = MakeShared<FUmgMcpBlueprintCommands>();
    SequencerCommands = MakeShared<FUmgMcpSequencerCommands>();
    MaterialCommands = MakeShared<FUmgMcpMaterialCommands>();
}

UUmgMcpBridge::~UUmgMcpBridge()
{
    AttentionCommands.Reset();
    WidgetCommands.Reset();
    FileTransformationCommands.Reset();
    EditorCommands.Reset();
    BlueprintCommands.Reset();
    SequencerCommands.Reset();
    MaterialCommands.Reset();
}

// Initialize subsystem
void UUmgMcpBridge::Initialize(FSubsystemCollectionBase& Collection)
{
    UE_LOG(LogUmgMcp, Display, TEXT("UmgMcpBridge: Initializing"));
    
    bIsRunning = false;
    ListenerSocket = nullptr;
    ConnectionSocket = nullptr;
    ServerThread = nullptr;
    ServerRunnable = nullptr;
    Port = MCP_SERVER_PORT_DEFAULT;
    bCommandQueueProcessing = false;
    FIPv4Address::Parse(MCP_SERVER_HOST_DEFAULT, ServerAddress);

    // Start the server automatically
    StartServer();
}

// Clean up resources when subsystem is destroyed
void UUmgMcpBridge::Deinitialize()
{
    UE_LOG(LogUmgMcp, Display, TEXT("UmgMcpBridge: Shutting down"));
    StopServer();
}

// Initialize static member
bool UUmgMcpBridge::bGlobalServerStarted = false;

// Start the MCP server
void UUmgMcpBridge::StartServer()
{
    UE_LOG(LogUmgMcp, Display, TEXT("UmgMcpBridge: Attempting to start server on port %d (0 = OS assigned)..."), Port);

    if (bIsRunning)
    {
        UE_LOG(LogUmgMcp, Warning, TEXT("UmgMcpBridge: Server is already running (Instance check)"));
        return;
    }

    if (bGlobalServerStarted)
    {
        UE_LOG(LogUmgMcp, Warning, TEXT("UmgMcpBridge: Server is already running (Global check). Skipping start to avoid port conflict."));
        return;
    }

    // Create socket subsystem
    ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    if (!SocketSubsystem)
    {
        UE_LOG(LogUmgMcp, Error, TEXT("UmgMcpBridge: Failed to get socket subsystem"));
        return;
    }

    // Create listener socket
    TSharedPtr<FSocket> NewListenerSocket = MakeShareable(
        SocketSubsystem->CreateSocket(NAME_Stream, TEXT("UnrealMCPListener"), false),
        [](FSocket* Socket)
        {
            if (Socket) ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(Socket);
        });
    if (!NewListenerSocket.IsValid())
    {
        UE_LOG(LogUmgMcp, Error, TEXT("UmgMcpBridge: Failed to create listener socket: %s"), SocketSubsystem->GetSocketError(SocketSubsystem->GetLastErrorCode()));
        return;
    }

    // A shared listener port must have exactly one owner. On Windows, SO_REUSEADDR
    // can let multiple editor processes bind the same endpoint and makes discovery
    // records route clients to the wrong UE instance. A failed exclusive bind is
    // intentional here: it activates the ephemeral-port fallback below.
    NewListenerSocket->SetReuseAddr(false);
    NewListenerSocket->SetNonBlocking(true);

    // Bind to address
    FIPv4Endpoint Endpoint(ServerAddress, Port);
    if (!NewListenerSocket->Bind(*Endpoint.ToInternetAddr()))
    {
        ESocketErrors LastError = SocketSubsystem->GetLastErrorCode();
        const TCHAR* ErrorString = SocketSubsystem->GetSocketError(LastError);
        int32 ErrorCode = (int32)LastError;
        
        UE_LOG(LogUmgMcp, Error, TEXT("UmgMcpBridge: Failed to bind listener socket to %s:%d. Error Code: %d (%s)"), 
            *ServerAddress.ToString(), Port, ErrorCode, ErrorString);
            
        if (ErrorCode == 10013) // WSAEACCES
        {
            UE_LOG(LogUmgMcp, Error, TEXT("UmgMcpBridge: Port %d is likely reserved by Windows (Hyper-V/Docker). Please change the port in UmgMcpConfig.h."), Port);
        }
        else if (ErrorCode == 10048) // WSAEADDRINUSE
        {
            UE_LOG(LogUmgMcp, Error, TEXT("UmgMcpBridge: Port %d is already in use by another process."), Port);
        }
        
        // Multiple UE editor instances are a supported topology. If the well-known
        // default is occupied, ask the OS for an ephemeral port instead of disabling MCP.
        FIPv4Endpoint DynamicEndpoint(ServerAddress, 0);
        if (!NewListenerSocket->Bind(*DynamicEndpoint.ToInternetAddr()))
        {
            return;
        }

    }

    // Bind(0) succeeds with an OS-selected port. Always query the socket before
    // publishing discovery data; keeping Port == 0 would make the record unusable.
    TSharedRef<FInternetAddr> BoundAddress = SocketSubsystem->CreateInternetAddr();
    NewListenerSocket->GetAddress(*BoundAddress);
    if (BoundAddress->GetPort() <= 0)
    {
        UE_LOG(LogUmgMcp, Error, TEXT("UmgMcpBridge: Failed to resolve the assigned listener port."));
        return;
    }
    Port = BoundAddress->GetPort();
    UE_LOG(LogUmgMcp, Display, TEXT("UmgMcpBridge: Assigned unique listener port %d."), Port);

    // Start listening
    if (!NewListenerSocket->Listen(5))
    {
        ESocketErrors LastError = SocketSubsystem->GetLastErrorCode();
        UE_LOG(LogUmgMcp, Error, TEXT("UmgMcpBridge: Failed to start listening. Error Code: %d (%s)"), 
            (int32)LastError, 
            SocketSubsystem->GetSocketError(LastError));
        return;
    }

    ListenerSocket = NewListenerSocket;
    bIsRunning = true;
    bGlobalServerStarted = true; // Set global flag
    UE_LOG(LogUmgMcp, Display, TEXT("UmgMcpBridge: Server started successfully on %s:%d"), *ServerAddress.ToString(), Port);

    // Publish a small discovery record. Clients validate records with server_info, so a
    // stale file after a crash is harmless and will be ignored.
    const FString DiscoveryDir = FPaths::Combine(FPlatformProcess::UserSettingsDir(), TEXT("UmgMcp"), TEXT("instances"));
    IFileManager::Get().MakeDirectory(*DiscoveryDir, true);
    DiscoveryFilePath = FPaths::Combine(DiscoveryDir, ServerInstanceId + TEXT(".json"));
    TSharedRef<FJsonObject> Discovery = MakeShared<FJsonObject>();
    Discovery->SetStringField(TEXT("server_instance_id"), ServerInstanceId);
    Discovery->SetStringField(TEXT("host"), ServerAddress.ToString());
    Discovery->SetNumberField(TEXT("port"), Port);
    Discovery->SetStringField(TEXT("project"), FApp::GetProjectName());
    Discovery->SetStringField(TEXT("project_file"), FPaths::ConvertRelativePathToFull(FPaths::GetProjectFilePath()));
    Discovery->SetNumberField(TEXT("process_id"), FPlatformProcess::GetCurrentProcessId());
    Discovery->SetStringField(TEXT("started_at"), FDateTime::UtcNow().ToIso8601());
    FString DiscoveryJson;
    FJsonSerializer::Serialize(Discovery, TJsonWriterFactory<>::Create(&DiscoveryJson));
    FFileHelper::SaveStringToFile(DiscoveryJson, *DiscoveryFilePath);

    // Start server thread
    ServerRunnable = new FMCPServerRunnable(this, ListenerSocket);
    ServerThread = FRunnableThread::Create(
        ServerRunnable,
        TEXT("UnrealMCPServerThread"),
        0, TPri_Normal
    );

    if (!ServerThread)
    {
        UE_LOG(LogUmgMcp, Error, TEXT("UmgMcpBridge: Failed to create server thread"));
        StopServer();
        return;
    }
}

// Stop the MCP server
void UUmgMcpBridge::StopServer()
{
    if (!bIsRunning)
    {
        return;
    }

    bIsRunning = false;
    bGlobalServerStarted = false; // Reset global flag
    {
        // Deinitialization runs on the Game Thread. Wake every socket worker before
        // waiting for it, otherwise a worker waiting on a queued editor command could
        // deadlock editor shutdown.
        FScopeLock CommandLock(&CommandQueueCs);
        for (const TSharedPtr<FQueuedBridgeCommand, ESPMode::ThreadSafe>& Pending : CommandQueue)
        {
            if (Pending.IsValid())
            {
                Pending->Response = MakeErrorResponse(TEXT("UmgMcp server is shutting down."), TEXT("server_stopping"));
                if (Pending->CompletionEvent) Pending->CompletionEvent->Trigger();
            }
        }
        CommandQueue.Empty();
        bCommandQueueProcessing = false;
    }

    // Clean up thread
    if (ServerThread)
    {
        if (ServerRunnable)
        {
            ServerRunnable->Stop();
        }
        ServerThread->WaitForCompletion();
        delete ServerThread;
        ServerThread = nullptr;
    }
    delete ServerRunnable;
    ServerRunnable = nullptr;

    // Close sockets
    if (ConnectionSocket.IsValid())
    {
        ConnectionSocket->Close();
        ConnectionSocket.Reset();
    }

    if (ListenerSocket.IsValid())
    {
        ListenerSocket->Close();
        ListenerSocket.Reset();
    }

    if (!DiscoveryFilePath.IsEmpty())
    {
        IFileManager::Get().Delete(*DiscoveryFilePath, false, true);
        DiscoveryFilePath.Empty();
    }

    UE_LOG(LogUmgMcp, Display, TEXT("UmgMcpBridge: Server stopped"));
}

// Execute a command received from a client
FString UUmgMcpBridge::ExecuteCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params,
    const FString& InClientId, const FString& InRequestId, const FString& RawRequestJson)
{
    UE_LOG(LogUmgMcp, Display, TEXT("UmgMcpBridge: Received command: %s"), *CommandType);

    if (!bIsRunning && !IsInGameThread())
    {
        return MakeErrorResponse(TEXT("UmgMcp server is not running."), TEXT("server_stopped"));
    }
    
    // If we are already on the GameThread (e.g. called from FabServer or test), execute directly
    if (IsInGameThread())
    {
        UE_LOG(LogUmgMcp, Verbose, TEXT("UmgMcpBridge: Already on GameThread, executing directly."));
        TSharedPtr<FQueuedBridgeCommand, ESPMode::ThreadSafe> Direct = MakeShared<FQueuedBridgeCommand, ESPMode::ThreadSafe>();
        Direct->CommandType = CommandType;
        Direct->Params = Params;
        Direct->ClientId = InClientId.IsEmpty() ? TEXT("legacy") : InClientId;
        Direct->RequestId = InRequestId.IsEmpty() ? FGuid::NewGuid().ToString(EGuidFormats::Digits) : InRequestId;
        Direct->RawRequestJson = RawRequestJson;
        Direct->Sequence = ++NextSequence;
        Direct->EnqueuedAt = FPlatformTime::Seconds();
        return ExecuteQueuedCommand(Direct);
    }
    
    // Otherwise, queue execution on Game Thread and wait
    // This ensures thread safety for UObject operations (creating widgets, animations, etc.)
    UE_LOG(LogUmgMcp, Verbose, TEXT("UmgMcpBridge: Queueing command for GameThread execution..."));

    FEvent* CompletionEvent = FPlatformProcess::GetSynchEventFromPool(false);
    if (!CompletionEvent)
    {
        TSharedPtr<FJsonObject> ErrorResponse = MakeShareable(new FJsonObject);
        ErrorResponse->SetStringField(TEXT("status"), TEXT("error"));
        ErrorResponse->SetStringField(TEXT("error"), TEXT("Failed to allocate command completion event."));

        FString ResultString;
        TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResultString);
        FJsonSerializer::Serialize(ErrorResponse.ToSharedRef(), Writer);
        return ResultString;
    }

    TSharedRef<FQueuedBridgeCommand, ESPMode::ThreadSafe> QueuedCommand = MakeShared<FQueuedBridgeCommand, ESPMode::ThreadSafe>();
    QueuedCommand->CommandType = CommandType;
    QueuedCommand->Params = Params;
    QueuedCommand->CompletionEvent = CompletionEvent;
    QueuedCommand->ClientId = InClientId.IsEmpty() ? TEXT("legacy") : InClientId;
    QueuedCommand->RequestId = InRequestId.IsEmpty() ? FGuid::NewGuid().ToString(EGuidFormats::Digits) : InRequestId;
    QueuedCommand->RawRequestJson = RawRequestJson;
    QueuedCommand->Sequence = ++NextSequence;
    QueuedCommand->EnqueuedAt = FPlatformTime::Seconds();

    AddDebugRecord(*QueuedCommand, TEXT("queued"), TEXT(""), 0.0);

    bool bShouldScheduleProcessor = false;
    bool bRejectedDuringShutdown = false;
    {
        FScopeLock CommandLock(&CommandQueueCs);
        if (!bIsRunning)
        {
            bRejectedDuringShutdown = true;
        }
        else
        {
            CommandQueue.Add(QueuedCommand);
            if (!bCommandQueueProcessing)
            {
                bCommandQueueProcessing = true;
                bShouldScheduleProcessor = true;
            }
        }
    }

    if (bRejectedDuringShutdown)
    {
        FPlatformProcess::ReturnSynchEventToPool(CompletionEvent);
        QueuedCommand->CompletionEvent = nullptr;
        return MakeErrorResponse(TEXT("UmgMcp server is shutting down."), TEXT("server_stopping"));
    }

    if (bShouldScheduleProcessor)
    {
        AsyncTask(ENamedThreads::GameThread, [this]()
        {
            ProcessNextQueuedCommand();
        });
    }

    CompletionEvent->Wait();
    FString Result = QueuedCommand->Response;
    FPlatformProcess::ReturnSynchEventToPool(CompletionEvent);
    QueuedCommand->CompletionEvent = nullptr;
    return Result;
}

void UUmgMcpBridge::ProcessNextQueuedCommand()
{
    check(IsInGameThread());

    TSharedPtr<FQueuedBridgeCommand, ESPMode::ThreadSafe> QueuedCommand;
    {
        FScopeLock CommandLock(&CommandQueueCs);
        if (CommandQueue.Num() == 0)
        {
            bCommandQueueProcessing = false;
            return;
        }

        QueuedCommand = CommandQueue[0];
        CommandQueue.RemoveAt(0);
    }

    if (QueuedCommand.IsValid())
    {
        QueuedCommand->Response = ExecuteQueuedCommand(QueuedCommand);
        if (QueuedCommand->CompletionEvent)
        {
            QueuedCommand->CompletionEvent->Trigger();
        }
    }

    bool bHasMoreCommands = false;
    {
        FScopeLock CommandLock(&CommandQueueCs);
        bHasMoreCommands = CommandQueue.Num() > 0;
        if (!bHasMoreCommands)
        {
            bCommandQueueProcessing = false;
        }
    }

    if (bHasMoreCommands)
    {
        AsyncTask(ENamedThreads::GameThread, [this]()
        {
            ProcessNextQueuedCommand();
        });
    }
}

FString UUmgMcpBridge::MakeErrorResponse(const FString& Error, const FString& Code) const
{
    TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
    Json->SetStringField(TEXT("status"), TEXT("error"));
    Json->SetStringField(TEXT("error"), Error);
    if (!Code.IsEmpty())
    {
        Json->SetStringField(TEXT("code"), Code);
    }

    FString Result;
    FJsonSerializer::Serialize(Json, TJsonWriterFactory<>::Create(&Result));
    return Result;
}

void UUmgMcpBridge::AddDebugRecord(const FQueuedBridgeCommand& Command, const FString& State, const FString& Response, double DurationMs)
{
    FMcpDebugRecord Record;
    Record.Sequence = Command.Sequence;
    Record.Time = FDateTime::Now().ToString(TEXT("%H:%M:%S.%s"));
    Record.ClientId = Command.ClientId;
    Record.RequestId = Command.RequestId;
    Record.Command = Command.CommandType;
    Record.RequestJson = Command.RawRequestJson;
    Record.ResponseJson = Response;
    Record.State = State;
    Record.DurationMs = DurationMs;

    FScopeLock Lock(&DebugRecordsCs);
    // Replace the queued row when the same request reaches a terminal state.
    const int32 Existing = DebugRecords.IndexOfByPredicate([&Record](const FMcpDebugRecord& Item)
    {
        return Item.Sequence == Record.Sequence;
    });
    if (Existing != INDEX_NONE)
    {
        DebugRecords[Existing] = MoveTemp(Record);
    }
    else
    {
        DebugRecords.Add(MoveTemp(Record));
    }
    constexpr int32 MaxDebugRecords = 500;
    if (DebugRecords.Num() > MaxDebugRecords)
    {
        DebugRecords.RemoveAt(0, DebugRecords.Num() - MaxDebugRecords);
    }
}

void UUmgMcpBridge::GetDebugRecords(TArray<FMcpDebugRecord>& OutRecords) const
{
    FScopeLock Lock(&DebugRecordsCs);
    OutRecords = DebugRecords;
}

FString UUmgMcpBridge::ExecuteDebugMessage(const FString& Message)
{
    TSharedPtr<FJsonObject> Json;
    if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Message), Json) || !Json.IsValid())
    {
        return MakeErrorResponse(TEXT("Debug request is not valid JSON."), TEXT("invalid_json"));
    }

    FString Command;
    if (!Json->TryGetStringField(TEXT("command"), Command) || Command.IsEmpty())
    {
        return MakeErrorResponse(TEXT("Debug request is missing 'command'."), TEXT("missing_command"));
    }

    FString ClientId = TEXT("debug-ui");
    FString RequestId;
    Json->TryGetStringField(TEXT("client_id"), ClientId);
    Json->TryGetStringField(TEXT("request_id"), RequestId);
    TSharedPtr<FJsonObject> Params = MakeShared<FJsonObject>();
    const TSharedPtr<FJsonValue> ParamsValue = Json->TryGetField(TEXT("params"));
    if (ParamsValue.IsValid() && ParamsValue->Type == EJson::Object)
    {
        Params = ParamsValue->AsObject();
    }
    return ExecuteCommand(Command, Params, ClientId, RequestId, Message);
}

bool UUmgMcpBridge::RestoreSessionContext(const FString& ClientId, FString& OutError)
{
    if (ClientId.IsEmpty() || ClientId == TEXT("legacy") || !GEditor)
    {
        return true;
    }

    FConnectionSession Session;
    {
        FScopeLock Lock(&SessionCs);
        const FConnectionSession* Existing = Sessions.Find(ClientId);
        if (!Existing)
        {
            OutError = FString::Printf(TEXT("Client '%s' is not connected. Call connect first."), *ClientId);
            return false;
        }
        Session = *Existing;
    }

    if (UUmgAttentionSubsystem* Attention = GEditor->GetEditorSubsystem<UUmgAttentionSubsystem>())
    {
        if (!Session.TargetAsset.IsEmpty() && !Attention->SetTargetBlueprintAsset(Session.TargetAsset))
        {
            OutError = FString::Printf(TEXT("Could not restore target '%s' for client '%s'."), *Session.TargetAsset, *ClientId);
            return false;
        }
        if (!Session.TargetWidget.IsEmpty()) Attention->SetTargetWidget(Session.TargetWidget);
        Attention->SetTargetGraph(Session.TargetGraph);
        Attention->SetCursorNode(Session.CursorNode);
        Attention->SetTargetAnimation(Session.TargetAnimation);
    }
    if (!Session.TargetMaterial.IsEmpty())
    {
        if (UUmgMcpMaterialSubsystem* Material = GEditor->GetEditorSubsystem<UUmgMcpMaterialSubsystem>())
        {
            Material->SetTargetMaterial(Session.TargetMaterial, false);
        }
    }
    return true;
}

void UUmgMcpBridge::CaptureSessionContext(const FString& ClientId)
{
    if (ClientId.IsEmpty() || ClientId == TEXT("legacy") || !GEditor)
    {
        return;
    }

    FScopeLock Lock(&SessionCs);
    FConnectionSession* Session = Sessions.Find(ClientId);
    if (!Session)
    {
        return;
    }
    if (UUmgAttentionSubsystem* Attention = GEditor->GetEditorSubsystem<UUmgAttentionSubsystem>())
    {
        Session->TargetAsset = Attention->GetTargetUmgAsset();
        Session->TargetWidget = Attention->GetTargetWidget();
        Session->TargetGraph = Attention->GetTargetGraph();
        Session->CursorNode = Attention->GetCursorNode();
        Session->TargetAnimation = Attention->GetTargetAnimation();
    }
    if (UUmgMcpMaterialSubsystem* Material = GEditor->GetEditorSubsystem<UUmgMcpMaterialSubsystem>())
    {
        if (UMaterial* Target = Material->GetTargetMaterial())
        {
            Session->TargetMaterial = Target->GetPathName();
        }
    }
    for (auto It = TargetOwners.CreateIterator(); It; ++It)
    {
        if (It.Value() == ClientId) It.RemoveCurrent();
    }
    if (Session->bExclusiveTargets)
    {
        if (!Session->TargetAsset.IsEmpty()) TargetOwners.Add(Session->TargetAsset.ToLower(), ClientId);
        if (!Session->TargetMaterial.IsEmpty()) TargetOwners.Add(Session->TargetMaterial.ToLower(), ClientId);
    }
    Session->LastSeenAt = FDateTime::UtcNow();
}

bool UUmgMcpBridge::ValidateTargetLease(const FString& ClientId, const FString& CommandType,
    const TSharedPtr<FJsonObject>& Params, FString& OutError)
{
    if (ClientId.IsEmpty() || ClientId == TEXT("legacy") || !Params.IsValid() || IsTargetIndependentCommand(CommandType))
    {
        return true;
    }
    FString RequestedTarget;
    if (CommandType == TEXT("set_target_umg_asset") || CommandType == TEXT("set_target_blueprint_asset"))
    {
        Params->TryGetStringField(TEXT("asset_path"), RequestedTarget);
        FString Left, Right;
        if (RequestedTarget.Split(TEXT(":"), &Left, &Right)) RequestedTarget = Left;
    }
    else if (CommandType == TEXT("material_set_target") || CommandType == TEXT("hlsl_set_target"))
    {
        Params->TryGetStringField(TEXT("path"), RequestedTarget);
    }
    FScopeLock Lock(&SessionCs);
    FConnectionSession* Session = Sessions.Find(ClientId);
    if (!Session) return true;
    if (RequestedTarget.IsEmpty())
    {
        RequestedTarget = (CommandType.StartsWith(TEXT("material_")) || CommandType.StartsWith(TEXT("hlsl_")))
            ? Session->TargetMaterial : Session->TargetAsset;
    }
    if (RequestedTarget.IsEmpty()) return true;
    const FString Canonical = RequestedTarget.ToLower();
    if (const FString* Owner = TargetOwners.Find(Canonical))
    {
        if (*Owner != ClientId)
        {
            OutError = FString::Printf(TEXT("Target '%s' is exclusively bound to client '%s'."), *RequestedTarget, **Owner);
            return false;
        }
    }
    return true;
}

FString UUmgMcpBridge::HandleConnectionCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params, const FString& ClientId)
{
    TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("status"), TEXT("success"));
    if (CommandType == TEXT("connect"))
    {
        FString EffectiveId = ClientId;
        if (EffectiveId.IsEmpty() || EffectiveId == TEXT("legacy"))
        {
            Params->TryGetStringField(TEXT("client_id"), EffectiveId);
        }
        if (EffectiveId.IsEmpty()) EffectiveId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
        FConnectionSession Session;
        Session.ClientId = EffectiveId;
        Params->TryGetStringField(TEXT("display_name"), Session.DisplayName);
        Params->TryGetStringField(TEXT("target"), Session.TargetAsset);
        Params->TryGetBoolField(TEXT("exclusive"), Session.bExclusiveTargets);
        Session.ConnectedAt = FDateTime::UtcNow();
        Session.LastSeenAt = Session.ConnectedAt;
        if (Session.TargetAsset.IsEmpty() && GEditor)
        {
            if (UUmgAttentionSubsystem* Attention = GEditor->GetEditorSubsystem<UUmgAttentionSubsystem>())
            {
                Session.TargetAsset = Attention->GetTargetUmgAsset();
                Session.TargetWidget = Attention->GetTargetWidget();
                Session.TargetGraph = Attention->GetTargetGraph();
                Session.CursorNode = Attention->GetCursorNode();
                Session.TargetAnimation = Attention->GetTargetAnimation();
            }
        }
        {
            FScopeLock Lock(&SessionCs);
            if (FConnectionSession* Existing = Sessions.Find(EffectiveId))
            {
                const FString PreservedTarget = Existing->TargetAsset;
                const FString PreservedWidget = Existing->TargetWidget;
                const FString PreservedMaterial = Existing->TargetMaterial;
                *Existing = Session;
                if (Existing->TargetAsset.IsEmpty()) Existing->TargetAsset = PreservedTarget;
                Existing->TargetWidget = PreservedWidget;
                Existing->TargetMaterial = PreservedMaterial;
            }
            else Sessions.Add(EffectiveId, Session);
        }
        Result->SetStringField(TEXT("client_id"), EffectiveId);
        Result->SetStringField(TEXT("server_instance_id"), ServerInstanceId);
        Result->SetNumberField(TEXT("port"), Port);
        Result->SetBoolField(TEXT("exclusive"), Session.bExclusiveTargets);
    }
    else if (CommandType == TEXT("disconnect"))
    {
        FScopeLock Lock(&SessionCs);
        Sessions.Remove(ClientId);
        for (auto It = TargetOwners.CreateIterator(); It; ++It)
        {
            if (It.Value() == ClientId) It.RemoveCurrent();
        }
        Result->SetStringField(TEXT("client_id"), ClientId);
    }
    else if (CommandType == TEXT("server_info") || CommandType == TEXT("list_connections"))
    {
        Result->SetStringField(TEXT("server_instance_id"), ServerInstanceId);
        Result->SetNumberField(TEXT("port"), Port);
        int32 QueuedRequests = 0;
        {
            FScopeLock QueueLock(&CommandQueueCs);
            QueuedRequests = CommandQueue.Num();
        }
        Result->SetNumberField(TEXT("queued_requests"), QueuedRequests);
        TArray<TSharedPtr<FJsonValue>> Items;
        FScopeLock Lock(&SessionCs);
        for (const auto& Pair : Sessions)
        {
            TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
            Item->SetStringField(TEXT("client_id"), Pair.Key);
            Item->SetStringField(TEXT("display_name"), Pair.Value.DisplayName);
            Item->SetStringField(TEXT("target"), Pair.Value.TargetAsset);
            Item->SetStringField(TEXT("material_target"), Pair.Value.TargetMaterial);
            Item->SetBoolField(TEXT("exclusive"), Pair.Value.bExclusiveTargets);
            Item->SetStringField(TEXT("last_seen"), Pair.Value.LastSeenAt.ToIso8601());
            Items.Add(MakeShared<FJsonValueObject>(Item));
        }
        Result->SetArrayField(TEXT("connections"), Items);
    }
    FString Serialized;
    FJsonSerializer::Serialize(Result, TJsonWriterFactory<>::Create(&Serialized));
    return Serialized;
}

FString UUmgMcpBridge::ExecuteQueuedCommand(const TSharedPtr<FQueuedBridgeCommand, ESPMode::ThreadSafe>& Command)
{
    const double StartedAt = FPlatformTime::Seconds();
    FString Response;
    if (Command->CommandType == TEXT("connect") || Command->CommandType == TEXT("disconnect") ||
        Command->CommandType == TEXT("server_info") || Command->CommandType == TEXT("list_connections"))
    {
        Response = HandleConnectionCommand(Command->CommandType, Command->Params, Command->ClientId);
    }
    else
    {
        FString Error;
        if (!RestoreSessionContext(Command->ClientId, Error))
        {
            Response = MakeErrorResponse(Error, TEXT("not_connected"));
        }
        else if (!ValidateTargetLease(Command->ClientId, Command->CommandType, Command->Params, Error))
        {
            Response = MakeErrorResponse(Error, TEXT("target_locked"));
        }
        else
        {
            Response = InternalExecuteCommand(Command->CommandType, Command->Params);
            if (!IsTargetIndependentCommand(Command->CommandType))
            {
                CaptureSessionContext(Command->ClientId);
            }
        }
    }
    const double DurationMs = (FPlatformTime::Seconds() - StartedAt) * 1000.0;
    AddDebugRecord(*Command, Response.Contains(TEXT("\"status\":\"error\"")) ? TEXT("error") : TEXT("completed"), Response, DurationMs);
    return Response;
}

FString UUmgMcpBridge::InternalExecuteCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> ResponseJson = MakeShareable(new FJsonObject);
    
    try
    {
        TSharedPtr<FJsonObject> ResultJson;
        
        if (CommandType == TEXT("ping"))
        {
            ResultJson = MakeShareable(new FJsonObject);
            ResultJson->SetStringField(TEXT("message"), TEXT("pong"));
        }
        // Attention Commands
        else if (CommandType == TEXT("get_last_edited_umg_asset") ||
                 CommandType == TEXT("get_recently_edited_umg_assets") ||
                 CommandType == TEXT("get_target_umg_asset") ||
                 CommandType == TEXT("get_target_blueprint_asset") ||
                 CommandType == TEXT("get_target_widget") ||
                 CommandType == TEXT("set_target_widget") ||
                 CommandType == TEXT("set_target_umg_asset") ||
                 CommandType == TEXT("set_target_blueprint_asset"))
        {
            ResultJson = AttentionCommands->HandleCommand(CommandType, Params);
        }
        else if (CommandType == TEXT("set_target_graph") ||
                 CommandType == TEXT("get_target_graph") ||
                 CommandType == TEXT("set_cursor_node") ||
                 CommandType == TEXT("get_cursor_node") ||
                 CommandType == TEXT("set_edit_function"))
        {
              // Handle new Stateful Attention commands directly via Subsystem
              if (GEditor)
              {
                  UUmgAttentionSubsystem* AttentionSystem = GEditor->GetEditorSubsystem<UUmgAttentionSubsystem>();
                  if (AttentionSystem)
                  {
                       ResultJson = MakeShareable(new FJsonObject);
                       if (CommandType == TEXT("set_target_graph") || CommandType == TEXT("set_edit_function"))
                       {
                           FString GraphName;
                           // Support both 'graph_name' (legacy) and 'function_name' (new)
                           if (!Params->TryGetStringField(TEXT("graph_name"), GraphName))
                           {
                               Params->TryGetStringField(TEXT("function_name"), GraphName);
                           }
                           
                           if (!GraphName.IsEmpty())
                           {
                               // Function Creation / Event Binding Logic
                               UUmgBlueprintFunctionSubsystem* BPSystem = GEditor->GetEditorSubsystem<UUmgBlueprintFunctionSubsystem>();
                               UBlueprint* TargetBP = AttentionSystem->GetCachedTargetBlueprint();
                               
                               if (BPSystem && TargetBP)
                               {
                                    FString TargetNodeId;
                                    FString ActualGraphName;
                                    FString FunctionStatus;
                                    bool bSupportedTarget = true;

                                   FString ComponentName;
                                   FString EventName;
                                   
                                   // Check for "Component.Event" syntax
                                   if (GraphName.Split(TEXT("."), &ComponentName, &EventName))
                                   {
                                        // It's a Component Event!
                                        if (UWidgetBlueprint* WidgetTarget = Cast<UWidgetBlueprint>(TargetBP))
                                        {
                                            TargetNodeId = BPSystem->EnsureComponentEventExists(WidgetTarget, ComponentName, EventName, FunctionStatus);
                                            ActualGraphName = TEXT("EventGraph");
                                        }
                                        else
                                        {
                                             ResultJson->SetBoolField(TEXT("success"), false);
                                             ResultJson->SetStringField(TEXT("error"), TEXT("Component.Event targets require a Widget Blueprint. Use a graph/function name for general Blueprints."));
                                             bSupportedTarget = false;
                                        }
                                   }
                                   else
                                   {
                                       // It's a regular Function
                                       // Pass full params to allow signature definition if creation is needed
                                       FString ParamString;
                                       TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ParamString);
                                       FJsonSerializer::Serialize(Params.ToSharedRef(), Writer);
                                       
                                       TargetNodeId = BPSystem->EnsureFunctionExists(TargetBP, GraphName, FunctionStatus, ParamString);
                                       
                                       // Check if it was resolved to an Event (Custom Event or Component Event)
                                       if (FunctionStatus.Contains(TEXT("Event")))
                                       {
                                           ActualGraphName = TEXT("EventGraph");
                                       }
                                       else
                                       {
                                           ActualGraphName = GraphName;
                                       }
                                   }

                                    if (bSupportedTarget)
                                    {
                                        // Set Context
                                        AttentionSystem->SetTargetGraph(ActualGraphName);
                                        if (!TargetNodeId.IsEmpty())
                                        {
                                            AttentionSystem->SetCursorNode(TargetNodeId);
                                        }

                                        ResultJson->SetStringField(TEXT("target_graph"), ActualGraphName);
                                        ResultJson->SetStringField(TEXT("cursor_node"), TargetNodeId);
                                        ResultJson->SetStringField(TEXT("status"), FunctionStatus); // Found, Created, Inherited
                                        ResultJson->SetBoolField(TEXT("success"), true);
                                    }
                               }
                               else
                               {
                                   // Fallback if systems missing (unlikely)
                                   AttentionSystem->SetTargetGraph(GraphName);
                                   ResultJson->SetBoolField(TEXT("success"), true);
                               }
                           }
                           else
                           {
                               ResultJson->SetBoolField(TEXT("success"), false);
                               ResultJson->SetStringField(TEXT("error"), TEXT("Missing function_name or graph_name"));
                           }
                       }
                       else if (CommandType == TEXT("get_target_graph"))
                       {
                            ResultJson->SetStringField(TEXT("target_graph"), AttentionSystem->GetTargetGraph());
                            ResultJson->SetBoolField(TEXT("success"), true);
                       }
                       else if (CommandType == TEXT("set_cursor_node"))
                       {
                           FString NodeId;
                           if (Params->TryGetStringField(TEXT("node_id"), NodeId))
                           {
                               AttentionSystem->SetCursorNode(NodeId);
                               ResultJson->SetStringField(TEXT("cursor_node"), NodeId);
                               ResultJson->SetBoolField(TEXT("success"), true);
                           }
                       }
                       else if (CommandType == TEXT("get_cursor_node"))
                       {
                            ResultJson->SetStringField(TEXT("cursor_node"), AttentionSystem->GetCursorNode());
                            ResultJson->SetBoolField(TEXT("success"), true);
                       }
                  }
              }
        }
        // Widget Commands
        else if (CommandType == TEXT("get_widget_tree") ||
                 CommandType == TEXT("query_widget_properties") ||
                 CommandType == TEXT("get_layout_data") ||
                 CommandType == TEXT("check_widget_overlap") ||
                 CommandType == TEXT("create_widget") ||
                 CommandType == TEXT("set_widget_properties") ||
                 CommandType == TEXT("delete_widget") ||
                 CommandType == TEXT("reparent_widget") ||
                 CommandType == TEXT("reorder_widget_tree") ||
                 CommandType == TEXT("save_asset") ||
                 CommandType == TEXT("get_widget_schema"))
        {
            ResultJson = WidgetCommands->HandleCommand(CommandType, Params);
        }
        // File Transformation Commands
        else if (CommandType == TEXT("export_umg_to_json") ||
                 CommandType == TEXT("apply_json_to_umg") ||
                 CommandType == TEXT("apply_layout"))
        {
            ResultJson = FileTransformationCommands->HandleCommand(CommandType, Params);
        }
        // Sequencer Commands
        else if (CommandType == TEXT("get_all_animations") ||
                 CommandType == TEXT("create_animation") ||
                 CommandType == TEXT("delete_animation") ||
                 CommandType == TEXT("set_animation_scope") ||
                 CommandType == TEXT("animation_target") ||
                 CommandType == TEXT("set_widget_scope") ||
                 CommandType == TEXT("widget_target") ||
                 CommandType == TEXT("set_property_keys") ||
                 CommandType == TEXT("set_animation_data") ||
                 CommandType == TEXT("remove_property_track") ||
                 CommandType == TEXT("remove_keys") ||
                 CommandType == TEXT("get_animation_keyframes") ||
                 CommandType == TEXT("get_animated_widgets") ||
                 CommandType == TEXT("get_animation_full_data") ||
                 CommandType == TEXT("get_widget_animation_data") ||
                 CommandType == TEXT("animation_widget_properties") ||
                 CommandType == TEXT("animation_time_properties") ||
                 CommandType == TEXT("animation_overview") ||
                 CommandType == TEXT("animation_append_widget_tracks") ||
                 CommandType == TEXT("animation_append_time_slice") ||
                 CommandType == TEXT("animation_delete_widget_keys"))
        {
            ResultJson = SequencerCommands->HandleCommand(CommandType, Params);
        }
        // Editor Commands (Actors, Level, etc.)
        else if (CommandType == TEXT("get_actors_in_level") ||
                 CommandType == TEXT("find_actors_by_name") ||
                 CommandType == TEXT("spawn_actor") ||
                 CommandType == TEXT("delete_actor") ||
                 CommandType == TEXT("set_actor_transform") ||
                 CommandType == TEXT("refresh_asset_registry") ||
                 CommandType == TEXT("list_assets"))
        {
            ResultJson = EditorCommands->HandleCommand(CommandType, Params);
        }
        // Blueprint Commands
        else if (CommandType == TEXT("create_blueprint") ||
                 CommandType == TEXT("add_component_to_blueprint") ||
                 CommandType == TEXT("set_physics_properties") ||
                 CommandType == TEXT("compile_blueprint") ||
                 CommandType == TEXT("set_static_mesh_properties") ||
                 CommandType == TEXT("spawn_blueprint_actor") ||
                 CommandType == TEXT("set_mesh_material_color") ||
                 CommandType == TEXT("get_available_materials") ||
                 CommandType == TEXT("apply_material_to_actor") ||
                 CommandType == TEXT("apply_material_to_blueprint") ||
                 CommandType == TEXT("get_actor_material_info"))
        {
            ResultJson = BlueprintCommands->HandleCommand(CommandType, Params);
        }
        // Material commands
        else if (CommandType.StartsWith(TEXT("material_")) || CommandType.StartsWith(TEXT("hlsl_")))
        {
             ResultJson = MaterialCommands->HandleCommand(CommandType, Params);
        }
        // Low-level Graph Manipulation (New)
        else if (CommandType == TEXT("manage_blueprint_graph"))
        {
             if (GEditor)
             {
                 UUmgBlueprintFunctionSubsystem* GraphSystem = GEditor->GetEditorSubsystem<UUmgBlueprintFunctionSubsystem>();
                 UUmgAttentionSubsystem* AttentionSystem = GEditor->GetEditorSubsystem<UUmgAttentionSubsystem>();
                 
                 if (GraphSystem && AttentionSystem)
                 {
                     // 1. Resolve Target Blueprint
                     // We *should* use the one from Attention System if not specified? 
                     // Or just pass the Attention System's cached one.
                     UBlueprint* TargetBP = AttentionSystem->GetCachedTargetBlueprint();
                     
                     // Fallback: Check if payload specifies an asset?
                     // For now, Strict Stateful mode: Must have target set in Attention.
                     
                     if (TargetBP)
                     {
                         // 2. Inject Context (Current Graph) and Auto-Wiring Info
                         FString GraphName;
                         // Helper to get writable copy or modify existing
                         TSharedPtr<FJsonObject> ModifiedParams = MakeShareable(new FJsonObject());
                         // Copy existing fields
                         for (auto& Elem : Params->Values)
                         {
                             ModifiedParams->SetField(UmgMcpJsonCompat::KeyToString(Elem.Key), Elem.Value);
                         }

                         FString SubAction;
                         Params->TryGetStringField(TEXT("action"), SubAction);

                          
                          if (SubAction.IsEmpty())
                          {
                              Params->TryGetStringField(TEXT("subAction"), SubAction);
                          }

                          // Ensure "subAction" is present for the subsystem
                          ModifiedParams->SetStringField(TEXT("subAction"), SubAction);

                          if (!Params->HasField(TEXT("graphName")))
                          {
                             ModifiedParams->SetStringField(TEXT("graphName"), AttentionSystem->GetTargetGraph());
                         }
                         
                         // Auto-Layout & Auto-Connect
                         if (SubAction == TEXT("create_node") || SubAction == TEXT("add_node") || SubAction == TEXT("add_param") ||
                             SubAction == TEXT("add_function_step") || SubAction == TEXT("add_step_param") ||
                             SubAction == TEXT("bluecode_apply"))
                         {
                             if (!Params->HasField(TEXT("x")) || !Params->HasField(TEXT("y")))
                             {
                                 FVector2D Pos = AttentionSystem->GetAndAdvanceCursorPosition();
                                 if (!Params->HasField(TEXT("x"))) ModifiedParams->SetNumberField(TEXT("x"), Pos.X);
                                 if (!Params->HasField(TEXT("y"))) ModifiedParams->SetNumberField(TEXT("y"), Pos.Y);
                             }
                             
                             if (!Params->HasField(TEXT("autoConnectToNodeId")))
                             {
                                 FString CursorNode = AttentionSystem->GetCursorNode();
                                 if (!CursorNode.IsEmpty())
                                 {
                                     ModifiedParams->SetStringField(TEXT("autoConnectToNodeId"), CursorNode);
                                 }
                             }
                         }
                          else if (SubAction == TEXT("delete_node"))
                          {
                              if (!Params->HasField(TEXT("nodeId")))
                              {
                                  FString CursorNode = AttentionSystem->GetCursorNode();
                                  if (!CursorNode.IsEmpty())
                                  {
                                      ModifiedParams->SetStringField(TEXT("nodeId"), CursorNode);
                                  }
                              }
                          }

                         // Serialize Payload
                         FString PayloadString;
                         TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&PayloadString);
                         FJsonSerializer::Serialize(ModifiedParams.ToSharedRef(), Writer);
                         
                         FString ResultString = GraphSystem->HandleBlueprintGraphAction(TargetBP, CommandType, PayloadString);
                         
                         // Deserialize result
                         TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResultString);
                         FJsonSerializer::Deserialize(Reader, ResultJson);
                         
                         // 3. Post-Action: Update Attention Context
                         if (ResultJson.IsValid() && ResultJson->GetBoolField(TEXT("success")))
                         {
                             if (SubAction == TEXT("create_node") || SubAction == TEXT("add_node") || SubAction == TEXT("add_param") ||
                                 SubAction == TEXT("add_function_step") ||
                                 SubAction == TEXT("bluecode_apply"))
                             {
                                 FString NewNodeId;
                                 if (ResultJson->TryGetStringField(TEXT("nodeId"), NewNodeId))
                                 {
                                     // Only update Cursor (PC) if it's an Exec node
                                     bool bIsExec = false; 
                                     if (ResultJson->TryGetBoolField(TEXT("isExec"), bIsExec))
                                     {
                                         if (bIsExec)
                                         {
                                             AttentionSystem->SetCursorNode(NewNodeId);
                                         }
                                     }
                                     else 
                                     {
                                         // Fallback for older/other commands: assume Exec if not specified? 
                                         // Or assume Exec for 'add_function_step' specifically.
                                         // But since we updated CreateNodeInstance, it should be there.
                                         AttentionSystem->SetCursorNode(NewNodeId);
                                     }
                                 }
                             }
                             else if (SubAction == TEXT("delete_node"))
                             {
                                 FString NewCursor;
                                 if (ResultJson->TryGetStringField(TEXT("newCursorNode"), NewCursor))
                                 {
                                     AttentionSystem->SetCursorNode(NewCursor);
                                 }
                             }
                         }
                     }
                     else
                     {
                         ResultJson = MakeShareable(new FJsonObject);
                         ResultJson->SetStringField(TEXT("error"), TEXT("No target Blueprint set in Attention Subsystem. Use set_target_umg_asset first."));
                         ResultJson->SetBoolField(TEXT("success"), false);
                     }
                 }
             }
        }
        // --- Material Commands ---
        else if (CommandType.StartsWith(TEXT("material_")) || CommandType.StartsWith(TEXT("hlsl_")))
        {
            ResultJson = MaterialCommands->HandleCommand(CommandType, Params);
        }
        else
        {
            ResponseJson->SetStringField(TEXT("status"), TEXT("error"));
            ResponseJson->SetStringField(TEXT("error"), FString::Printf(TEXT("Unknown command: %s"), *CommandType));
            
            FString ResultString;
            TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResultString);
            FJsonSerializer::Serialize(ResponseJson.ToSharedRef(), Writer);
            return ResultString;
        }
        
        // Check if the result contains an error
        bool bSuccess = true;
        FString ErrorMessage;
        
        // Determine success by checking for "success" bool, or "status" string, or absence of "error"
        if (ResultJson->HasField(TEXT("success")))
        {
            bSuccess = ResultJson->GetBoolField(TEXT("success"));
            if (!bSuccess && ResultJson->HasField(TEXT("error")))
            {
                ErrorMessage = ResultJson->GetStringField(TEXT("error"));
            }
        }
        else if (ResultJson->HasField(TEXT("status")))
        {
            FString InnerStatus;
            ResultJson->TryGetStringField(TEXT("status"), InnerStatus);
            bSuccess = (InnerStatus != TEXT("error"));
            if (!bSuccess && ResultJson->HasField(TEXT("error")))
            {
                ErrorMessage = ResultJson->GetStringField(TEXT("error"));
            }
            else if (!bSuccess && ResultJson->HasField(TEXT("message")))
            {
                ErrorMessage = ResultJson->GetStringField(TEXT("message"));
            }
        }
        
        if (bSuccess)
        {
            // Flatten: copy all fields from ResultJson directly into ResponseJson (skip internal keys)
            ResponseJson->SetStringField(TEXT("status"), TEXT("success"));
            for (const auto& Field : ResultJson->Values)
            {
                const FString Key = UmgMcpJsonCompat::KeyToString(Field.Key);
                if (Key != TEXT("success") && Key != TEXT("status"))
                {
                    ResponseJson->SetField(Key, Field.Value);
                }
            }
        }
        else
        {
            // Preserve structured error metadata so clients can recover without reparsing text.
            ResponseJson->SetStringField(TEXT("status"), TEXT("error"));
            ResponseJson->SetStringField(TEXT("error"), ErrorMessage);
            for (const auto& Field : ResultJson->Values)
            {
                const FString Key = UmgMcpJsonCompat::KeyToString(Field.Key);
                if (Key != TEXT("success") && Key != TEXT("status") && Key != TEXT("error"))
                {
                    ResponseJson->SetField(Key, Field.Value);
                }
            }
        }
    }
    catch (const std::exception& e)
    {
        ResponseJson->SetStringField(TEXT("status"), TEXT("error"));
        ResponseJson->SetStringField(TEXT("error"), UTF8_TO_TCHAR(e.what()));
    }
    
    FString ResultString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResultString);
    FJsonSerializer::Serialize(ResponseJson.ToSharedRef(), Writer);
    return ResultString;
}
