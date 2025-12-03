#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "UmgMcpBlueprintLibrary.generated.h"

class UWidgetBlueprint;
class UK2Node;

/**
 * Handles Blueprint Graph operations for UMG MCP.
 */
class FUmgMcpBlueprintLibrary
{
public:
    FUmgMcpBlueprintLibrary();

    /**
     * Adds an event node (e.g., OnClicked) to the widget blueprint.
     */
    static TSharedPtr<FJsonObject> AddEventNode(const TSharedPtr<FJsonObject>& Params);

    /**
     * Adds a function call node (e.g., PlayAnimation) to the widget blueprint.
     */
    static TSharedPtr<FJsonObject> AddCallFunctionNode(const TSharedPtr<FJsonObject>& Params);

private:
    /**
     * Helper to find a function entry in the blueprint's skeleton or ubergraph.
     */
    static UK2Node* FindEventNode(UWidgetBlueprint* Blueprint, const FString& WidgetName, const FString& EventName);
};
