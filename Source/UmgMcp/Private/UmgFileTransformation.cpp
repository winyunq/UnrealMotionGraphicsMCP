// UmgFileTransformation.cpp (v1.3 - Added comments and thread safety)

#include "UmgFileTransformation.h"
#include "Blueprint/UserWidget.h"
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetTree.h"
#include "Components/PanelWidget.h"
#include "Components/PanelSlot.h"
#include "Components/TextBlock.h"
#include "JsonObjectConverter.h"
#include "Serialization/JsonWriter.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "UObject/UnrealType.h"
#include "Misc/PackageName.h"
#include "UObject/ObjectMacros.h"
#include "Async/Async.h"
#include "WidgetBlueprintFactory.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Kismet2/BlueprintEditorUtils.h"

// Log category for this module
DEFINE_LOG_CATEGORY_STATIC(LogUmgMcp, Log, All);

// Forward declaration for our recursive helper function
static UWidget* CreateWidgetFromJson(const TSharedPtr<FJsonObject>& WidgetJson, UWidgetTree* WidgetTree, UWidget* ParentWidget);

bool ApplyJsonToUmgAsset_GameThread(const FString& AssetPath, const FString& JsonData);

TSharedPtr<FJsonObject> UUmgFileTransformation::ExportWidgetToJson(UWidget* Widget)
{
    if (!Widget)
    {
        UE_LOG(LogUmgMcp, Warning, TEXT("ExportWidgetToJson: Input Widget is null."));
        return nullptr;
    }

    TSharedPtr<FJsonObject> WidgetJson = MakeShared<FJsonObject>();
    UObject* DefaultWidget = Widget->GetClass()->GetDefaultObject();

    // 1. Add basic information
    WidgetJson->SetStringField(TEXT("widget_name"), Widget->GetName());
    WidgetJson->SetStringField(TEXT("widget_class"), Widget->GetClass()->GetPathName());

    // 2. Iterate over all properties and only add non-default ones
    TSharedPtr<FJsonObject> PropertiesJson = MakeShared<FJsonObject>();
    
    for (TFieldIterator<FProperty> PropIt(Widget->GetClass()); PropIt; ++PropIt)
    {
        FProperty* Property = *PropIt;

        bool bIsEditorOnly = false;
#if WITH_EDITOR
        bIsEditorOnly = Property->HasAnyPropertyFlags(CPF_EditorOnly);
#endif

        if (Property->HasAnyPropertyFlags(CPF_Edit) && !Property->HasAnyPropertyFlags(CPF_Transient) && !bIsEditorOnly)
        {
            void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Widget);
            void* DefaultValuePtr = Property->ContainerPtrToValuePtr<void>(DefaultWidget);

            if (!Property->Identical(ValuePtr, DefaultValuePtr))
            {
                if (Property->GetFName() == TEXT("Slot"))
                {
                    if (UPanelSlot* SlotObject = Cast<UPanelSlot>(CastField<FObjectProperty>(Property)->GetObjectPropertyValue_InContainer(Widget)))
                    {
                        TSharedPtr<FJsonObject> SlotPropertiesJson = MakeShared<FJsonObject>();
                        UObject* DefaultSlotObject = SlotObject->GetClass()->GetDefaultObject();

                        for (TFieldIterator<FProperty> SlotPropIt(SlotObject->GetClass()); SlotPropIt; ++SlotPropIt)
                        {
                            FProperty* SlotProperty = *SlotPropIt;
                            FName SlotPropertyName = SlotProperty->GetFName();

                            if (SlotPropertyName == TEXT("Content") || SlotPropertyName == TEXT("Parent"))
                            {
                                continue;
                            }

                            if (SlotProperty->HasAnyPropertyFlags(CPF_Edit) && !SlotProperty->HasAnyPropertyFlags(CPF_Transient))
                            {
                                void* SlotValuePtr = SlotProperty->ContainerPtrToValuePtr<void>(SlotObject);
                                void* DefaultSlotValuePtr = SlotProperty->ContainerPtrToValuePtr<void>(DefaultSlotObject);

                                if (!SlotProperty->Identical(SlotValuePtr, DefaultSlotValuePtr))
                                {
                                    TSharedPtr<FJsonValue> SlotPropertyJsonValue = FJsonObjectConverter::UPropertyToJsonValue(SlotProperty, SlotValuePtr);
                                    if (SlotPropertyJsonValue.IsValid())
                                    {
                                        SlotPropertiesJson->SetField(SlotProperty->GetName(), SlotPropertyJsonValue);
                                    }
                                }
                            }
                        }
                        
                        if(SlotPropertiesJson->Values.Num() > 0)
                        {
                           PropertiesJson->SetObjectField(TEXT("Slot"), SlotPropertiesJson);
                        }
                    }
                }
                else
                {
                    TSharedPtr<FJsonValue> PropertyJsonValue = FJsonObjectConverter::UPropertyToJsonValue(Property, ValuePtr);
                    if (PropertyJsonValue.IsValid())
                    {
                        PropertiesJson->SetField(Property->GetName(), PropertyJsonValue);
                    }
                }
            }
        }
    }
    
    if (PropertiesJson->Values.Num() > 0)
    {
        WidgetJson->SetObjectField(TEXT("properties"), PropertiesJson);
    }

    TArray<TSharedPtr<FJsonValue>> ChildrenJsonArray;
    if (UPanelWidget* PanelWidget = Cast<UPanelWidget>(Widget))
    {
        for (int32 i = 0; i < PanelWidget->GetChildrenCount(); ++i)
        {
            if (UWidget* ChildWidget = PanelWidget->GetChildAt(i))
            {
                TSharedPtr<FJsonObject> ChildJson = ExportWidgetToJson(ChildWidget);
                if (ChildJson.IsValid())
                {
                    ChildrenJsonArray.Add(MakeShared<FJsonValueObject>(ChildJson));
                }
            }
        }
    }
    
    if (ChildrenJsonArray.Num() > 0)
    {
        WidgetJson->SetArrayField(TEXT("children"), ChildrenJsonArray);
    }

    return WidgetJson;
}

