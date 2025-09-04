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
	FString GetWidgetTree(const FString& AssetPath);

	UFUNCTION(BlueprintCallable, Category = "UMG MCP|Get")
	FString QueryWidgetProperties(const FString& WidgetId, const TArray<FString>& Properties);

	UFUNCTION(BlueprintCallable, Category = "UMG MCP|Get")
	FString GetLayoutData(const FString& AssetPath, int32 ResolutionWidth = 1920, int32 ResolutionHeight = 1080);

	UFUNCTION(BlueprintCallable, Category = "UMG MCP|Get")
	bool CheckWidgetOverlap(const FString& AssetPath, const TArray<FString>& WidgetIds);

	UFUNCTION(BlueprintCallable, Category = "UMG MCP|Get")
	FString GetAssetFileSystemPath(const FString& AssetPath);
};
