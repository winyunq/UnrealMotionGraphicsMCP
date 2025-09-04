#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class FUmgMcpAttentionCommands
{
public:
	TSharedPtr<FJsonObject> HandleCommand(const FString& Command, const TSharedPtr<FJsonObject>& Params);
};