FString UUmgFileTransformation::ExportUmgAssetToJsonString(const FString& AssetPath)
{
    FString PackageName = FPackageName::ObjectPathToPackageName(AssetPath);
    
    UWidgetBlueprint* WidgetBlueprint = Cast<UWidgetBlueprint>(StaticLoadObject(UWidgetBlueprint::StaticClass(), nullptr, *PackageName));

    if (!WidgetBlueprint)
    {
        UE_LOG(LogUmgMcp, Error, TEXT("ExportUmgAssetToJsonString: Failed to load UWidgetBlueprint from path '%s'."), *AssetPath);
        return FString();
    }
    
    if (!WidgetBlueprint->WidgetTree)
    {
        UE_LOG(LogUmgMcp, Error, TEXT("ExportUmgAssetToJsonString: WidgetTree is null in UWidgetBlueprint '%s'."), *AssetPath);
        return FString();
    }

    UWidget* RootWidget = WidgetBlueprint->WidgetTree->RootWidget;

    if (!RootWidget)
    {
        UE_LOG(LogUmgMcp, Warning, TEXT("ExportUmgAssetToJsonString: Root widget not found in UWidgetBlueprint '%s'. This may be an empty UI."), *AssetPath);
        return FString();
    }

    TSharedPtr<FJsonObject> RootJsonObject = ExportWidgetToJson(RootWidget);

    if (!RootJsonObject.IsValid())
    {
        UE_LOG(LogUmgMcp, Error, TEXT("ExportUmgAssetToJsonString: Failed to convert root widget of '%s' to FJsonObject."), *AssetPath);
        return FString();
    }

    FString JsonString;
    TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> JsonWriter = TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&JsonString);
    
    if (FJsonSerializer::Serialize(RootJsonObject.ToSharedRef(), JsonWriter))
    {
        JsonWriter->Close();
        UE_LOG(LogUmgMcp, Log, TEXT("Successfully exported UMG asset '%s' to JSON."), *AssetPath);
        return JsonString;
    }
    else
    {
        JsonWriter->Close();
        UE_LOG(LogUmgMcp, Error, TEXT("ExportUmgAssetToJsonString: Failed to serialize FJsonObject to string for asset '%s'."), *AssetPath);
        return FString();
    }
}

bool UUmgFileTransformation::ApplyJsonStringToUmgAsset(const FString& AssetPath, const FString& JsonData)
{
    // Dispatch the task to the game thread asynchronously ("fire and forget").
    // The original implementation used a blocking wait which could cause the editor to freeze or deadlock.
    // We capture parameters by value to ensure they are valid when the task eventually executes.
    FFunctionGraphTask::CreateAndDispatchWhenReady([AssetPath, JsonData]()
    {
        ApplyJsonToUmgAsset_GameThread(AssetPath, JsonData);
    }, TStatId(), nullptr, ENamedThreads::GameThread);

    // Return true to indicate the task was successfully dispatched.
    // The operation itself runs in the background and the result will be visible in the editor.
    return true;
}


