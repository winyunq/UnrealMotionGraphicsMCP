#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "UmgSetSubsystem.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogUmgSet, Log, All);

UCLASS()
class UMGMCP_API UUmgSetSubsystem : public UEditorSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    UFUNCTION(BlueprintCallable, Category = "UMG MCP|Set")
    bool SetWidgetProperties(class UWidgetBlueprint* WidgetBlueprint, const FString& WidgetName, const FString& PropertiesJson);

    UFUNCTION(BlueprintCallable, Category = "UMG MCP|Set")
    FString CreateWidget(class UWidgetBlueprint* WidgetBlueprint, const FString& ParentName, const FString& WidgetType, const FString& WidgetName);

    UFUNCTION(BlueprintCallable, Category = "UMG MCP|Set")
    bool DeleteWidget(class UWidgetBlueprint* WidgetBlueprint, const FString& WidgetName);

    UFUNCTION(BlueprintCallable, Category = "UMG MCP|Set")
    bool ReparentWidget(class UWidgetBlueprint* WidgetBlueprint, const FString& WidgetName, const FString& NewParentName);
};