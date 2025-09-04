#include "UmgAttentionSubsystem.h"
#include "Editor.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "WidgetBlueprint.h"
#include "Logging/LogMacros.h"

// Define a log category for easy debugging.
DEFINE_LOG_CATEGORY_STATIC(LogUmgAttention, Log, All);

void UUmgAttentionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Register the delegate to be called when an asset is opened.
	if (GEditor)
	{
		UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
		if (AssetEditorSubsystem)
		{
			AssetEditorSubsystem->OnAssetOpenedInEditor().AddUObject(this, &UUmgAttentionSubsystem::HandleAssetOpened);
		}
	}

	// Use a Warning level to make sure this log is visible.
	UE_LOG(LogUmgAttention, Warning, TEXT("UmgAttentionSubsystem Initialized and is monitoring assets."));
}

void UUmgAttentionSubsystem::Deinitialize()
{
	// Unregister the delegate.
	if (GEditor)
	{
		UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
		if (AssetEditorSubsystem)
		{
			AssetEditorSubsystem->OnAssetOpenedInEditor().RemoveAll(this);
		}
	}

	UE_LOG(LogUmgAttention, Log, TEXT("UmgAttentionSubsystem Deinitialized."));

	Super::Deinitialize();
}

void UUmgAttentionSubsystem::HandleAssetOpened(UObject* Asset, class IAssetEditorInstance* EditorInstance)
{
	if (!Asset)
	{
		return;
	}

	UE_LOG(LogUmgAttention, Log, TEXT("Asset Opened: %s"), *Asset->GetPathName());

	if (UWidgetBlueprint* WidgetBlueprint = Cast<UWidgetBlueprint>(Asset))
	{
		FString AssetPath = Asset->GetPathName();
		UE_LOG(LogUmgAttention, Log, TEXT("UMG Asset Detected: %s"), *AssetPath);

		// Remove if it exists to avoid duplicates and then add to the front.
		UmgAssetHistory.Remove(AssetPath);
		UmgAssetHistory.Insert(AssetPath, 0);

		// Log the current history.
		UE_LOG(LogUmgAttention, Log, TEXT("--- UmgAttention History ---"));
		for (int32 i = 0; i < UmgAssetHistory.Num(); ++i)
		{
			UE_LOG(LogUmgAttention, Log, TEXT("[%d] %s"), i, *UmgAssetHistory[i]);
		}
		UE_LOG(LogUmgAttention, Log, TEXT("---------------------------"));
	}
}

FString UUmgAttentionSubsystem::GetLastEditedUMGAsset() const
{
    if (UmgAssetHistory.Num() > 0)
    {
        return UmgAssetHistory[0];
    }
    return FString();
}

TArray<FString> UUmgAttentionSubsystem::GetRecentlyEditedUMGAssets(int32 MaxCount) const
{
    TArray<FString> Result;
    int32 Count = FMath::Min(UmgAssetHistory.Num(), MaxCount);
    for (int32 i = 0; i < Count; ++i)
    {
        Result.Add(UmgAssetHistory[i]);
    }
    return Result;
}
