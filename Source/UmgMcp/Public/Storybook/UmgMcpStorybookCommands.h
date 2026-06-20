// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

/**
 * MCP command handler for UMG Storybook preview/screenshot tools.
 */
class UMGMCP_API FUmgMcpStorybookCommands
{
public:
	TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
	static int32 ResolveViewportDimension(const TSharedPtr<FJsonObject>& Params, const TCHAR* PrimaryKey, const TCHAR* AlternateKey, int32 DefaultValue);
};
