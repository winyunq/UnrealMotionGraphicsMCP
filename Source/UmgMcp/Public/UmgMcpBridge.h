#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "Http.h"
#include "Json.h"
#include "Interfaces/IPv4/IPv4Address.h"
#include "Interfaces/IPv4/IPv4Endpoint.h"
#include "Commands/UmgMcpEditorCommands.h"
#include "Commands/UmgMcpBlueprintCommands.h"
#include "Commands/UmgMcpAttentionCommands.h"
// Include the new WidgetCommands header
#include "Commands/UmgMcpWidgetCommands.h" // This line was added/moved
#include "Commands/UmgMcpFileTransformationCommands.h" // Add this line

#include "UmgMcpBridge.generated.h"

class FMCPServerRunnable;
class FUmgMcpAttentionCommands;
class FUmgMcpWidgetCommands; // Forward declaration for Widget Commands
class FUmgMcpFileTransformationCommands; // Forward declaration for File Transformation Commands

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
    TSharedPtr<FUmgMcpEditorCommands> EditorCommands;
    TSharedPtr<FUmgMcpBlueprintCommands> BlueprintCommands;
    TSharedPtr<FUmgMcpAttentionCommands> AttentionCommands;
    TSharedPtr<FUmgMcpWidgetCommands> WidgetCommands;
    TSharedPtr<FUmgMcpFileTransformationCommands> FileTransformationCommands; // Add this line

    // Server state variables
    bool bIsRunning;
    TSharedPtr<FSocket> ListenerSocket;
    TSharedPtr<FSocket> ConnectionSocket;
    FRunnableThread* ServerThread;
    int32 Port;
    FIPv4Address ServerAddress;
};