// This function is executed on the game thread to ensure thread safety when dealing with UObjects.
bool ApplyJsonToUmgAsset_GameThread(const FString& AssetPath, const FString& JsonData)
{
    UE_LOG(LogUmgMcp, Log, TEXT("ApplyJsonToUmgAsset_GameThread: Starting for asset '%s'."), *AssetPath);

    // 1. Parse the incoming JSON data.
    UE_LOG(LogUmgMcp, Log, TEXT("ApplyJsonToUmgAsset_GameThread: Parsing JSON data."));
    TSharedPtr<FJsonObject> RootJsonObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonData);
    if (!FJsonSerializer::Deserialize(Reader, RootJsonObject) || !RootJsonObject.IsValid())
    {
        UE_LOG(LogUmgMcp, Error, TEXT("ApplyJsonToUmgAsset_GameThread: Failed to parse JSON data."));
        return false;
    }
    UE_LOG(LogUmgMcp, Log, TEXT("ApplyJsonToUmgAsset_GameThread: JSON data parsed successfully."));

    // 2. Load the target Widget Blueprint asset.
    UE_LOG(LogUmgMcp, Log, TEXT("ApplyJsonToUmgAsset_GameThread: Loading Widget Blueprint from '%s'."), *AssetPath);
    UWidgetBlueprint* WidgetBlueprint = Cast<UWidgetBlueprint>(StaticLoadObject(UWidgetBlueprint::StaticClass(), nullptr, *AssetPath));
    if (!WidgetBlueprint)
    {
        UE_LOG(LogUmgMcp, Error, TEXT("ApplyJsonToUmgAsset_GameThread: Failed to load Widget Blueprint at '%s'."), *AssetPath);
        return false;
    }
    UE_LOG(LogUmgMcp, Log, TEXT("ApplyJsonToUmgAsset_GameThread: Widget Blueprint loaded."));

    // 3. Ensure the WidgetTree is valid.
    if (!WidgetBlueprint->WidgetTree)
    {
        UE_LOG(LogUmgMcp, Error, TEXT("ApplyJsonToUmgAsset_GameThread: WidgetTree is null in UWidgetBlueprint '%s'."), *AssetPath);
        return false;
    }
    UE_LOG(LogUmgMcp, Log, TEXT("ApplyJsonToUmgAsset_GameThread: WidgetTree is valid."));

    // 4. Mark the blueprint as modified so the editor knows to save it.
    UE_LOG(LogUmgMcp, Log, TEXT("ApplyJsonToUmgAsset_GameThread: Marking blueprint as modified."));
    WidgetBlueprint->Modify();
    UE_LOG(LogUmgMcp, Log, TEXT("ApplyJsonToUmgAsset_GameThread: Blueprint marked as modified."));

    // 5. Clear the existing widget tree to rebuild it from the JSON data.
    UE_LOG(LogUmgMcp, Log, TEXT("ApplyJsonToUmgAsset_GameThread: Clearing existing widget tree."));
    if (WidgetBlueprint->WidgetTree->RootWidget)
    {
        WidgetBlueprint->WidgetTree->RemoveWidget(WidgetBlueprint->WidgetTree->RootWidget);
        UE_LOG(LogUmgMcp, Log, TEXT("ApplyJsonToUmgAsset_GameThread: Existing root widget removed."));
    }
    else
    {
        UE_LOG(LogUmgMcp, Log, TEXT("ApplyJsonToUmgAsset_GameThread: No existing root widget to remove."));
    }
    UE_LOG(LogUmgMcp, Log, TEXT("ApplyJsonToUmgAsset_GameThread: Existing widget tree cleared."));

    // 6. Recursively create the new widget tree from the JSON data.
    UE_LOG(LogUmgMcp, Log, TEXT("ApplyJsonToUmgAsset_GameThread: Starting recursive widget creation from JSON."));
    UWidget* NewRootWidget = CreateWidgetFromJson(RootJsonObject, WidgetBlueprint->WidgetTree, nullptr);
    if (!NewRootWidget)
    {
        UE_LOG(LogUmgMcp, Error, TEXT("ApplyJsonToUmgAsset_GameThread: Failed to create root widget from JSON for asset '%s'."), *AssetPath);
        return false;
    }
    UE_LOG(LogUmgMcp, Log, TEXT("ApplyJsonToUmgAsset_GameThread: New widget tree created."));

    // 7. Set the new root widget on the widget tree.
    UE_LOG(LogUmgMcp, Log, TEXT("ApplyJsonToUmgAsset_GameThread: Setting new root widget."));
    WidgetBlueprint->WidgetTree->RootWidget = NewRootWidget;
    UE_LOG(LogUmgMcp, Log, TEXT("ApplyJsonToUmgAsset_GameThread: New root widget set."));

    // 8. Mark the blueprint as structurally modified to trigger a refresh in the UMG editor.
    UE_LOG(LogUmgMcp, Log, TEXT("ApplyJsonToUmgAsset_GameThread: Marking blueprint as structurally modified."));
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBlueprint);
    UE_LOG(LogUmgMcp, Log, TEXT("ApplyJsonToUmgAsset_GameThread: Blueprint structurally modified."));
    UE_LOG(LogUmgMcp, Log, TEXT("Successfully applied JSON to UMG asset '%s'."), *AssetPath);

    return true;
}

