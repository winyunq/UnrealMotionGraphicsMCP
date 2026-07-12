// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#include "FileManage/UmgAttentionSubsystem.h"
#include "Editor.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "WidgetBlueprint.h"
#include "Engine/Blueprint.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Widget.h"
#include "Logging/LogMacros.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "WidgetBlueprintFactory.h"
#include "Misc/PackageName.h"

// Define a log category for easy debugging.
DEFINE_LOG_CATEGORY_STATIC(LogUmgAttention, Log, All);

namespace
{
bool ResolveUmgAssetPath(const FString& InAssetPath, FString& OutCleanPath, FString& OutPackageName, FString& OutObjectPath, FString& OutError)
{
    OutCleanPath = InAssetPath.TrimStartAndEnd();
    OutCleanPath.ReplaceInline(TEXT("\\"), TEXT("/"));

    if (OutCleanPath.EndsWith(FPackageName::GetAssetPackageExtension()))
    {
        OutCleanPath.LeftChopInline(FPackageName::GetAssetPackageExtension().Len());
    }

    if (OutCleanPath.IsEmpty() || !OutCleanPath.StartsWith(TEXT("/")))
    {
        OutError = FString::Printf(TEXT("Invalid AssetPath '%s'. Must start with '/'."), *InAssetPath);
        return false;
    }

    OutPackageName = FPackageName::ObjectPathToPackageName(OutCleanPath);
    if (OutPackageName.IsEmpty())
    {
        OutPackageName = OutCleanPath;
    }

    FText FailureReason;
    if (!FPackageName::IsValidLongPackageName(OutPackageName, true, &FailureReason))
    {
        OutError = FString::Printf(TEXT("Invalid package path '%s': %s"), *OutPackageName, *FailureReason.ToString());
        return false;
    }

    const FString AssetName = FPackageName::GetLongPackageAssetName(OutPackageName);
    if (AssetName.IsEmpty())
    {
        OutError = FString::Printf(TEXT("Invalid package path '%s': missing asset name."), *OutPackageName);
        return false;
    }

    OutObjectPath = OutCleanPath.Contains(TEXT("."))
        ? OutCleanPath
        : FString::Printf(TEXT("%s.%s"), *OutPackageName, *AssetName);

    return true;
}

bool CanAutoCreateUmgAsset(const FString& PackageName)
{
    return PackageName.StartsWith(TEXT("/Game/"));
}

bool IsSameAssetPackagePath(const FString& LeftPath, const FString& RightPath)
{
    FString LeftCleanPath;
    FString LeftPackageName;
    FString LeftObjectPath;
    FString LeftError;
    FString RightCleanPath;
    FString RightPackageName;
    FString RightObjectPath;
    FString RightError;

    if (!ResolveUmgAssetPath(LeftPath, LeftCleanPath, LeftPackageName, LeftObjectPath, LeftError) ||
        !ResolveUmgAssetPath(RightPath, RightCleanPath, RightPackageName, RightObjectPath, RightError))
    {
        return LeftPath.Equals(RightPath, ESearchCase::IgnoreCase);
    }

    return LeftPackageName.Equals(RightPackageName, ESearchCase::IgnoreCase);
}
}

void UUmgAttentionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	CachedTargetBlueprint = nullptr;

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

	if (UBlueprint* Blueprint = Cast<UBlueprint>(Asset))
	{
		FString AssetPath = Blueprint->GetPathName();
		UE_LOG(LogUmgAttention, Log, TEXT("Blueprint Asset Opened: %s"), *AssetPath);

		// Update history
		UmgAssetHistory.Remove(AssetPath);
		UmgAssetHistory.Insert(AssetPath, 0);

        // If the opened asset is our current target, update the cached object pointer for free.
        if (AssetPath == AttentionTargetAssetPath)
        {
            UE_LOG(LogUmgAttention, Log, TEXT("Opened asset matches current attention target. Updating cached object."));
            CachedTargetBlueprint = Blueprint;
        }
	}
}

bool UUmgAttentionSubsystem::SetTargetUmgAsset(const FString& AssetPath)
{
    return SetTargetBlueprintAssetInternal(AssetPath, true);
}

bool UUmgAttentionSubsystem::SetTargetBlueprintAsset(const FString& AssetPath)
{
    return SetTargetBlueprintAssetInternal(AssetPath, false);
}

