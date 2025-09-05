#include "UmgSetSubsystem.h"
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetTree.h"
#include "JsonObjectConverter.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Kismet2/BlueprintEditorUtils.h"

DEFINE_LOG_CATEGORY(LogUmgSet);

void UUmgSetSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    UE_LOG(LogUmgSet, Warning, TEXT("UmgSetSubsystem Initialized."));
}

void UUmgSetSubsystem::Deinitialize()
{
    UE_LOG(LogUmgSet, Log, TEXT("UmgSetSubsystem Deinitialized."));
    Super::Deinitialize();
}

bool UUmgSetSubsystem::SetWidgetProperties(const FString& WidgetId, const FString& PropertiesJson)
{
    FString AssetPath;
    FString WidgetName;
    if (!WidgetId.Split(TEXT(":"), &AssetPath, &WidgetName, ESearchCase::IgnoreCase, ESearchDir::FromEnd))
    {
        UE_LOG(LogUmgSet, Error, TEXT("SetWidgetProperties: Invalid WidgetId format. Expected AssetPath:WidgetName. Received: %s"), *WidgetId);
        return false;
    }

    UWidgetBlueprint* WidgetBlueprint = LoadObject<UWidgetBlueprint>(nullptr, *AssetPath);
    if (!WidgetBlueprint || !WidgetBlueprint->WidgetTree)
    {
        UE_LOG(LogUmgSet, Error, TEXT("SetWidgetProperties: Failed to load WidgetBlueprint or its WidgetTree for AssetPath: %s"), *AssetPath);
        return false;
    }

    UWidget* FoundWidget = WidgetBlueprint->WidgetTree->FindWidget(FName(*WidgetName));
    if (!FoundWidget)
    {
        UE_LOG(LogUmgSet, Error, TEXT("SetWidgetProperties: Failed to find widget '%s' in asset '%s'."), *WidgetName, *AssetPath);
        return false;
    }

    TSharedPtr<FJsonObject> PropertiesJsonObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(PropertiesJson);
    if (!FJsonSerializer::Deserialize(Reader, PropertiesJsonObject) || !PropertiesJsonObject.IsValid())
    {
        UE_LOG(LogUmgSet, Error, TEXT("SetWidgetProperties: Failed to parse PropertiesJson string."));
        return false;
    }

    WidgetBlueprint->Modify();
    FoundWidget->Modify();

    for (const auto& Pair : PropertiesJsonObject->Values)
    {
        FProperty* Property = FindFProperty<FProperty>(FoundWidget->GetClass(), FName(*Pair.Key));
        if (Property)
        {
            FJsonObjectConverter::JsonValueToUProperty(Pair.Value, Property, FoundWidget, 0, 0);
        }
    }

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBlueprint);

    return true;
}

#include "Components/PanelWidget.h"

FString UUmgSetSubsystem::CreateWidget(const FString& AssetPath, const FString& ParentId, const FString& WidgetType, const FString& WidgetName)
{
    UWidgetBlueprint* WidgetBlueprint = LoadObject<UWidgetBlueprint>(nullptr, *AssetPath);
    if (!WidgetBlueprint || !WidgetBlueprint->WidgetTree)
    {
        UE_LOG(LogUmgSet, Error, TEXT("CreateWidget: Failed to load WidgetBlueprint or its WidgetTree for AssetPath: %s"), *AssetPath);
        return FString();
    }

    UPanelWidget* ParentWidget = Cast<UPanelWidget>(WidgetBlueprint->WidgetTree->FindWidget(FName(*ParentId)));
    if (!ParentWidget)
    {
        UE_LOG(LogUmgSet, Error, TEXT("CreateWidget: Failed to find ParentWidget with name '%s' in asset '%s'."), *ParentId, *AssetPath);
        return FString();
    }

    UClass* WidgetClass = LoadObject<UClass>(nullptr, *WidgetType);
    if (!WidgetClass)
    {
        UE_LOG(LogUmgSet, Error, TEXT("CreateWidget: Failed to find widget class '%s'."), *WidgetType);
        return FString();
    }

    WidgetBlueprint->Modify();

    UWidget* NewWidget = NewObject<UWidget>(WidgetBlueprint->WidgetTree, WidgetClass, FName(*WidgetName));
    if (!NewWidget)
    {
        UE_LOG(LogUmgSet, Error, TEXT("CreateWidget: Failed to create widget of class '%s'."), *WidgetType);
        return FString();
    }

    ParentWidget->AddChild(NewWidget);

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBlueprint);

    return NewWidget->GetPathName();
}