static UWidget* CreateWidgetFromJson(const TSharedPtr<FJsonObject>& WidgetJson, UWidgetTree* WidgetTree, UWidget* ParentWidget)
{
    FString ParentName = ParentWidget ? ParentWidget->GetName() : TEXT("None");
    UE_LOG(LogUmgMcp, Log, TEXT("CreateWidgetFromJson: Starting for Parent: %s"), *ParentName);

    if (!WidgetJson.IsValid() || !WidgetTree)
    {
        UE_LOG(LogUmgMcp, Error, TEXT("CreateWidgetFromJson: Invalid WidgetJson or WidgetTree."));
        return nullptr;
    }
    UE_LOG(LogUmgMcp, Log, TEXT("CreateWidgetFromJson: WidgetJson and WidgetTree are valid."));

    FString WidgetClassPath, WidgetName;
    if (!WidgetJson->TryGetStringField(TEXT("widget_class"), WidgetClassPath) || !WidgetJson->TryGetStringField(TEXT("widget_name"), WidgetName))
    {
        UE_LOG(LogUmgMcp, Error, TEXT("CreateWidgetFromJson: JSON is missing widget_class or widget_name."));
        return nullptr;
    }
    UE_LOG(LogUmgMcp, Log, TEXT("CreateWidgetFromJson: Processing widget '%s' of class '%s'."), *WidgetName, *WidgetClassPath);

    UClass* WidgetClass = StaticLoadClass(UWidget::StaticClass(), nullptr, *WidgetClassPath);
    if (!WidgetClass)
    {
        UE_LOG(LogUmgMcp, Error, TEXT("CreateWidgetFromJson: Failed to find widget class '%s'."), *WidgetClassPath);
        return nullptr;
    }
    UE_LOG(LogUmgMcp, Log, TEXT("CreateWidgetFromJson: Widget class '%s' found."), *WidgetClassPath);

    UWidget* NewWidget = NewObject<UWidget>(WidgetTree, WidgetClass, FName(*WidgetName));
    if (!NewWidget)
    {
        UE_LOG(LogUmgMcp, Error, TEXT("CreateWidgetFromJson: Failed to create widget of class '%s'."), *WidgetClassPath);
        return nullptr;
    }
    UE_LOG(LogUmgMcp, Log, TEXT("CreateWidgetFromJson: Widget '%s' created."), *WidgetName);

    if (ParentWidget)
    {
        UPanelWidget* ParentPanel = Cast<UPanelWidget>(ParentWidget);
        if (ParentPanel)
        {
            UE_LOG(LogUmgMcp, Log, TEXT("CreateWidgetFromJson: Adding '%s' as child to parent '%s'."), *WidgetName, *ParentName);
            ParentPanel->AddChild(NewWidget);
            UE_LOG(LogUmgMcp, Log, TEXT("CreateWidgetFromJson: '%s' added as child."), *WidgetName);
        }
        else
        {
            UE_LOG(LogUmgMcp, Warning, TEXT("CreateWidgetFromJson: Parent '%s' is not a UPanelWidget, cannot add child '%s'."), *ParentName, *WidgetName);
        }
    }

    UE_LOG(LogUmgMcp, Log, TEXT("CreateWidgetFromJson: Applying default properties for '%s'."), *WidgetName);
    UObject* DefaultWidget = WidgetClass->GetDefaultObject();
    for (TFieldIterator<FProperty> PropIt(WidgetClass); PropIt; ++PropIt)
    {
        FProperty* Property = *PropIt;
        if (Property->HasAnyPropertyFlags(CPF_Edit))
        {
            void* DestPtr = Property->ContainerPtrToValuePtr<void>(NewWidget);
            const void* SrcPtr = Property->ContainerPtrToValuePtr<void>(DefaultWidget);
            Property->CopyCompleteValue(DestPtr, SrcPtr);
        }
    }
    UE_LOG(LogUmgMcp, Log, TEXT("CreateWidgetFromJson: Default properties applied for '%s'."), *WidgetName);

    const TSharedPtr<FJsonObject>* PropertiesJsonObj;
    if (WidgetJson->TryGetObjectField(TEXT("properties"), PropertiesJsonObj))
    {
        UE_LOG(LogUmgMcp, Log, TEXT("CreateWidgetFromJson: Applying custom properties for '%s'."), *WidgetName);
        for (const auto& Pair : (*PropertiesJsonObj)->Values)
        {
            const FString& PropertyName = Pair.Key;
            const TSharedPtr<FJsonValue>& JsonVal = Pair.Value;

            if (PropertyName == TEXT("Slot"))
            {
                if (NewWidget->Slot)
                {
                    const TSharedPtr<FJsonObject>* SlotJsonObj;
                    if (JsonVal->TryGetObject(SlotJsonObj))
                    {
                        for (const auto& SlotPair : (*SlotJsonObj)->Values)
                        {
                            FProperty* SlotProperty = FindFProperty<FProperty>(NewWidget->Slot->GetClass(), *SlotPair.Key);
                            if(SlotProperty)
                            {
                                if (SlotPair.Value.IsValid())
                                {
                                    UE_LOG(LogUmgMcp, Log, TEXT("CreateWidgetFromJson: Applying Slot property '%s' for '%s'."), *SlotPair.Key, *WidgetName);
                                    FJsonObjectConverter::JsonValueToUProperty(SlotPair.Value, SlotProperty, NewWidget->Slot, 0, 0);
                                }
                            }
                        }
                    }
                }
            }
            else
            {
                FProperty* Property = FindFProperty<FProperty>(NewWidget->GetClass(), *PropertyName);
                if (Property)
                {
                    if (JsonVal.IsValid())
                    {
                        bool bHandledManually = false;
                        // Check if it's an FText property and the JSON value is a simple string
                        if (Property->IsA(FTextProperty::StaticClass()) && JsonVal->Type == EJson::String)
                        {
                            FString SourceString = JsonVal->AsString();
                            if (!SourceString.IsEmpty())
                            {
                                FTextProperty* TextProp = CastField<FTextProperty>(Property);
                                if (TextProp)
                                {
                                    TextProp->SetPropertyValue_InContainer(NewWidget, FText::FromString(SourceString));
                                    UE_LOG(LogUmgMcp, Log, TEXT("CreateWidgetFromJson: Manually applied FText property '%s' for '%s' on '%s'."), *PropertyName, *SourceString, *WidgetName);
                                    bHandledManually = true;
                                }
                            }
                            else
                            {
                                UE_LOG(LogUmgMcp, Warning, TEXT("CreateWidgetFromJson: Failed to extract SourceString for FText property '%s' of '%s' (empty string)."), *PropertyName, *WidgetName);
                            }
                        }

                        if (!bHandledManually) // Fallback to generic property application if not handled manually
                        {
                            UE_LOG(LogUmgMcp, Log, TEXT("CreateWidgetFromJson: Applying property '%s' for '%s'."), *PropertyName, *WidgetName);
                            FJsonObjectConverter::JsonValueToUProperty(JsonVal, Property, NewWidget, 0, 0);
                        }
                    }
                }
            }
        }
        UE_LOG(LogUmgMcp, Log, TEXT("CreateWidgetFromJson: Custom properties applied for '%s'."), *WidgetName);
    }
    else
    {
        UE_LOG(LogUmgMcp, Log, TEXT("CreateWidgetFromJson: No custom properties to apply for '%s'."), *WidgetName);
    }

    const TArray<TSharedPtr<FJsonValue>>* ChildrenJsonArray;
    if (WidgetJson->TryGetArrayField(TEXT("children"), ChildrenJsonArray))
    {
        UE_LOG(LogUmgMcp, Log, TEXT("CreateWidgetFromJson: Processing children for '%s'. Total children: %d"), *WidgetName, ChildrenJsonArray->Num());
        for (const TSharedPtr<FJsonValue>& ChildValue : *ChildrenJsonArray)
        {
            const TSharedPtr<FJsonObject>* ChildObject;
            if (ChildValue->TryGetObject(ChildObject))
            {
                CreateWidgetFromJson(*ChildObject, WidgetTree, NewWidget);
            }
        }
        UE_LOG(LogUmgMcp, Log, TEXT("CreateWidgetFromJson: Finished processing children for '%s'."), *WidgetName);
    }
    else
    {
        UE_LOG(LogUmgMcp, Log, TEXT("CreateWidgetFromJson: No children to process for '%s'."), *WidgetName);
    }

    UE_LOG(LogUmgMcp, Log, TEXT("CreateWidgetFromJson: Returning new widget '%s'."), *WidgetName);
    return NewWidget;
}