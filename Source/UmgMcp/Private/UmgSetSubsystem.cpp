#include "UmgSetSubsystem.h"
#include "UmgAttentionSubsystem.h"
#include "Editor.h"
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetTree.h"
#include "Components/PanelWidget.h"
#include "JsonObjectConverter.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Kismet2/BlueprintEditorUtils.h"

DEFINE_LOG_CATEGORY(LogUmgSet);

// Helper function to get the cached blueprint from the attention subsystem
static UWidgetBlueprint* GetCachedWidgetBlueprintForSet(FString& OutError)
{
    if (!GEditor)
    {
        OutError = TEXT("GEditor is not available.");
        return nullptr;
    }
    UUmgAttentionSubsystem* AttentionSubsystem = GEditor->GetEditorSubsystem<UUmgAttentionSubsystem>();
    if (!AttentionSubsystem)
    {
        OutError = TEXT("UmgAttentionSubsystem is not available.");
        return nullptr;
    }
    UWidgetBlueprint* WidgetBlueprint = AttentionSubsystem->GetCachedTargetWidgetBlueprint();
    if (!WidgetBlueprint)
    {
        OutError = TEXT("No cached target UMG asset found. Please set a target first.");
        return nullptr;
    }
    return WidgetBlueprint;
}

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

bool UUmgSetSubsystem::SetWidgetProperties(const FString& WidgetName, const FString& PropertiesJson)
{
    FString ErrorMessage;
    UWidgetBlueprint* WidgetBlueprint = GetCachedWidgetBlueprintForSet(ErrorMessage);
    if (!WidgetBlueprint)
    {
        UE_LOG(LogUmgSet, Error, TEXT("SetWidgetProperties: %s"), *ErrorMessage);
        return false;
    }

    if (!WidgetBlueprint->WidgetTree)
    {
        UE_LOG(LogUmgSet, Error, TEXT("SetWidgetProperties: WidgetTree is null for asset '%s'."), *WidgetBlueprint->GetPathName());
        return false;
    }

    UWidget* FoundWidget = WidgetBlueprint->WidgetTree->FindWidget(FName(*WidgetName));
    if (!FoundWidget)
    {
        UE_LOG(LogUmgSet, Error, TEXT("SetWidgetProperties: Failed to find widget '%s' in asset '%s'."), *WidgetName, *WidgetBlueprint->GetPathName());
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

FString UUmgSetSubsystem::CreateWidget(const FString& ParentName, const FString& WidgetType, const FString& WidgetName)
{
    FString ErrorMessage;
    UWidgetBlueprint* WidgetBlueprint = GetCachedWidgetBlueprintForSet(ErrorMessage);
    if (!WidgetBlueprint)
    {
        UE_LOG(LogUmgSet, Error, TEXT("CreateWidget: %s"), *ErrorMessage);
        return FString();
    }

    if (!WidgetBlueprint->WidgetTree)
    {
        UE_LOG(LogUmgSet, Error, TEXT("CreateWidget: WidgetTree is null for asset '%s'."), *WidgetBlueprint->GetPathName());
        return FString();
    }

    UPanelWidget* ParentWidget = Cast<UPanelWidget>(WidgetBlueprint->WidgetTree->FindWidget(FName(*ParentName)));
    if (!ParentWidget)
    {
        UE_LOG(LogUmgSet, Error, TEXT("CreateWidget: Failed to find ParentWidget with name '%s' in asset '%s'."), *ParentName, *WidgetBlueprint->GetPathName());
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

bool UUmgSetSubsystem::DeleteWidget(const FString& WidgetName)
{
    FString ErrorMessage;
    UWidgetBlueprint* WidgetBlueprint = GetCachedWidgetBlueprintForSet(ErrorMessage);
    if (!WidgetBlueprint)
    {
        UE_LOG(LogUmgSet, Error, TEXT("DeleteWidget: %s"), *ErrorMessage);
        return false;
    }

    if (!WidgetBlueprint->WidgetTree)
    {
        UE_LOG(LogUmgSet, Error, TEXT("DeleteWidget: WidgetTree is null for asset '%s'."), *WidgetBlueprint->GetPathName());
        return false;
    }

    UWidget* FoundWidget = WidgetBlueprint->WidgetTree->FindWidget(FName(*WidgetName));
    if (!FoundWidget)
    {
        UE_LOG(LogUmgSet, Error, TEXT("DeleteWidget: Failed to find widget '%s' in asset '%s'."), *WidgetName, *WidgetBlueprint->GetPathName());
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

bool UUmgSetSubsystem::ReparentWidget(const FString& WidgetName, const FString& NewParentName)
{
    FString ErrorMessage;
    UWidgetBlueprint* WidgetBlueprint = GetCachedWidgetBlueprintForSet(ErrorMessage);
    if (!WidgetBlueprint)
    {
        UE_LOG(LogUmgSet, Error, TEXT("ReparentWidget: %s"), *ErrorMessage);
        return false;
    }

    if (!WidgetBlueprint->WidgetTree)
    {
        UE_LOG(LogUmgSet, Error, TEXT("ReparentWidget: WidgetTree is null for asset '%s'."), *WidgetBlueprint->GetPathName());
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