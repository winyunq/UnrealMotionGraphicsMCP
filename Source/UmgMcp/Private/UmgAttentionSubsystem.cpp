#include "UmgAttentionSubsystem.h"
#include "Editor.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "WidgetBlueprint.h"
#include "Logging/LogMacros.h"
#include "WidgetBlueprintEditor.h" // For FWidgetBlueprintEditor
#include "Framework/Docking/TabManager.h" // For FGlobalTabmanager
#include "Misc/PackageName.h" // Add this line

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

void UUmgAttentionSubsystem::SetAttentionTarget(const FString& AssetPath)
{
    if (AssetPath.IsEmpty())
    {
        UE_LOG(LogUmgAttention, Warning, TEXT("SetAttentionTarget called with empty AssetPath. Ignoring."));
        return;
    }

    UE_LOG(LogUmgAttention, Log, TEXT("SetAttentionTarget called for Asset: %s"), *AssetPath);

    // Remove if it exists to avoid duplicates and then add to the front.
    UmgAssetHistory.Remove(AssetPath);
    UmgAssetHistory.Insert(AssetPath, 0);

    // Log the current history.
    UE_LOG(LogUmgAttention, Log, TEXT("--- UmgAttention History (after SetAttentionTarget) ---"));
    for (int32 i = 0; i < UmgAssetHistory.Num(); ++i)
    {
        UE_LOG(LogUmgAttention, Log, TEXT("[%d] %s"), i, *UmgAssetHistory[i]);
    }
    UE_LOG(LogUmgAttention, Log, TEXT("----------------------------------------------------"));
}

FString UUmgAttentionSubsystem::GetLastEditedUMGAsset() const
{
    if (UmgAssetHistory.Num() > 0)
    {
        FString AssetPath = UmgAssetHistory[0];
        FString FileSystemPath = FPackageName::LongPackageNameToFilename(AssetPath, TEXT("")); // Pass empty extension
        if (!FileSystemPath.IsEmpty()) // Check if conversion was successful
        {
            return FileSystemPath;
        }
        UE_LOG(LogUmgAttention, Warning, TEXT("Failed to convert asset path to file system path for: %s"), *AssetPath);
        return FString(); // Return empty string on failure
    }
    return FString();
}

TArray<FString> UUmgAttentionSubsystem::GetRecentlyEditedUMGAssets(int32 MaxCount) const
{
    TArray<FString> Result;
    int32 Count = FMath::Min(UmgAssetHistory.Num(), MaxCount);
    for (int32 i = 0; i < Count; ++i)
    {
        FString AssetPath = UmgAssetHistory[i];
        FString FileSystemPath = FPackageName::LongPackageNameToFilename(AssetPath, TEXT(".uasset")); // Pass .uasset extension
        if (!FileSystemPath.IsEmpty())
        {
            Result.Add(FileSystemPath);
        }
        else
        {
            UE_LOG(LogUmgAttention, Warning, TEXT("Failed to convert asset path to file system path for recently edited asset: %s"), *AssetPath);
        }
    }
    return Result;
}

FString UUmgAttentionSubsystem::GetActiveUMGContext() const
{
    if (!GEditor)
    {
        return FString();
    }

    UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
    if (!AssetEditorSubsystem)
    {
        return FString();
    }

    // Get all currently open assets
    TArray<UObject*> EditedAssets = AssetEditorSubsystem->GetAllEditedAssets();

    // Get the currently active tab in the editor
    TSharedPtr<SDockTab> ActiveTab = FGlobalTabmanager::Get()->GetActiveTab();

    if (!ActiveTab.IsValid())
    {
        return FString();
    }

    for (UObject* EditedAsset : EditedAssets)
    {
        if (EditedAsset)
        {
            // Check if the edited asset is a Widget Blueprint
            UWidgetBlueprint* WidgetBlueprint = Cast<UWidgetBlueprint>(EditedAsset);
            if (WidgetBlueprint)
            {
                // Find the editor instance for this Widget Blueprint
                IAssetEditorInstance* EditorInstance = AssetEditorSubsystem->FindEditorForAsset(WidgetBlueprint, false);
                if (EditorInstance)
                {
                    // Get the TabManager associated with this editor instance
                    TSharedPtr<FTabManager> EditorTabManager = EditorInstance->GetAssociatedTabManager();
                    if (EditorTabManager.IsValid())
                    {
                        // Get the owner tab of this editor's TabManager
                        TSharedPtr<SDockTab> EditorOwnerTab = EditorTabManager->GetOwnerTab();
                        
                        // Check if this editor's owner tab is the currently active tab
                        if (EditorOwnerTab.IsValid() && EditorOwnerTab == ActiveTab)
                        {
                            FString AssetPath = WidgetBlueprint->GetPathName();
                            FString FileSystemPath = FPackageName::LongPackageNameToFilename(AssetPath, TEXT("")); // Pass empty extension
                            if (!FileSystemPath.IsEmpty()) // Check if conversion was successful
                            {
                                return FileSystemPath;
                            }
                            UE_LOG(LogUmgAttention, Warning, TEXT("Failed to convert asset path to file system path for active UMG context: %s"), *AssetPath);
                            return FString(); // Return empty string on failure
                        }
                    }
                }
            }
        }
    }

    return GetLastEditedUMGAsset();
}

bool UUmgAttentionSubsystem::IsUMGEditorActive() const
{
    return !GetActiveUMGContext().IsEmpty();
}