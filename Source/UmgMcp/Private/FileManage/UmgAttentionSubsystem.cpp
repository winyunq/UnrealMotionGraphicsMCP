#include "FileManage/UmgAttentionSubsystem.h"
#include "Editor.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "WidgetBlueprint.h"
#include "Logging/LogMacros.h"

// Define a log category for easy debugging.
DEFINE_LOG_CATEGORY_STATIC(LogUmgAttention, Log, All);

void UUmgAttentionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
    CachedTargetWidgetBlueprint = nullptr;

	// Register the delegate to be called when an asset is opened.
	if (GEditor)
	{
		UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
		if (AssetEditorSubsystem)
		{
			AssetEditorSubsystem->OnAssetOpenedInEditor().AddUObject(this, &UUmgAttentionSubsystem::HandleAssetOpened);
		}
	}

	UE_LOG(LogUmgAttention, Log, TEXT("UmgAttentionSubsystem Initialized."));
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

	if (UWidgetBlueprint* WidgetBP = Cast<UWidgetBlueprint>(Asset))
	{
		FString AssetPath = WidgetBP->GetPathName();
		UE_LOG(LogUmgAttention, Log, TEXT("UMG Asset Opened: %s"), *AssetPath);

		// Update history
		UmgAssetHistory.Remove(AssetPath);
		UmgAssetHistory.Insert(AssetPath, 0);

        // If the opened asset is our current target, update the cached object pointer for free.
        if (AssetPath == AttentionTargetAssetPath)
        {
            UE_LOG(LogUmgAttention, Log, TEXT("Opened asset matches current attention target. Updating cached object."));
            CachedTargetWidgetBlueprint = WidgetBP;
        }
	}
}

void UUmgAttentionSubsystem::SetTargetUmgAsset(const FString& AssetPath)
{
    if (AssetPath.IsEmpty())
    {
        UE_LOG(LogUmgAttention, Warning, TEXT("SetTargetUmgAsset called with empty AssetPath. Clearing target."));
        AttentionTargetAssetPath.Empty();
        CachedTargetWidgetBlueprint = nullptr;
        return;
    }

    // Attempt to load the object from the provided path.
    UWidgetBlueprint* LoadedBP = LoadObject<UWidgetBlueprint>(nullptr, *AssetPath);

    if (LoadedBP)
    {
        // SUCCESS: The path is valid and the object was loaded.
        UE_LOG(LogUmgAttention, Log, TEXT("Setting Attention Target Path to: %s"), *AssetPath);
        AttentionTargetAssetPath = AssetPath;
        CachedTargetWidgetBlueprint = LoadedBP;

        // Also treat setting a target as an 'edit' action to update the history, ensuring consistency.
        UmgAssetHistory.Remove(AssetPath);
        UmgAssetHistory.Insert(AssetPath, 0);
        UE_LOG(LogUmgAttention, Log, TEXT("Successfully loaded and cached UMG asset object."));
    }
    else
    {
        // FAILURE: The path was invalid. Clear any existing target to prevent errors.
        UE_LOG(LogUmgAttention, Warning, TEXT("Failed to load UMG asset from path: %s. Clearing attention target."), *AssetPath);
        AttentionTargetAssetPath.Empty();
        CachedTargetWidgetBlueprint = nullptr;
    }
}

