#include "UmgGetSubsystem.h"
#include "Logging/LogMacros.h"
#include "Misc/PackageName.h"

DEFINE_LOG_CATEGORY(LogUmgGet);

void UUmgGetSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	UE_LOG(LogUmgGet, Warning, TEXT("UmgGetSubsystem Initialized."));
}

void UUmgGetSubsystem::Deinitialize()
{
	UE_LOG(LogUmgGet, Log, TEXT("UmgGetSubsystem Deinitialized."));
	Super::Deinitialize();
}

FString UUmgGetSubsystem::GetWidgetTree(const FString& AssetPath)
{
    UE_LOG(LogUmgGet, Log, TEXT("GetWidgetTree called for Asset: %s"), *AssetPath);
    return FString(); // Placeholder
}

FString UUmgGetSubsystem::QueryWidgetProperties(const FString& WidgetId, const TArray<FString>& Properties)
{
    UE_LOG(LogUmgGet, Log, TEXT("QueryWidgetProperties called for WidgetId: %s, Properties: %s"), *WidgetId, *FString::Join(Properties, TEXT(", ")));
    return FString(); // Placeholder
}

FString UUmgGetSubsystem::GetLayoutData(const FString& AssetPath, int32 ResolutionWidth, int32 ResolutionHeight)
{
    UE_LOG(LogUmgGet, Log, TEXT("GetLayoutData called for Asset: %s, Resolution: %dx%d"), *AssetPath, ResolutionWidth, ResolutionHeight);
    return FString(); // Placeholder
}

bool UUmgGetSubsystem::CheckWidgetOverlap(const FString& AssetPath, const TArray<FString>& WidgetIds)
{
    UE_LOG(LogUmgGet, Log, TEXT("CheckWidgetOverlap called for Asset: %s, WidgetIds: %s"), *AssetPath, *FString::Join(WidgetIds, TEXT(", ")));
    return false; // Placeholder
}

FString UUmgGetSubsystem::GetAssetFileSystemPath(const FString& AssetPath)
{
    if (AssetPath.IsEmpty())
    {
        UE_LOG(LogUmgGet, Warning, TEXT("GetAssetFileSystemPath called with empty AssetPath."));
        return FString();
    }

    FString FileSystemPath;
    bool bSuccess = FPackageName::TryConvertLongPackageNameToFilename(AssetPath, FileSystemPath);
    if (bSuccess)
    {
        UE_LOG(LogUmgGet, Log, TEXT("Converted AssetPath '%s' to FileSystemPath '%s'"), *AssetPath, *FileSystemPath);
        return FileSystemPath;
    }
    else
    {
        UE_LOG(LogUmgGet, Error, TEXT("Failed to convert AssetPath '%s' to FileSystemPath."), *AssetPath);
        return FString();
    }
}
