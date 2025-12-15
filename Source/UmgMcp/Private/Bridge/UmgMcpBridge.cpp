#include "Bridge/UmgMcpBridge.h"
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
#include "EditorAssetLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "JsonObjectConverter.h"
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



UUmgMcpBridge::UUmgMcpBridge()
{
    AttentionCommands = MakeShared<FUmgMcpAttentionCommands>();
    WidgetCommands = MakeShared<FUmgMcpWidgetCommands>();
    FileTransformationCommands = MakeShared<FUmgMcpFileTransformationCommands>();
    EditorCommands = MakeShared<FUmgMcpEditorCommands>();
    BlueprintCommands = MakeShared<FUmgMcpBlueprintCommands>();
    SequencerCommands = MakeShared<FUmgMcpSequencerCommands>();
}

UUmgMcpBridge::~UUmgMcpBridge()
{
    AttentionCommands.Reset();
    WidgetCommands.Reset();
    FileTransformationCommands.Reset();
    EditorCommands.Reset();
    BlueprintCommands.Reset();
    SequencerCommands.Reset();
}

// Initialize subsystem
void UUmgMcpBridge::Initialize(FSubsystemCollectionBase& Collection)
{
    UE_LOG(LogUmgMcp, Display, TEXT("UmgMcpBridge: Initializing"));
    
    bIsRunning = false;
    ListenerSocket = nullptr;
    ConnectionSocket = nullptr;
    ServerThread = nullptr;
    Port = MCP_SERVER_PORT_DEFAULT;
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
    UE_LOG(LogUmgMcp, Display, TEXT("UmgMcpBridge: Attempting to start server on port %d..."), Port);

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
    TSharedPtr<FSocket> NewListenerSocket = MakeShareable(SocketSubsystem->CreateSocket(NAME_Stream, TEXT("UnrealMCPListener"), false));
    if (!NewListenerSocket.IsValid())
    {
        UE_LOG(LogUmgMcp, Error, TEXT("UmgMcpBridge: Failed to create listener socket: %s"), SocketSubsystem->GetSocketError(SocketSubsystem->GetLastErrorCode()));
        return;
    }

    // Allow address reuse for quick restarts
    // Note: We are keeping this ENABLED as it is standard practice, but we are aware it can cause SE_EACCES on Windows if port is excluded.
    NewListenerSocket->SetReuseAddr(true);
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
        
        return;
    }

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

    // Start server thread
    ServerThread = FRunnableThread::Create(
        new FMCPServerRunnable(this, ListenerSocket),
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

    // Clean up thread
    if (ServerThread)
    {
        ServerThread->Kill(true);
        delete ServerThread;
        ServerThread = nullptr;
    }

    // Close sockets
    if (ConnectionSocket.IsValid())
    {
        ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(ConnectionSocket.Get());
        ConnectionSocket.Reset();
    }

    if (ListenerSocket.IsValid())
    {
        ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(ListenerSocket.Get());
        ListenerSocket.Reset();
    }

    UE_LOG(LogUmgMcp, Display, TEXT("UmgMcpBridge: Server stopped"));
}

