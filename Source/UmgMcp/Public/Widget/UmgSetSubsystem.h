// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "UmgSetSubsystem.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogUmgSet, Log, All);

/**
 * @brief Provides "action" capabilities for the AI to modify UMG assets.
 *
 * This subsystem acts as the "hands" of the AI agent. It contains functions for making
 * destructive or creative changes to a UMG asset. Responsibilities include creating new
 * widgets (CreateWidget), deleting existing ones (DeleteWidget), and modifying their
 * properties (SetWidgetProperties). These functions are the primary means by which the AI
 * manipulates and builds UI layouts.
 **/
UCLASS()
class UMGMCP_API UUmgSetSubsystem : public UEditorSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    /**
     * @brief Sets or updates properties of a specified widget in a UWidgetBlueprint.
     *
     * This function performs an incremental "union write" (merging properties). 
     * Only the properties explicitly specified in the incoming JSON payload will be modified; 
     * all unspecified properties will remain untouched and retain their existing values.
     *
     * @param WidgetBlueprint The target widget blueprint containing the widget.
     * @param WidgetName The name of the widget to edit. If empty or omitted at the command level, 
     *                   the active target widget currently focused in the attention context will be used.
     * @param PropertiesJson A JSON formatted string containing the properties to modify.
     * @return True if the properties were successfully set and applied, false otherwise.
     **/
    UFUNCTION(BlueprintCallable, Category = "UMG MCP|Set")
    bool SetWidgetProperties(class UWidgetBlueprint* WidgetBlueprint, const FString& WidgetName, const FString& PropertiesJson);

    UFUNCTION(BlueprintCallable, Category = "UMG MCP|Set")
    FString CreateWidget(class UWidgetBlueprint* WidgetBlueprint, const FString& ParentName, const FString& WidgetType, const FString& WidgetName);

    UFUNCTION(BlueprintCallable, Category = "UMG MCP|Set")
    bool DeleteWidget(class UWidgetBlueprint* WidgetBlueprint, const FString& WidgetName);

    UFUNCTION(BlueprintCallable, Category = "UMG MCP|Set")
    bool ReparentWidget(class UWidgetBlueprint* WidgetBlueprint, const FString& WidgetName, const FString& NewParentName);

    UFUNCTION(BlueprintCallable, Category = "UMG MCP|Set")
    bool SaveAsset(class UWidgetBlueprint* WidgetBlueprint);
};