#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

/**
 * @brief Handles all MCP commands related to managing the AI's "attention".
 *
 * This class processes commands for setting and getting the current UMG context,
 * such as retrieving the last-edited asset or explicitly setting a target for
 * subsequent operations. It acts as a command router, dispatching requests to the
 * UUmgAttentionSubsystem.
 */
class FUmgMcpAttentionCommands
{
public:
	TSharedPtr<FJsonObject> HandleCommand(const FString& Command, const TSharedPtr<FJsonObject>& Params);
};