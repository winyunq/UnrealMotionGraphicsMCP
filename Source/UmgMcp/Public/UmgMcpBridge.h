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
#include "Commands/UmgMcpAttentionCommands.h" // Added for Attention Commands
#include "UmgMcpBridge.generated.h"

class FMCPServerRunnable;
class FUmgMcpAttentionCommands; // Forward declaration for Attention Commands

/**
 * Editor subsystem for MCP Bridge
 * Handles communication between external tools and the Unreal Editor
 * through a TCP socket connection. Commands are received as JSON and
 * routed to appropriate command handlers.
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
	// Server state
	bool bIsRunning;
	TSharedPtr<FSocket> ListenerSocket;
	TSharedPtr<FSocket> ConnectionSocket;
	FRunnableThread* ServerThread;

	// Server configuration
	FIPv4Address ServerAddress;
	uint16 Port;

	// Command handler instances
	TSharedPtr<FUmgMcpEditorCommands> EditorCommands;
	TSharedPtr<FUmgMcpBlueprintCommands> BlueprintCommands;
	TSharedPtr<FUmgMcpAttentionCommands> AttentionCommands; // Attention Commands
};