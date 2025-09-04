#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "UmgAttentionSubsystem.generated.h"

// Desired Logic for UmgAttentionSubsystem (as per user's detailed explanation):
// 1. Track ALL editor focus changes, not just asset opening.
// 2. If focus is a UMG asset, add it to a UMG-specific history/queue (UmgAssetHistory).
// 3. GetActiveUMGContext:
//    a. If current editor focus IS a UMG asset, return that asset.
//    b. ELSE (if current editor focus is NOT a UMG asset), return the most recent UMG asset from UmgAssetHistory.
// 4. Introduce an "Operational Target UMG": A UMG asset explicitly set by the AI (e.g., via SetAttentionTarget).
//    This target should override the "active UMG" for subsequent operations if set.
// 5. All UMG operations (GetWidgetTree, QueryWidgetProperties, etc.) should ideally use this "Operational Target UMG"
//    as their default target if not explicitly provided in the function call.
// This aims for a more robust and controllable "attention" mechanism.

/**
 * An editor subsystem to monitor asset opening events.
 */
UCLASS()
class UMGMCP_API UUmgAttentionSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	// UEditorSubsystem implementation
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/**
	 * Gets the path of the last edited UMG asset.
	 * @return FString The asset path. Returns empty string if history is empty.
	 */
	FString GetLastEditedUMGAsset() const;

	/**
	 * Gets a list of recently edited UMG assets.
	 * @param MaxCount The maximum number of assets to return.
	 * @return TArray<FString> A list of asset paths.
	 */
	TArray<FString> GetRecentlyEditedUMGAssets(int32 MaxCount = 5) const;

	/**
	 * Gets the asset path of the currently active UMG editor.
	 * @return FString The asset path. Returns empty string if no UMG editor is active.
	 */
	FString GetActiveUMGContext() const;

	/**
	 * Checks if a UMG editor is currently active.
	 * @return bool True if a UMG editor is active, false otherwise.
	 */
	bool IsUMGEditorActive() const;

public: // Moved from private
	// Attention Set Functions
	UFUNCTION(BlueprintCallable, Category = "UMG MCP|Attention")
	void SetAttentionTarget(const FString& AssetPath);

private:
	void HandleAssetOpened(UObject* Asset, class IAssetEditorInstance* EditorInstance);

protected: // UmgAssetHistory is now explicitly protected
	TArray<FString> UmgAssetHistory;
};