// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

/** Routes theme, catalog, and patch orchestration commands. */
class FUmgMcpOrchestrationCommands
{
public:
	TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);
};
