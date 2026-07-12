// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#include "Bridge/MCPServerRunnable.h"
#include "Bridge/UmgMcpBridge.h"
#include "UmgMcp.h" // Include specifically for LogUmgMcp
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "Interfaces/IPv4/IPv4Address.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"
#include "JsonObjectConverter.h"
#include "Misc/ScopeLock.h"
#include "HAL/PlatformTime.h"
#include "Async/Async.h"
#include "Misc/ScopeLock.h"

FMCPServerRunnable::FMCPServerRunnable(UUmgMcpBridge* InBridge, TSharedPtr<FSocket> InListenerSocket)
    : Bridge(InBridge)
    , ListenerSocket(InListenerSocket)
    , bRunning(true)
    , ActiveConnectionCount(0)
{
    // UE_LOG(LogUmgMcp, Display, TEXT("MCPServerRunnable: Created server runnable"));
}

FMCPServerRunnable::~FMCPServerRunnable()
{
    // Note: We don't delete the sockets here as they're owned by the bridge
}

bool FMCPServerRunnable::Init()
{
    return true;
}

uint32 FMCPServerRunnable::Run()
{
    // UE_LOG(LogUmgMcp, Display, TEXT("MCPServerRunnable: Server thread starting..."));
    
    while (bRunning)
    {
        // UE_LOG(LogUmgMcp, Display, TEXT("MCPServerRunnable: Waiting for client connection..."));
        
        bool bPending = false;
        if (ListenerSocket->HasPendingConnection(bPending) && bPending)
        {
            // UE_LOG(LogUmgMcp, Display, TEXT("MCPServerRunnable: Client connection pending, accepting..."));
            
            ClientSocket = MakeShareable(
                ListenerSocket->Accept(TEXT("MCPClient")),
                [](FSocket* Socket)
                {
                    if (Socket) ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(Socket);
                });
            if (ClientSocket.IsValid())
            {
                // UE_LOG(LogUmgMcp, Display, TEXT("MCPServerRunnable: Client connection accepted"));
                
                // Each socket is read on a worker. Commands themselves are still forced through
                // UUmgMcpBridge's single FIFO, so clients may connect concurrently without ever
                // mutating editor state concurrently.
                TSharedPtr<FSocket> AcceptedSocket = ClientSocket;
                ActiveConnectionCount++;
                {
                    FScopeLock Lock(&ActiveSocketsCs);
                    ActiveSockets.Add(AcceptedSocket);
                }
                Async(EAsyncExecution::ThreadPool, [this, AcceptedSocket]()
                {
                    HandleClientConnection(AcceptedSocket);
                    AcceptedSocket->Close();
                    {
                        FScopeLock Lock(&ActiveSocketsCs);
                        ActiveSockets.Remove(AcceptedSocket);
                    }
                    ActiveConnectionCount--;
                });
            }
            else
            {
                // UE_LOG(LogUmgMcp, Warning, TEXT("MCPServerRunnable: Failed to accept client connection"));
            }
        }
        
        // Small sleep to prevent tight loop
        FPlatformProcess::Sleep(0.1f);
    }
    
    // UE_LOG(LogUmgMcp, Display, TEXT("MCPServerRunnable: Server thread stopping"));
    return 0;
}

void FMCPServerRunnable::Stop()
{
    bRunning = false;
    TArray<TSharedPtr<FSocket>> SocketsToClose;
    {
        FScopeLock Lock(&ActiveSocketsCs);
        SocketsToClose = ActiveSockets;
    }
    for (const TSharedPtr<FSocket>& Socket : SocketsToClose)
    {
        if (Socket.IsValid())
        {
            Socket->Close();
        }
    }

    // The bridge owns this runnable and waits for the server thread before deleting it.
    // Let short-lived connection workers leave their lambdas first.
    while (ActiveConnectionCount.Load() > 0)
    {
        FPlatformProcess::Sleep(0.001f);
    }
}

void FMCPServerRunnable::Exit()
{
}