FString UUmgAttentionSubsystem::GetTargetUmgAsset() const
{
    // Non-const mutable copy for potential lazy-loading
    UUmgAttentionSubsystem* MutableThis = const_cast<UUmgAttentionSubsystem*>(this);

    // If we have an explicit target path, but our cached object is invalid, try to reload it.
    if (!AttentionTargetAssetPath.IsEmpty() && !CachedTargetWidgetBlueprint.IsValid())
    {
        UE_LOG(LogUmgAttention, Log, TEXT("Cached target object is invalid. Attempting to reload from path: %s"), *AttentionTargetAssetPath);
        UWidgetBlueprint* ReloadedBP = LoadObject<UWidgetBlueprint>(nullptr, *AttentionTargetAssetPath);
        if (ReloadedBP)
        {
            UE_LOG(LogUmgAttention, Log, TEXT("Successfully reloaded and re-cached UMG asset object."));
            MutableThis->CachedTargetWidgetBlueprint = ReloadedBP;
        }
        else
        {
             UE_LOG(LogUmgAttention, Warning, TEXT("Failed to reload UMG asset object."));
        }
    }

    if (!AttentionTargetAssetPath.IsEmpty())
    {
        return AttentionTargetAssetPath;
    }
    
    // NEW: Check currently open UMG editors
    if (GEditor)
    {
        UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
        if (AssetEditorSubsystem)
        {
            TArray<UObject*> EditedAssets = AssetEditorSubsystem->GetAllEditedAssets();
            for (UObject* Asset : EditedAssets)
            {
                if (UWidgetBlueprint* WidgetBP = Cast<UWidgetBlueprint>(Asset))
                {
                    FString AssetPath = WidgetBP->GetPathName();
                    UE_LOG(LogUmgAttention, Log, TEXT("Found currently open UMG editor: %s"), *AssetPath);
                    
                    // Update our cache
                    MutableThis->CachedTargetWidgetBlueprint = WidgetBP;
                    
                    // Return the first found UMG asset
                    return AssetPath;
                }
            }
        }
    }
    
    // Fallback to history
    return GetLastEditedUMGAsset();
}

UWidgetBlueprint* UUmgAttentionSubsystem::GetCachedTargetWidgetBlueprint() const
{
    // Before returning, call GetTargetUmgAsset() to ensure lazy-load/reload logic is triggered.
    GetTargetUmgAsset(); 
    
    if (CachedTargetWidgetBlueprint.IsValid())
    {
        return CachedTargetWidgetBlueprint.Get();
    }

    // If there's no explicit target, maybe the last edited one is what we want.
    if (AttentionTargetAssetPath.IsEmpty())
    {
        FString LastEditedPath = GetLastEditedUMGAsset();
        if (!LastEditedPath.IsEmpty())
        {
            // Non-const mutable copy for lazy-loading
            UUmgAttentionSubsystem* MutableThis = const_cast<UUmgAttentionSubsystem*>(this);
            UWidgetBlueprint* LoadedBP = LoadObject<UWidgetBlueprint>(nullptr, *LastEditedPath);
            if (LoadedBP)
            {
                UE_LOG(LogUmgAttention, Log, TEXT("No explicit target. Lazy loading last edited asset: %s"), *LastEditedPath);
                MutableThis->CachedTargetWidgetBlueprint = LoadedBP;
                return LoadedBP;
            }
        }
    }

    UE_LOG(LogUmgAttention, Warning, TEXT("Returning nullptr because cached target is not valid and could not be loaded."));
    return nullptr;
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
        // Return the direct asset path, consistent with GetLastEditedUMGAsset
        Result.Add(UmgAssetHistory[i]);
    }
    return Result;
}

void UUmgAttentionSubsystem::SetTargetAnimation(const FString& AnimationName)
{
	CurrentAnimationName = AnimationName;
	UE_LOG(LogUmgAttention, Log, TEXT("Context: Focused Animation set to '%s'"), *CurrentAnimationName);
}

FString UUmgAttentionSubsystem::GetTargetAnimation() const
{
	return CurrentAnimationName;
}

void UUmgAttentionSubsystem::SetTargetWidget(const FString& WidgetName)
{
	CurrentWidgetName = WidgetName;
	UE_LOG(LogUmgAttention, Log, TEXT("Context: Focused Widget set to '%s'"), *CurrentWidgetName);
}

FString UUmgAttentionSubsystem::GetTargetWidget() const
{
	return CurrentWidgetName;
}