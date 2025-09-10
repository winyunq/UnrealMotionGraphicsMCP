#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "UmgGetSubsystem.generated.h"

// Define a log category for easy debugging.
DECLARE_LOG_CATEGORY_EXTERN(LogUmgGet, Log, All);

/**
 * UEditorSubsystem for providing various "get" functionalities related to UMG.
 */
UCLASS(BlueprintType, Blueprintable)
class UMGMCP_API UUmgGetSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	// UEditorSubsystem interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// MCP Get Functions
	UFUNCTION(BlueprintCallable, Category = "UMG MCP|Get")
	FString GetWidgetTree(class UWidgetBlueprint* WidgetBlueprint);

	UFUNCTION(BlueprintCallable, Category = "UMG MCP|Get")
	FString QueryWidgetProperties(class UWidgetBlueprint* WidgetBlueprint, const FString& WidgetName, const TArray<FString>& Properties);

	UFUNCTION(BlueprintCallable, Category = "UMG MCP|Get")
	FString GetLayoutData(class UWidgetBlueprint* WidgetBlueprint, int32 ResolutionWidth = 1920, int32 ResolutionHeight = 1080);

	UFUNCTION(BlueprintCallable, Category = "UMG MCP|Get")
	bool CheckWidgetOverlap(class UWidgetBlueprint* WidgetBlueprint, const TArray<FString>& WidgetIds);

	UFUNCTION(BlueprintCallable, Category = "UMG MCP|Get")
	FString GetAssetFileSystemPath(const FString& AssetPath);
};
