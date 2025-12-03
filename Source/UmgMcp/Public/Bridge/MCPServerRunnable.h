#pragma once

#include "CoreMinimal.h"
#include "HAL/Runnable.h"
#include "Sockets.h"
#include "Interfaces/IPv4/IPv4Address.h"

class UUmgMcpBridge;

/**
 * @brief Implements the FRunnable for the dedicated TCP server thread.
 *
 * This class encapsulates the logic for the TCP server's main loop, which runs on a
 * separate thread to avoid blocking the Unreal Editor's main thread. Its primary
 * responsibility in the `Run()` method is to accept incoming client connections and
 * handle message reception. When a full message (JSON command) is received, it passes
 * the command string to the UUmgMcpBridge for execution.
 */
class FMCPServerRunnable : public FRunnable
{
public:
	FMCPServerRunnable(UUmgMcpBridge* InBridge, TSharedPtr<FSocket> InListenerSocket);
	virtual ~FMCPServerRunnable();

	// FRunnable interface
	virtual bool Init() override;
	virtual uint32 Run() override;
	virtual void Stop() override;
	virtual void Exit() override;

protected:
	void HandleClientConnection(TSharedPtr<FSocket> ClientSocket);
	void ProcessMessage(TSharedPtr<FSocket> Client, const FString& Message);

private:
	UUmgMcpBridge* Bridge;
	TSharedPtr<FSocket> ListenerSocket;
	TSharedPtr<FSocket> ClientSocket;
	bool bRunning;
}; 