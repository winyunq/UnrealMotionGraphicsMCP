#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/UserWidget.h"
#include "Components/PanelWidget.h"
#include "WidgetBlueprint.h"
#include "UObject/UObjectGlobals.h"

/**
 * @brief Handles all MCP commands for querying and manipulating UMG widgets.
 *
 * This class is the workhorse for interacting with the UMG widget hierarchy. It processes
 * commands for both "sensing" (e.g., get_widget_tree, query_widget_properties) and "action"
 * (e.g., set_widget_properties, create_widget, delete_widget). It dispatches these requests
 * to the appropriate subsystems (UUmgGetSubsystem and UUmgSetSubsystem) to perform the
 * actual operations on the UMG assets.
 */
class FUmgMcpWidgetCommands
{
public:
    TSharedPtr<FJsonObject> HandleCommand(const FString& Command, const TSharedPtr<FJsonObject>& Params);

private:
    TSharedPtr<FJsonObject> BuildWidgetJson(UWidget* CurrentWidget);
};