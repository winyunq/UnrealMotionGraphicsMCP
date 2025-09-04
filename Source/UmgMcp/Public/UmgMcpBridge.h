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

#include "UmgMcpBridge.generated.h"

class FMCPServerRunnable;
class FUmgMcpAttentionCommands;
class FUmgMcpWidgetCommands; // Forward declaration for Widget Commands

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
    TSharedPtr<FUmgMcpEditorCommands> EditorCommands;
    TSharedPtr<FUmgMcpBlueprintCommands> BlueprintCommands;
    TSharedPtr<FUmgMcpAttentionCommands> AttentionCommands;
    TSharedPtr<FUmgMcpWidgetCommands> WidgetCommands; // This line was added

    // Server state variables
    bool bIsRunning;
    TSharedPtr<FSocket> ListenerSocket;
    TSharedPtr<FSocket> ConnectionSocket;
    FRunnableThread* ServerThread;
    int32 Port;
    FIPv4Address ServerAddress;
};