bool UUmgAttentionSubsystem::SetTargetBlueprintAssetInternal(const FString& AssetPath, bool bAllowCreateWidget)
{
    FString CleanAssetPath;
    FString PackageName;
    FString ObjectPath;
    FString PathError;
    if (!ResolveUmgAssetPath(AssetPath, CleanAssetPath, PackageName, ObjectPath, PathError))
    {
        UE_LOG(LogUmgAttention, Warning, TEXT("SetTargetUmgAsset: %s Clearing target."), *PathError);
        AttentionTargetAssetPath.Empty();
        CachedTargetBlueprint = nullptr;
        return false;
    }

    UBlueprint* TargetBP = nullptr;

    // 1. Try to find the asset in an open editor FIRST
    if (GEditor)
    {
        if (UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>())
        {
             FSoftObjectPath SoftObjectPath(ObjectPath);
             UObject* AssetObj = SoftObjectPath.ResolveObject();
             
             if (AssetObj)
             {
                 IAssetEditorInstance* Editor = AssetEditorSubsystem->FindEditorForAsset(AssetObj, false);
                 if (Editor)
                 {
                     TargetBP = Cast<UBlueprint>(AssetObj);
                     if (TargetBP)
                     {
                         UE_LOG(LogUmgAttention, Log, TEXT("SetTargetUmgAsset: Found open editor for %s. Using live instance."), *ObjectPath);
                     }
                 }
             }
        }
    }

    // 2. Fallback to LoadObject if not found in editor
    if (!TargetBP)
    {
        // Use LoadObject but handle failure gracefully without blocking ensure
        TargetBP = LoadObject<UBlueprint>(nullptr, *ObjectPath, nullptr, LOAD_NoWarn);
    }

    // 3. Conditional Creation
    // Only create if it looks like a valid path but just missing
    if (!TargetBP && bAllowCreateWidget)
    {
        if (!CanAutoCreateUmgAsset(PackageName))
        {
            UE_LOG(LogUmgAttention, Warning, TEXT("SetTargetUmgAsset: Asset '%s' was not found. Automatic creation is only supported under /Game; refusing to create under package root '%s'."), *ObjectPath, *PackageName);
        }
        else
        {
            UE_LOG(LogUmgAttention, Log, TEXT("SetTargetUmgAsset: Asset '%s' not found. Creating new WidgetBlueprint..."), *ObjectPath);
            
            FString AssetName = FPackageName::GetLongPackageAssetName(PackageName);
            FString PackagePath = FPaths::GetPath(PackageName);

            // Ensure AssetTools module is loaded
            IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
            
            // Create a factory for WidgetBlueprint
            UWidgetBlueprintFactory* Factory = NewObject<UWidgetBlueprintFactory>();
            
            UObject* NewAsset = AssetTools.CreateAsset(AssetName, PackagePath, UWidgetBlueprint::StaticClass(), Factory);
            TargetBP = Cast<UBlueprint>(NewAsset);
            
            if (TargetBP)
            {
                 UE_LOG(LogUmgAttention, Log, TEXT("SetTargetUmgAsset: Successfully created new asset '%s'."), *ObjectPath);
            }
            else
            {
                 UE_LOG(LogUmgAttention, Error, TEXT("SetTargetUmgAsset: Failed to create asset '%s'."), *ObjectPath);
            }
        }
    }

    if (TargetBP)
    {
        // SUCCESS: The path is valid and the object was loaded/found.
        UE_LOG(LogUmgAttention, Log, TEXT("Setting Attention Target Path to: %s"), *CleanAssetPath);
        AttentionTargetAssetPath = CleanAssetPath;
        CachedTargetBlueprint = TargetBP;
        CurrentWidgetName.Empty(); // Clear stale widget scope when switching assets unless explicitly set later.

        // Also treat setting a target as an 'edit' action to update the history, ensuring consistency.
        UmgAssetHistory.Remove(CleanAssetPath);
        UmgAssetHistory.Insert(CleanAssetPath, 0);
        UE_LOG(LogUmgAttention, Log, TEXT("Successfully cached Blueprint asset object (%s)."), *TargetBP->GetClass()->GetName());
        return true;
    }
    else
    {
        // FAILURE: The path was invalid. Clear any existing target to prevent errors.
        UE_LOG(LogUmgAttention, Warning, TEXT("Failed to load or create UMG asset from path: %s. Clearing attention target."), *CleanAssetPath);
        AttentionTargetAssetPath.Empty();
        CachedTargetBlueprint = nullptr;
        return false;
    }
}

