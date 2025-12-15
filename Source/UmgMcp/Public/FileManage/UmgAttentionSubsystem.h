#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "UmgAttentionSubsystem.generated.h"

class UWidgetBlueprint;

/**
 * @brief Manages the "attention" or context for AI-driven UMG operations.
 *
 * This subsystem addresses the ambiguity of user commands by tracking which UMG asset
 * is the most relevant target. It maintains a history of recently edited assets and allows
 * for an explicit "attention target" to be set. When a command is issued without a specific
 * target asset (e.g., "change the button color"), other systems can query this subsystem
 * to determine which UMG asset the user is likely working on.
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
	 * Gets the asset path of the current target UMG asset.
	 * If a target is explicitly set via SetTargetUmgAsset, it returns that target.
	 * Otherwise, it falls back to returning the last edited UMG asset.
	 * @return FString The asset path of the target UMG asset.
	 */
	FString GetTargetUmgAsset() const;

	/**
	 * Explicitly sets the UMG asset to be the target for subsequent operations.
     * This updates both the path cache for conversation and the object cache for performance.
	 * @param AssetPath The path of the asset to set as the target.
     * @return bool True if the asset was successfully found or created, False if invalid path or error.
	 */
	bool SetTargetUmgAsset(const FString& AssetPath);

    /**
     * Gets the cached UWidgetBlueprint object.
     * @return A pointer to the UWidgetBlueprint, or nullptr if not valid or not set.
     */
    UWidgetBlueprint* GetCachedTargetWidgetBlueprint() const;

	/**
	 * Sets the name of the animation currently being focused on.
	 * @param AnimationName The name of the animation.
	 */
	void SetTargetAnimation(const FString& AnimationName);

	/**
	 * Gets the name of the currently focused animation.
	 * @return FString The animation name.
	 */
	FString GetTargetAnimation() const;

	/**
	 * Sets the name of the widget currently being focused on.
	 * @param WidgetName The name of the widget.
	 */
	void SetTargetWidget(const FString& WidgetName);

	/**
	 * Gets the name of the currently focused widget.
	 * @return FString The widget name.
	 */
	FString GetTargetWidget() const;

private:
	void HandleAssetOpened(UObject* Asset, class IAssetEditorInstance* EditorInstance);

	// The asset path of the UMG widget blueprint that is the current focus of attention (for conversation context).
	FString AttentionTargetAssetPath;

    // A weak pointer to the actual loaded UMG asset (for performance).
    TWeakObjectPtr<UWidgetBlueprint> CachedTargetWidgetBlueprint;

	// The name of the animation currently in focus.
	FString CurrentAnimationName;

	// The name of the widget currently in focus.
	FString CurrentWidgetName;

protected:
	TArray<FString> UmgAssetHistory;
};