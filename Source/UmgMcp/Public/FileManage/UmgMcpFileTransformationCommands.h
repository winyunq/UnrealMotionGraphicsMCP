// UmgMcpFileTransformationCommands.h
#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "UmgFileTransformation.h" // Include our file transformation utility

/**
 * @brief Handles MCP commands for "compiling" and "decompiling" UMG assets.
 *
 * This class processes commands related to the file-based version control workflow,
 * specifically `export_umg_to_json` and `apply_json_to_umg`. It acts as a command
 * router, dispatching requests to the static functions in the UUmgFileTransformation
 * utility class to perform the actual file operations.
 */
class FUmgMcpFileTransformationCommands
{
public:
    TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);
};