void FMCPServerRunnable::HandleClientConnection(TSharedPtr<FSocket> InClientSocket)
{
    if (!InClientSocket.IsValid())
    {
        UE_LOG(LogUmgMcp, Error, TEXT("MCPServerRunnable: Invalid client socket passed to HandleClientConnection"));
        return;
    }

    // UE_LOG(LogUmgMcp, Display, TEXT("MCPServerRunnable: Starting to handle client connection"));
    
    // Set socket options for better connection stability
    InClientSocket->SetNonBlocking(false);
    // UE_LOG(LogUmgMcp, Display, TEXT("MCPServerRunnable: Set socket to blocking mode"));
    
    // Properly read full message with timeout
    const int32 MaxBufferSize = 4096;
    uint8 Buffer[MaxBufferSize];
    TArray<uint8> PendingData;
    
    // UE_LOG(LogUmgMcp, Display, TEXT("MCPServerRunnable: Starting message receive loop"));
    
    while (bRunning && InClientSocket.IsValid())
    {
        // Try to receive data with timeout
        int32 BytesRead = 0;
        bool bReadSuccess = false;
        
        bReadSuccess = InClientSocket->Recv(Buffer, MaxBufferSize, BytesRead, ESocketReceiveFlags::None);
        
        if (BytesRead > 0)
        {
            // Iterate through received bytes
            for (int32 i = 0; i < BytesRead; ++i)
            {
                uint8 Byte = Buffer[i];
                
                if (Byte == 0)
                {
                    // Found delimiter
                    if (PendingData.Num() > 0)
                    {
                        // Null-terminate the pending data for conversion
                        PendingData.Add(0);
                        
                        // Convert to string
                        FString Message = UTF8_TO_TCHAR((const char*)PendingData.GetData());
                        
                        // Process
                        UE_LOG(LogUmgMcp, Display, TEXT("MCPServerRunnable: Processing message (%d bytes)"), PendingData.Num() - 1);
                        ProcessMessage(InClientSocket, Message);
                        
                        // Clear for next message
                        PendingData.Empty();
                    }
                }
                else
                {
                    // Accumulate byte
                    PendingData.Add(Byte);
                }
            }
        }
        else if (!bReadSuccess)
        {
            int32 LastError = (int32)ISocketSubsystem::Get()->GetLastErrorCode();
            if (LastError != 0)
            {
                UE_LOG(LogUmgMcp, Warning, TEXT("MCPServerRunnable: Connection error occurred - Last error: %d"), LastError);
            }
            break;
        }
        
        // Small sleep to prevent tight loop
        FPlatformProcess::Sleep(0.001f); // 1ms sleep
    }
    
    // UE_LOG(LogUmgMcp, Display, TEXT("MCPServerRunnable: Exited message receive loop"));
}

void FMCPServerRunnable::ProcessMessage(TSharedPtr<FSocket> Client, const FString& Message)
{
    UE_LOG(LogUmgMcp, Display, TEXT("[UMGMCP-Message] Received: %s"), *Message);
    
    // Parse message as JSON
    TSharedPtr<FJsonObject> JsonMessage;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Message);
    
    if (!FJsonSerializer::Deserialize(Reader, JsonMessage) || !JsonMessage.IsValid())
    {
        UE_LOG(LogUmgMcp, Warning, TEXT("MCPServerRunnable: Failed to parse message as JSON"));
        return;
    }
    
    // Extract command type and parameters using MCP protocol format
    FString CommandType;
    FString ClientId;
    FString RequestId;
    TSharedPtr<FJsonObject> Params = MakeShareable(new FJsonObject());
    
    if (!JsonMessage->TryGetStringField(TEXT("command"), CommandType))
    {
        UE_LOG(LogUmgMcp, Warning, TEXT("MCPServerRunnable: Message missing 'command' field"));
        return;
    }

    JsonMessage->TryGetStringField(TEXT("client_id"), ClientId);
    JsonMessage->TryGetStringField(TEXT("request_id"), RequestId);
    
    // Parameters are optional in MCP protocol
    if (JsonMessage->HasField(TEXT("params")))
    {
        TSharedPtr<FJsonValue> ParamsValue = JsonMessage->TryGetField(TEXT("params"));
        if (ParamsValue.IsValid() && ParamsValue->Type == EJson::Object)
        {
            Params = ParamsValue->AsObject();
        }
    }
    
    // UE_LOG(LogUmgMcp, Display, TEXT("MCPServerRunnable: Executing command: %s"), *CommandType);
    
    // Execute command
    FString Response = Bridge->ExecuteCommand(CommandType, Params, ClientId, RequestId, Message);
    
    // Send response
    // Convert to UTF8
    FTCHARToUTF8 Utf8Response(*Response);

    // FSocket::Send may perform a partial write. Keep sending until the complete
    // response and its frame delimiter are on the wire.
    int32 Offset = 0;
    while (Offset < Utf8Response.Length())
    {
        int32 BytesSent = 0;
        if (!Client->Send(reinterpret_cast<const uint8*>(Utf8Response.Get()) + Offset,
            Utf8Response.Length() - Offset, BytesSent) || BytesSent <= 0)
        {
            UE_LOG(LogUmgMcp, Warning, TEXT("MCPServerRunnable: Response send failed after %d/%d bytes."), Offset, Utf8Response.Length());
            return;
        }
        Offset += BytesSent;
    }

    uint8 Delimiter = 0;
    int32 DelimiterBytesSent = 0;
    Client->Send(&Delimiter, 1, DelimiterBytesSent);
    
    UE_LOG(LogUmgMcp, Display, TEXT("[UMGMCP-Message] Sent response: %s"), *Response);
}
