#include "Widget/UmgSetSubsystem.h"
#include "FileManage/UmgAttentionSubsystem.h"
#include "Editor.h"
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetTree.h"
#include "Components/PanelWidget.h"
#include "JsonObjectConverter.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "FileHelpers.h"

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

// Forward declaration
static TSharedPtr<FJsonObject> NormalizeJsonKeysToPascalCase(const TSharedPtr<FJsonObject>& SourceJson);

/**
 * Normalizes JSON keys from camelCase to PascalCase to match C++ UPROPERTY names.
 * (Same implementation as in UmgFileTransformation.cpp)
 */
static TSharedPtr<FJsonObject> NormalizeJsonKeysToPascalCase(const TSharedPtr<FJsonObject>& SourceJson)
{
    if (!SourceJson.IsValid())
    {
        return nullptr;
    }
    
    TSharedPtr<FJsonObject> NormalizedJson = MakeShared<FJsonObject>();
    
    for (const auto& Pair : SourceJson->Values)
    {
        FString OriginalKey = Pair.Key;
        FString NormalizedKey = OriginalKey;
        
        // Convert first character to uppercase (camelCase → PascalCase)
        if (NormalizedKey.Len() > 0 && FChar::IsLower(NormalizedKey[0]))
        {
            NormalizedKey[0] = FChar::ToUpper(NormalizedKey[0]);
        }
        
        // Recursively process nested objects
        TSharedPtr<FJsonValue> Value = Pair.Value;
        if (Value->Type == EJson::Object)
        {
            TSharedPtr<FJsonObject> NormalizedNestedObj = NormalizeJsonKeysToPascalCase(Value->AsObject());
            NormalizedJson->SetObjectField(NormalizedKey, NormalizedNestedObj);
        }
        else if (Value->Type == EJson::Array)
        {
            TArray<TSharedPtr<FJsonValue>> SourceArray = Value->AsArray();
            TArray<TSharedPtr<FJsonValue>> NormalizedArray;
            
            for (const TSharedPtr<FJsonValue>& ArrayValue : SourceArray)
            {
                if (ArrayValue->Type == EJson::Object)
                {
                    TSharedPtr<FJsonObject> NormalizedArrayObj = NormalizeJsonKeysToPascalCase(ArrayValue->AsObject());
                    NormalizedArray.Add(MakeShared<FJsonValueObject>(NormalizedArrayObj));
                }
                else
                {
                    NormalizedArray.Add(ArrayValue);
                }
            }
            NormalizedJson->SetArrayField(NormalizedKey, NormalizedArray);
        }
        else
        {
            NormalizedJson->SetField(NormalizedKey, Value);
        }
    }
    
    return NormalizedJson;
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

    // Normalize JSON keys from camelCase to PascalCase (especially important for Slot properties)
    UE_LOG(LogUmgSet, Log, TEXT("SetWidgetProperties: Normalizing property keys for widget '%s'"), *WidgetName);
    TSharedPtr<FJsonObject> NormalizedProperties = NormalizeJsonKeysToPascalCase(PropertiesJsonObject);
    
    // Separate Slot properties from widget properties (Slot must be applied to the Slot object, not the Widget)
    TSharedPtr<FJsonObject> SlotProperties;
    if (NormalizedProperties->HasField(TEXT("Slot")))
    {
        UE_LOG(LogUmgSet, Log, TEXT("SetWidgetProperties: Detected Slot property for widget '%s'"), *WidgetName);
        
        SlotProperties = NormalizedProperties->GetObjectField(TEXT("Slot"));
        
        // Log the normalized Slot JSON
        FString SlotJsonString;
        TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&SlotJsonString);
        FJsonSerializer::Serialize(SlotProperties.ToSharedRef(), Writer);
        UE_LOG(LogUmgSet, Log, TEXT("SetWidgetProperties: Normalized Slot JSON: %s"), *SlotJsonString);
        
        // Remove Slot from widget properties (we'll apply it separately)
        NormalizedProperties->RemoveField(TEXT("Slot"));
    }

    WidgetBlueprint->Modify();
    FoundWidget->Modify();

    // Apply widget properties (excluding Slot)
    if (NormalizedProperties->Values.Num() > 0)
    {
        if (!FJsonObjectConverter::JsonObjectToUStruct(NormalizedProperties.ToSharedRef(), FoundWidget->GetClass(), FoundWidget, 0, 0))
        {
            UE_LOG(LogUmgSet, Warning, TEXT("SetWidgetProperties: JsonObjectToUStruct reported issues applying some properties to '%s'."), *WidgetName);
        }
        else
        {
            UE_LOG(LogUmgSet, Log, TEXT("SetWidgetProperties: Successfully applied widget properties to '%s'."), *WidgetName);
        }
    }
    
    // Apply Slot properties separately (CRITICAL: must apply to Slot object, not Widget object)
    if (SlotProperties.IsValid() && FoundWidget->Slot)
    {
        UE_LOG(LogUmgSet, Log, TEXT("SetWidgetProperties: Applying Slot properties to Slot object (class: %s)"), *FoundWidget->Slot->GetClass()->GetName());
        
        FoundWidget->Slot->Modify();
        
        if (!FJsonObjectConverter::JsonObjectToUStruct(SlotProperties.ToSharedRef(), FoundWidget->Slot->GetClass(), FoundWidget->Slot, 0, 0))
        {
            UE_LOG(LogUmgSet, Warning, TEXT("SetWidgetProperties: Issues applying Slot properties to '%s'."), *WidgetName);
        }
        else
        {
            UE_LOG(LogUmgSet, Log, TEXT("SetWidgetProperties: Successfully applied Slot properties to '%s'."), *WidgetName);
        }
    }
    else if (SlotProperties.IsValid() && !FoundWidget->Slot)
    {
        UE_LOG(LogUmgSet, Warning, TEXT("SetWidgetProperties: Slot properties specified but widget '%s' has no Slot object."), *WidgetName);
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

    // Load widget class first to check if it's valid
    // Load widget class first to check if it's valid
    UClass* WidgetClass = FindObject<UClass>(nullptr, *WidgetType);
    if(!WidgetClass)
    {
        WidgetClass = LoadObject<UClass>(nullptr, *WidgetType);
    }
    
    // Fallback: Try finding it in the UMG package (for native widgets like VerticalBox, TextBlock)
    if (!WidgetClass)
    {
        FString NativePath = FString::Printf(TEXT("/Script/UMG.%s"), *WidgetType);
        WidgetClass = FindObject<UClass>(nullptr, *NativePath);
        if (!WidgetClass)
        {
             WidgetClass = LoadObject<UClass>(nullptr, *NativePath);
        }
    }

    // Fallback: Try finding it with 'U' prefix in UMG package (e.g. UVerticalBox) - though usually reflection name is without U
    if (!WidgetClass)
    {
        FString NativePathU = FString::Printf(TEXT("/Script/UMG.U%s"), *WidgetType);
        WidgetClass = FindObject<UClass>(nullptr, *NativePathU);
         if (!WidgetClass)
        {
             WidgetClass = LoadObject<UClass>(nullptr, *NativePathU);
        }
    }
    
    if (!WidgetClass)
    {
        UE_LOG(LogUmgSet, Error, TEXT("CreateWidget: Failed to find or load widget class '%s'."), *WidgetType);
        return FString();
    }

    // Check if this is a request to create root widget (no existing root)
    bool bCreatingRootWidget = false;
    if (!WidgetBlueprint->WidgetTree->RootWidget)
    {
        // If the tree is empty, ANY creation request must be for the root.
        // We are lenient here: if the widget type is a Panel (valid for root), we allow it regardless of what ParentName the AI sent.
        // This handles cases where AI sends "None", "Null", or hallucinates a parent name.
        
        if (WidgetClass->IsChildOf(UPanelWidget::StaticClass()))
        {
            bCreatingRootWidget = true;
            UE_LOG(LogUmgSet, Log, TEXT("CreateWidget: No root widget exists. Auto-promoting '%s' to root widget (ignoring ParentName: '%s')."), *WidgetType, *ParentName);
        }
        else
        {
            UE_LOG(LogUmgSet, Error, TEXT("CreateWidget: Cannot create '%s' as root widget. Root widget must be a Panel type (e.g., VerticalBox, HorizontalBox, CanvasPanel)."), *WidgetType);
            return FString();
        }
    }

    UPanelWidget* ParentWidget = nullptr;
    
    if (!bCreatingRootWidget)
    {
        // Normal case: creating a child widget, need to find parent
        ParentWidget = Cast<UPanelWidget>(WidgetBlueprint->WidgetTree->FindWidget(FName(*ParentName)));
        if (!ParentWidget)
        {
            // Check if root widget exists
            if (!WidgetBlueprint->WidgetTree->RootWidget)
            {
                UE_LOG(LogUmgSet, Error, TEXT("CreateWidget: No root widget exists. Cannot create child widget '%s'. You must first create a root widget using a Panel type."), *WidgetName);
                UE_LOG(LogUmgSet, Error, TEXT("CreateWidget: To create a root widget, use parent_name=\"\" or \"Root\" with a Panel widget type like VerticalBox or HorizontalBox."));
            }
            else
            {
                UE_LOG(LogUmgSet, Error, TEXT("CreateWidget: Failed to find ParentWidget with name '%s' in asset '%s'."), *ParentName, *WidgetBlueprint->GetPathName());
            }
            return FString();
        }
    }

    WidgetBlueprint->Modify();

    UWidget* NewWidget = WidgetBlueprint->WidgetTree->ConstructWidget<UWidget>(WidgetClass, FName(*WidgetName));
    if (!NewWidget)
    {
        UE_LOG(LogUmgSet, Error, TEXT("CreateWidget: Failed to construct widget of class '%s'."), *WidgetType);
        return FString();
    }

    if (bCreatingRootWidget)
    {
        // Set as root widget
        WidgetBlueprint->WidgetTree->RootWidget = NewWidget;
        UE_LOG(LogUmgSet, Log, TEXT("CreateWidget: Successfully created '%s' as root widget."), *WidgetName);
    }
    else
    {
        // Add as child to parent
        ParentWidget->AddChild(NewWidget);
        UE_LOG(LogUmgSet, Log, TEXT("CreateWidget: Successfully created '%s' as child of '%s'."), *WidgetName, *ParentName);
    }

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

bool UUmgSetSubsystem::SaveAsset(UWidgetBlueprint* WidgetBlueprint)
{
    if (!WidgetBlueprint)
    {
        UE_LOG(LogUmgSet, Error, TEXT("SaveAsset: Received a null WidgetBlueprint."));
        return false;
    }

    UPackage* Package = WidgetBlueprint->GetOutermost();
    if (!Package)
    {
        UE_LOG(LogUmgSet, Error, TEXT("SaveAsset: Failed to get package for asset '%s'."), *WidgetBlueprint->GetPathName());
        return false;
    }

    TArray<UPackage*> PackagesToSave;
    PackagesToSave.Add(Package);

    FEditorFileUtils::EPromptReturnCode ReturnCode = FEditorFileUtils::PromptForCheckoutAndSave(PackagesToSave, false, false);
    
    if (ReturnCode == FEditorFileUtils::EPromptReturnCode::PR_Success)
    {
        UE_LOG(LogUmgSet, Log, TEXT("SaveAsset: Successfully saved asset '%s'."), *WidgetBlueprint->GetPathName());
        return true;
    }
    else
    {
        UE_LOG(LogUmgSet, Error, TEXT("SaveAsset: Failed to save asset '%s'."), *WidgetBlueprint->GetPathName());
        return false;
    }
}