FString UUmgAttentionSubsystem::GetTargetUmgAsset() const
{
    // Non-const mutable copy for potential lazy-loading
    UUmgAttentionSubsystem* MutableThis = const_cast<UUmgAttentionSubsystem*>(this);

    // If we have an explicit target path, but our cached object is invalid, try to reload it.
    if (!AttentionTargetAssetPath.IsEmpty() && !CachedTargetBlueprint.IsValid())
    {
        UE_LOG(LogUmgAttention, Log, TEXT("Cached target object is invalid. Attempting to reload from path: %s"), *AttentionTargetAssetPath);
        FString CleanAssetPath;
        FString PackageName;
        FString ObjectPath;
        FString PathError;
        UBlueprint* ReloadedBP = nullptr;
        if (ResolveUmgAssetPath(AttentionTargetAssetPath, CleanAssetPath, PackageName, ObjectPath, PathError))
        {
            ReloadedBP = LoadObject<UBlueprint>(nullptr, *ObjectPath, nullptr, LOAD_NoWarn);
        }
        else
        {
            UE_LOG(LogUmgAttention, Warning, TEXT("Failed to normalize cached target path: %s"), *PathError);
        }
        if (ReloadedBP)
        {
            UE_LOG(LogUmgAttention, Log, TEXT("Successfully reloaded and re-cached UMG asset object."));
            MutableThis->CachedTargetBlueprint = ReloadedBP;
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
                    MutableThis->CachedTargetBlueprint = WidgetBP;
                    
                    // Return the first found UMG asset
                    return AssetPath;
                }
            }
        }
    }
    
    // Fallback to history
    FString LastEdited = GetLastEditedUMGAsset();
    if (!LastEdited.IsEmpty())
    {
        return LastEdited;
    }

    // Auto-create default asset if nothing exists
    FString DefaultPath = TEXT("/Game/DefaultAndCanBeDelete.DefaultAndCanBeDelete");
    UE_LOG(LogUmgAttention, Log, TEXT("GetTargetUmgAsset: No active or historical UMG target found. Auto-generating default workspace: %s"), *DefaultPath);
    MutableThis->SetTargetUmgAsset(DefaultPath);
    return DefaultPath;
}

UBlueprint* UUmgAttentionSubsystem::GetCachedTargetBlueprint() const
{
    // Before returning, call GetTargetUmgAsset() to ensure lazy-load/reload logic is triggered.
    GetTargetUmgAsset(); 
    
    if (CachedTargetBlueprint.IsValid())
    {
        UBlueprint* CachedBP = CachedTargetBlueprint.Get();
        if (AttentionTargetAssetPath.IsEmpty() || IsSameAssetPackagePath(AttentionTargetAssetPath, CachedBP->GetPathName()))
        {
            return CachedBP;
        }

        UE_LOG(LogUmgAttention, Warning, TEXT("Cached target object '%s' does not match attention target '%s'. Discarding stale cache."),
            *CachedBP->GetPathName(),
            *AttentionTargetAssetPath);
        UUmgAttentionSubsystem* MutableThis = const_cast<UUmgAttentionSubsystem*>(this);
        MutableThis->CachedTargetBlueprint = nullptr;
    }

    // If there's no explicit target, maybe the last edited one is what we want.
    if (AttentionTargetAssetPath.IsEmpty())
    {
        FString LastEditedPath = GetLastEditedUMGAsset();
        if (!LastEditedPath.IsEmpty())
        {
            // Non-const mutable copy for lazy-loading
            UUmgAttentionSubsystem* MutableThis = const_cast<UUmgAttentionSubsystem*>(this);
            UBlueprint* LoadedBP = LoadObject<UBlueprint>(nullptr, *LastEditedPath);
            if (LoadedBP)
            {
                UE_LOG(LogUmgAttention, Log, TEXT("No explicit target. Lazy loading last edited asset: %s"), *LastEditedPath);
                MutableThis->CachedTargetBlueprint = LoadedBP;
                return LoadedBP;
            }
        }
    }

    UE_LOG(LogUmgAttention, Warning, TEXT("Returning nullptr because cached target is not valid and could not be loaded."));
    return nullptr;
}

