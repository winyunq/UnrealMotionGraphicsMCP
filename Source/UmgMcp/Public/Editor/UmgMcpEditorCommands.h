#pragma once

#include "CoreMinimal.h"
#include "Json.h"

/**
 * @brief Handler for MCP commands related to general editor and level manipulation.
 *
 * This class processes commands for interacting with the main editor environment,
 * such as spawning/deleting actors in the level, finding actors, and modifying their
 * transforms. These functions provide a way for an external client to interact with
 * the level scene, separate from UMG or Blueprint asset editing.
 */
class UMGMCP_API FUmgMcpEditorCommands
{
public:
    	FUmgMcpEditorCommands();

    // Handle editor commands
    TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
    // Actor manipulation commands
    TSharedPtr<FJsonObject> HandleGetActorsInLevel(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleFindActorsByName(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSpawnActor(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleDeleteActor(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetActorTransform(const TSharedPtr<FJsonObject>& Params);

    // Blueprint actor spawning
    TSharedPtr<FJsonObject> HandleSpawnBlueprintActor(const TSharedPtr<FJsonObject>& Params);

    // Asset Registry
    TSharedPtr<FJsonObject> HandleRefreshAssetRegistry(const TSharedPtr<FJsonObject>& Params);
};