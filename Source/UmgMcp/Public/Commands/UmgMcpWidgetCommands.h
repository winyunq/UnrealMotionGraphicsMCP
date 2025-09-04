#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/UserWidget.h"
#include "Components/PanelWidget.h"
#include "WidgetBlueprint.h"
#include "UObject/UObjectGlobals.h"

class FUmgMcpWidgetCommands
{
public:
    TSharedPtr<FJsonObject> HandleCommand(const FString& Command, const TSharedPtr<FJsonObject>& Params);

private:
    TSharedPtr<FJsonObject> BuildWidgetJson(UWidget* CurrentWidget);
};