UWidgetBlueprint* UUmgAttentionSubsystem::GetCachedTargetWidgetBlueprint() const
{
    return Cast<UWidgetBlueprint>(GetCachedTargetBlueprint());
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

bool UUmgAttentionSubsystem::SetTargetWidget(const FString& WidgetName)
{
    FString CleanWidgetName = WidgetName.TrimStartAndEnd();
    if (CleanWidgetName.IsEmpty() || CleanWidgetName.Equals(TEXT("Root"), ESearchCase::IgnoreCase))
    {
        CurrentWidgetName.Empty();
        UE_LOG(LogUmgAttention, Log, TEXT("Context: Focused Widget cleared; root scope will be used."));
        return true;
    }

    UWidgetBlueprint* WidgetBP = GetCachedTargetWidgetBlueprint();
    if (!WidgetBP || !WidgetBP->WidgetTree)
    {
        CurrentWidgetName.Empty();
        UE_LOG(LogUmgAttention, Warning, TEXT("SetTargetWidget: Cannot focus '%s' because no valid target widget blueprint is loaded."), *CleanWidgetName);
        return false;
    }

    if (!WidgetBP->WidgetTree->FindWidget(FName(*CleanWidgetName)))
    {
        CurrentWidgetName.Empty();
        UE_LOG(LogUmgAttention, Warning, TEXT("SetTargetWidget: Widget '%s' was not found in target asset '%s'. Focus cleared."),
            *CleanWidgetName,
            *WidgetBP->GetPathName());
        return false;
    }

	CurrentWidgetName = CleanWidgetName;
	UE_LOG(LogUmgAttention, Log, TEXT("Context: Focused Widget set to '%s' in '%s'"), *CurrentWidgetName, *WidgetBP->GetPathName());
    return true;
}

FString UUmgAttentionSubsystem::GetTargetWidget() const
{
	UWidgetBlueprint* WidgetBP = GetCachedTargetWidgetBlueprint();

	// If a focused widget is set, check if it still exists in the tree (has not been deleted)
	if (!CurrentWidgetName.IsEmpty() && WidgetBP && WidgetBP->WidgetTree)
	{
		if (WidgetBP->WidgetTree->FindWidget(FName(*CurrentWidgetName)))
		{
			return CurrentWidgetName;
		}
	}

	// Fallback to the RootWidget if unassigned or deleted
	if (WidgetBP && WidgetBP->WidgetTree && WidgetBP->WidgetTree->RootWidget)
	{
		return WidgetBP->WidgetTree->RootWidget->GetName();
	}

	return TEXT("Root");
}

void UUmgAttentionSubsystem::SetTargetGraph(const FString& GraphName)
{
    CurrentGraphName = GraphName;
    // Reset cursor when switching graphs, or maybe try to find the "End" of the graph?
    // For now, simpler is safer: reset to empty so we don't link across graphs.
    LastEditedNodeId.Empty(); 
    CurrentNodePosition = FVector2D(0, 0); 
    UE_LOG(LogUmgAttention, Log, TEXT("Context: Focused Graph set to '%s'"), *CurrentGraphName);
}

FString UUmgAttentionSubsystem::GetTargetGraph() const
{
    // If empty, default to "EventGraph" as a sensible default for UMG
    if (CurrentGraphName.IsEmpty())
    {
        return TEXT("EventGraph");
    }
    return CurrentGraphName;
}

void UUmgAttentionSubsystem::SetCursorNode(const FString& NodeId)
{
    LastEditedNodeId = NodeId;
    UE_LOG(LogUmgAttention, Log, TEXT("Context: Cursor Node set to '%s'"), *LastEditedNodeId);
}

FString UUmgAttentionSubsystem::GetCursorNode() const
{
    return LastEditedNodeId;
}

FVector2D UUmgAttentionSubsystem::GetAndAdvanceCursorPosition()
{
    FVector2D Result = CurrentNodePosition;
    // Simple auto-layout strategy: move right by 250 units
    CurrentNodePosition.X += 250.0f;
    return Result;
}

void UUmgAttentionSubsystem::SetCursorPosition(const FVector2D& NewPosition)
{
    CurrentNodePosition = NewPosition;
}