bool UUmgSetSubsystem::DeleteWidget(const FString& WidgetId)
{
    FString AssetPath;
    FString WidgetName;
    if (!WidgetId.Split(TEXT(":"), &AssetPath, &WidgetName, ESearchCase::IgnoreCase, ESearchDir::FromEnd))
    {
        UE_LOG(LogUmgSet, Error, TEXT("DeleteWidget: Invalid WidgetId format. Expected AssetPath:WidgetName. Received: %s"), *WidgetId);
        return false;
    }

    UWidgetBlueprint* WidgetBlueprint = LoadObject<UWidgetBlueprint>(nullptr, *AssetPath);
    if (!WidgetBlueprint || !WidgetBlueprint->WidgetTree)
    {
        UE_LOG(LogUmgSet, Error, TEXT("DeleteWidget: Failed to load WidgetBlueprint or its WidgetTree for AssetPath: %s"), *AssetPath);
        return false;
    }

    UWidget* FoundWidget = WidgetBlueprint->WidgetTree->FindWidget(FName(*WidgetName));
    if (!FoundWidget)
    {
        UE_LOG(LogUmgSet, Error, TEXT("DeleteWidget: Failed to find widget '%s' in asset '%s'."), *WidgetName, *AssetPath);
        return false;
    }

    WidgetBlueprint->Modify();

    if (WidgetBlueprint->WidgetTree->RemoveWidget(FoundWidget))
    {
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBlueprint);
        return true;
    }

    return false;
}

bool UUmgSetSubsystem::ReparentWidget(const FString& WidgetId, const FString& NewParentId)
{
    FString AssetPath;
    FString WidgetName;
    if (!WidgetId.Split(TEXT(":"), &AssetPath, &WidgetName, ESearchCase::IgnoreCase, ESearchDir::FromEnd))
    {
        UE_LOG(LogUmgSet, Error, TEXT("ReparentWidget: Invalid WidgetId format. Received: %s"), *WidgetId);
        return false;
    }

    FString NewParentAssetPath;
    FString NewParentName;
    if (!NewParentId.Split(TEXT(":"), &NewParentAssetPath, &NewParentName, ESearchCase::IgnoreCase, ESearchDir::FromEnd))
    {
        UE_LOG(LogUmgSet, Error, TEXT("ReparentWidget: Invalid NewParentId format. Received: %s"), *NewParentId);
        return false;
    }

    // For now, we only support reparenting within the same asset
    if (AssetPath != NewParentAssetPath)
    {
        UE_LOG(LogUmgSet, Error, TEXT("ReparentWidget: Cross-asset reparenting is not supported."));
        return false;
    }

    UWidgetBlueprint* WidgetBlueprint = LoadObject<UWidgetBlueprint>(nullptr, *AssetPath);
    if (!WidgetBlueprint || !WidgetBlueprint->WidgetTree)
    {
        UE_LOG(LogUmgSet, Error, TEXT("ReparentWidget: Failed to load WidgetBlueprint or its WidgetTree for AssetPath: %s"), *AssetPath);
        return false;
    }

    UWidget* WidgetToMove = WidgetBlueprint->WidgetTree->FindWidget(FName(*WidgetName));
    if (!WidgetToMove)
    {
        UE_LOG(LogUmgSet, Error, TEXT("ReparentWidget: Failed to find widget to move: '%s'"), *WidgetName);
        return false;
    }

    UPanelWidget* NewParentWidget = Cast<UPanelWidget>(WidgetBlueprint->WidgetTree->FindWidget(FName(*NewParentName)));
    if (!NewParentWidget)
    {
        UE_LOG(LogUmgSet, Error, TEXT("ReparentWidget: Failed to find new parent widget: '%s'"), *NewParentName);
        return false;
    }

    WidgetBlueprint->Modify();

    if (WidgetBlueprint->WidgetTree->RemoveWidget(WidgetToMove))
    {
        NewParentWidget->AddChild(WidgetToMove);
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBlueprint);
        return true;
    }

    return false;
}
