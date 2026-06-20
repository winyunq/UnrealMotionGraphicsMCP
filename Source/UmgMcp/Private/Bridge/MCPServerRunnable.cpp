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

namespace MCPServerTransport
{
	static bool SendAllBytes(const TSharedPtr<FSocket>& Socket, const uint8* Data, int32 TotalBytes)
	{
		if (!Socket.IsValid() || TotalBytes <= 0)
		{
			return true;
		}

		int32 TotalSent = 0;
		while (TotalSent < TotalBytes)
		{
			int32 BytesSent = 0;
			if (!Socket->Send(Data + TotalSent, TotalBytes - TotalSent, BytesSent))
			{
				UE_LOG(LogUmgMcp, Error, TEXT("MCPServerRunnable: Socket send failed after %d/%d bytes"), TotalSent, TotalBytes);
				return false;
			}

			if (BytesSent <= 0)
			{
				UE_LOG(LogUmgMcp, Error, TEXT("MCPServerRunnable: Socket send returned 0 bytes (%d/%d)"), TotalSent, TotalBytes);
				return false;
			}

			TotalSent += BytesSent;
		}

		return true;
	}

	static void RedactImageField(TSharedPtr<FJsonObject>& JsonObject, const TCHAR* FieldName)
	{
		if (!JsonObject.IsValid() || !JsonObject->HasField(FieldName))
		{
			return;
		}

		FString Base64Value;
		if (!JsonObject->TryGetStringField(FieldName, Base64Value))
		{
			return;
		}

		JsonObject->RemoveField(FieldName);
		JsonObject->SetNumberField(TEXT("image_base64_bytes"), Base64Value.Len());
	}

	static FString SummarizeResponseForLog(const FString& Response)
	{
		TSharedPtr<FJsonObject> JsonObject;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response);
		if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
		{
			return FString::Printf(TEXT("<unparsed response, %d chars>"), Response.Len());
		}

		RedactImageField(JsonObject, TEXT("image_base64"));

		const TArray<TSharedPtr<FJsonValue>>* RendersArray = nullptr;
		if (JsonObject->TryGetArrayField(TEXT("renders"), RendersArray))
		{
			TArray<TSharedPtr<FJsonValue>> RedactedRenders;
			for (const TSharedPtr<FJsonValue>& RenderValue : *RendersArray)
			{
				const TSharedPtr<FJsonObject>* RenderObjectPtr = nullptr;
				if (RenderValue->TryGetObject(RenderObjectPtr) && RenderObjectPtr && RenderObjectPtr->IsValid())
				{
					TSharedPtr<FJsonObject> RenderCopy = MakeShared<FJsonObject>();
					for (const auto& Field : (*RenderObjectPtr)->Values)
					{
						RenderCopy->SetField(Field.Key.ToView(), Field.Value);
					}
					RedactImageField(RenderCopy, TEXT("image_base64"));
					RedactedRenders.Add(MakeShared<FJsonValueObject>(RenderCopy));
				}
				else
				{
					RedactedRenders.Add(RenderValue);
				}
			}
			JsonObject->SetArrayField(TEXT("renders"), RedactedRenders);
		}

		FString Summary;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Summary);
		FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);
		return Summary;
	}
}

FMCPServerRunnable::FMCPServerRunnable(UUmgMcpBridge* InBridge, TSharedPtr<FSocket> InListenerSocket)
    : Bridge(InBridge)
    , ListenerSocket(InListenerSocket)
    , bRunning(true)
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
            
            ClientSocket = MakeShareable(ListenerSocket->Accept(TEXT("MCPClient")));
            if (ClientSocket.IsValid())
            {
                // UE_LOG(LogUmgMcp, Display, TEXT("MCPServerRunnable: Client connection accepted"));
                
                // Use the robust handler that supports large payloads and delimiters
                HandleClientConnection(ClientSocket);
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
    TSharedPtr<FJsonObject> Params = MakeShareable(new FJsonObject());
    
    if (!JsonMessage->TryGetStringField(TEXT("command"), CommandType))
    {
        UE_LOG(LogUmgMcp, Warning, TEXT("MCPServerRunnable: Message missing 'command' field"));
        return;
    }
    
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
    FString Response = Bridge->ExecuteCommand(CommandType, Params);
    
    // Send response (loop until all UTF-8 bytes and delimiter are sent)
    FTCHARToUTF8 Utf8Response(*Response);
    const int32 ResponseByteCount = Utf8Response.Length();

    if (ResponseByteCount > 0)
    {
        if (!MCPServerTransport::SendAllBytes(Client, reinterpret_cast<const uint8*>(Utf8Response.Get()), ResponseByteCount))
        {
            UE_LOG(LogUmgMcp, Error, TEXT("MCPServerRunnable: Failed to send full response body (%d bytes)"), ResponseByteCount);
            return;
        }
    }

    const uint8 Delimiter = 0;
    if (!MCPServerTransport::SendAllBytes(Client, &Delimiter, 1))
    {
        UE_LOG(LogUmgMcp, Error, TEXT("MCPServerRunnable: Failed to send response delimiter"));
        return;
    }

    UE_LOG(
        LogUmgMcp,
        Display,
        TEXT("[UMGMCP-Message] Sent response (%d utf8 bytes): %s"),
        ResponseByteCount,
        *MCPServerTransport::SummarizeResponseForLog(Response));
}