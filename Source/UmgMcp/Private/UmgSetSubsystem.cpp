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
#include "Kismet2/KismetEditorUtilities.h"

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

bool UUmgSetSubsystem::SetWidgetProperties(UWidgetBlueprint* WidgetBlueprint, const FString& WidgetName, const FString& PropertiesJson)
{
    if (!WidgetBlueprint)
    {
        UE_LOG(LogUmgSet, Error, TEXT("SetWidgetProperties: Received a null WidgetBlueprint."));
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
            if (Pair.Value.IsValid() && Pair.Value->IsNull())
            {
                UE_LOG(LogUmgSet, Warning, TEXT("SetWidgetProperties: Received a null JSON value for property '%s' on widget '%s'. Skipping to prevent potential crash."), *Pair.Key, *WidgetName);
                continue;
            }
            FJsonObjectConverter::JsonValueToUProperty(Pair.Value, Property, FoundWidget, 0, 0);
        }
    }

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBlueprint);
    return true;
}

FString UUmgSetSubsystem::CreateWidget(UWidgetBlueprint* WidgetBlueprint, const FString& ParentName, const FString& WidgetType, const FString& WidgetName)
{
    if (!WidgetBlueprint)
    {
        UE_LOG(LogUmgSet, Error, TEXT("CreateWidget: Received a null WidgetBlueprint."));
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

    UClass* WidgetClass = FindObject<UClass>(ANY_PACKAGE, *WidgetType);
    if(!WidgetClass)
    {
        WidgetClass = LoadObject<UClass>(nullptr, *WidgetType);
    }
    
    if (!WidgetClass)
    {
        UE_LOG(LogUmgSet, Error, TEXT("CreateWidget: Failed to find or load widget class '%s'."), *WidgetType);
        return FString();
    }

    WidgetBlueprint->Modify();

    UWidget* NewWidget = WidgetBlueprint->WidgetTree->ConstructWidget<UWidget>(WidgetClass, FName(*WidgetName));
    if (!NewWidget)
    {
        UE_LOG(LogUmgSet, Error, TEXT("CreateWidget: Failed to construct widget of class '%s'."), *WidgetType);
        return FString();
    }

    ParentWidget->AddChild(NewWidget);

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBlueprint);
    return NewWidget->GetName();
}

bool UUmgSetSubsystem::DeleteWidget(UWidgetBlueprint* WidgetBlueprint, const FString& WidgetName)
{
    if (!WidgetBlueprint)
    {
        UE_LOG(LogUmgSet, Error, TEXT("DeleteWidget: Received a null WidgetBlueprint."));
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

bool UUmgSetSubsystem::ReparentWidget(UWidgetBlueprint* WidgetBlueprint, const FString& WidgetName, const FString& NewParentName)
{
    if (!WidgetBlueprint)
    {
        UE_LOG(LogUmgSet, Error, TEXT("ReparentWidget: Received a null WidgetBlueprint."));
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

    if (WidgetToMove->GetParent())
    {
        WidgetToMove->GetParent()->RemoveChild(WidgetToMove);
    }

    NewParentWidget->AddChild(WidgetToMove);
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBlueprint);
    return true;
}
