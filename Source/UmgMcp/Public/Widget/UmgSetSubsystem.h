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

    /**
     * Reorders existing widgets under one or more panel parents using a partial tree specification.
     *
     * This is a union-style structural write: listed siblings are moved into the requested order,
     * unlisted siblings keep their relative order, and missing names are reported as warnings
     * without deleting or creating widgets.
     */
    bool ReorderWidgetTree(class UWidgetBlueprint* WidgetBlueprint, const FString& RootName, const FString& TreeSpec, TArray<FString>& OutReorderedWidgets, TArray<FString>& OutWarnings, FString& OutError);

    /**
     * @brief Performs in-place replacement by wrapping a widget with a new container.
     *
     * This function creates a new container widget based on the provided JSON specification,
     * inserts it into the target widget's original hierarchy position (inheriting the target's 
     * original parent slot layout properties), and parents the target widget under it.
     *
     * @param WidgetBlueprint The target widget blueprint.
     * @param WidgetName The name of the widget to wrap. If empty, the current active target is used.
     * @param NewParentWidgetJson A JSON string describing the new parent container to construct.
     *                            Must contain at least "widget_class" / "widget_type".
     * @return TArray<FString> An array containing the names of widgets whose slot/layout parameters 
     *                         were structurally affected (usually includes target widget and wrapper).
     **/
    UFUNCTION(BlueprintCallable, Category = "UMG MCP|Set")
    TArray<FString> ReparentWidget(class UWidgetBlueprint* WidgetBlueprint, const FString& WidgetName, const FString& NewParentWidgetJson);

    UFUNCTION(BlueprintCallable, Category = "UMG MCP|Set")
    bool SaveAsset(class UWidgetBlueprint* WidgetBlueprint);
};
