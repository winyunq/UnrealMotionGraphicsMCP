#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "Http.h"
#include "Json.h"
#include "Interfaces/IPv4/IPv4Address.h"
#include "Interfaces/IPv4/IPv4Endpoint.h"
#include "Editor/UmgMcpEditorCommands.h"
#include "Blueprint/UmgMcpBlueprintCommands.h"
#include "FileManage/UmgMcpAttentionCommands.h"
// Include the new WidgetCommands header
#include "Widget/UmgMcpWidgetCommands.h" 
#include "FileManage/UmgMcpFileTransformationCommands.h"
#include "Animation/UmgMcpSequencerCommands.h" // Add Sequencer Commands

#include "UmgMcpBridge.generated.h"

class FMCPServerRunnable;
class FUmgMcpAttentionCommands;
class FUmgMcpWidgetCommands; // Forward declaration for Widget Commands
class FUmgMcpFileTransformationCommands; // Forward declaration for File Transformation Commands
class FUmgMcpSequencerCommands; // Forward declaration for Sequencer Commands
class FUmgMcpMaterialCommands; // Forward declaration for Material Commands

/**
 * @brief The central communication hub for the UMG MCP plugin.
 *
 * This editor subsystem is responsible for creating and managing a TCP server that listens for
 * commands from external clients (like an AI agent). It deserializes incoming JSON requests,
 * interprets the command type, and dispatches the command to the appropriate handler
 * (e.g., FUmgMcpWidgetCommands, FUmgMcpAttentionCommands). It acts as the primary bridge
 * between the network layer and the plugin's internal logic.
 */
UCLASS()
class UMGMCP_API UUmgMcpBridge : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	UUmgMcpBridge();
	virtual ~UUmgMcpBridge();

	// UEditorSubsystem implementation
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// Server functions
	void StartServer();
	void StopServer();
	bool IsRunning() const { return bIsRunning; }

	// Command execution
	FString ExecuteCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
    // Internal helper to execute command logic (thread-agnostic)
    FString InternalExecuteCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FUmgMcpEditorCommands> EditorCommands;
    TSharedPtr<FUmgMcpBlueprintCommands> BlueprintCommands;
    TSharedPtr<FUmgMcpAttentionCommands> AttentionCommands;
    TSharedPtr<FUmgMcpWidgetCommands> WidgetCommands;
    TSharedPtr<FUmgMcpFileTransformationCommands> FileTransformationCommands;
    TSharedPtr<FUmgMcpSequencerCommands> SequencerCommands; // Add Sequencer Commands
    TSharedPtr<FUmgMcpMaterialCommands> MaterialCommands;

    // Server state variables
    bool bIsRunning;
    TSharedPtr<FSocket> ListenerSocket;
    TSharedPtr<FSocket> ConnectionSocket;
    FRunnableThread* ServerThread;
    int32 Port;
    FIPv4Address ServerAddress;

    // Static flag to prevent multiple instances from trying to bind the same port
    static bool bGlobalServerStarted;
};