// Execute a command received from a client
FString UUmgMcpBridge::ExecuteCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    UE_LOG(LogUmgMcp, Display, TEXT("UmgMcpBridge: Received command: %s"), *CommandType);
    
    // If we are already on the GameThread (e.g. called from FabServer or test), execute directly
    if (IsInGameThread())
    {
        UE_LOG(LogUmgMcp, Verbose, TEXT("UmgMcpBridge: Already on GameThread, executing directly."));
        return InternalExecuteCommand(CommandType, Params);
    }
    
    // Otherwise, queue execution on Game Thread and wait
    // This ensures thread safety for UObject operations (creating widgets, animations, etc.)
    UE_LOG(LogUmgMcp, Verbose, TEXT("UmgMcpBridge: Dispatching to GameThread..."));
    
    TPromise<FString> Promise;
    TFuture<FString> Future = Promise.GetFuture();
    
    AsyncTask(ENamedThreads::GameThread, [this, CommandType, Params, Promise = MoveTemp(Promise)]() mutable
    {
        FString Result = InternalExecuteCommand(CommandType, Params);
        Promise.SetValue(Result);
    });
    
    // Wait for the result with a timeout to prevent infinite hang
    // This ensures we return a proper error to the client instead of letting the socket timeout
    if (Future.WaitFor(FTimespan::FromSeconds(MCP_GAME_THREAD_TIMEOUT_DEFAULT)))
    {
        return Future.Get();
    }
    else
    {
        UE_LOG(LogUmgMcp, Error, TEXT("UmgMcpBridge: GameThread execution timed out (%.1fs) for command: %s"), MCP_GAME_THREAD_TIMEOUT_DEFAULT, *CommandType);
        
        TSharedPtr<FJsonObject> ErrorResponse = MakeShareable(new FJsonObject);
        ErrorResponse->SetStringField(TEXT("status"), TEXT("error"));
        ErrorResponse->SetStringField(TEXT("error"), FString::Printf(TEXT("Game Thread Timeout - The editor may be paused or busy (Waited %.1fs)."), MCP_GAME_THREAD_TIMEOUT_DEFAULT));
        
        FString ResultString;
        TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResultString);
        FJsonSerializer::Serialize(ErrorResponse.ToSharedRef(), Writer);
        return ResultString;
    }
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
                 CommandType == TEXT("set_target_umg_asset"))
        {
            ResultJson = AttentionCommands->HandleCommand(CommandType, Params);
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
                 CommandType == TEXT("save_asset") ||
                 CommandType == TEXT("get_widget_schema"))
        {
            ResultJson = WidgetCommands->HandleCommand(CommandType, Params);
        }
        // File Transformation Commands
        else if (CommandType == TEXT("export_umg_to_json") ||
                 CommandType == TEXT("apply_json_to_umg"))
        {
            ResultJson = FileTransformationCommands->HandleCommand(CommandType, Params);
        }
        // Sequencer Commands
        else if (CommandType == TEXT("get_all_animations") ||
                 CommandType == TEXT("create_animation") ||
                 CommandType == TEXT("delete_animation") ||
                 CommandType == TEXT("set_animation_scope") ||
                 CommandType == TEXT("set_widget_scope") ||
                 CommandType == TEXT("set_property_keys") ||
                 CommandType == TEXT("remove_property_track") ||
                 CommandType == TEXT("remove_keys") ||
                 CommandType == TEXT("get_animation_keyframes") ||
                 CommandType == TEXT("get_animated_widgets") ||
                 CommandType == TEXT("get_animation_full_data") ||
                 CommandType == TEXT("get_widget_animation_data"))
        {
            ResultJson = SequencerCommands->HandleCommand(CommandType, Params);
        }
        // Editor Commands (Actors, Level, etc.)
        else if (CommandType == TEXT("get_actors_in_level") ||
                 CommandType == TEXT("find_actors_by_name") ||
                 CommandType == TEXT("spawn_actor") ||
                 CommandType == TEXT("delete_actor") ||
                 CommandType == TEXT("set_actor_transform") ||
                 CommandType == TEXT("refresh_asset_registry"))
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
        
        if (ResultJson->HasField(TEXT("success")))
        {
            bSuccess = ResultJson->GetBoolField(TEXT("success"));
            if (!bSuccess && ResultJson->HasField(TEXT("error")))
            {
                ErrorMessage = ResultJson->GetStringField(TEXT("error"));
            }
        }
        
        if (bSuccess)
        {
            // Set success status and include the result
            ResponseJson->SetStringField(TEXT("status"), TEXT("success"));
            ResponseJson->SetObjectField(TEXT("result"), ResultJson);
        }
        else
        {
            // Set error status and include the error message
            ResponseJson->SetStringField(TEXT("status"), TEXT("error"));
            ResponseJson->SetStringField(TEXT("error"), ErrorMessage);
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