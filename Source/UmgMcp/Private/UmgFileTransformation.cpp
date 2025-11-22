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
    // 0. Handle default workspace: if AssetPath is empty, use default path
    FString FinalAssetPath = AssetPath;
    if (FinalAssetPath.IsEmpty() || FinalAssetPath.TrimStartAndEnd().IsEmpty())
    {
        FinalAssetPath = TEXT("/Game/UnrealMotionGraphicsMCP.UnrealMotionGraphicsMCP");
        UE_LOG(LogUmgMcp, Log, TEXT("ApplyJsonToUmgAsset_GameThread: No asset path provided. Using default workspace: '%s'."), *FinalAssetPath);
    }
    
    UE_LOG(LogUmgMcp, Log, TEXT("ApplyJsonToUmgAsset_GameThread: Starting for asset '%s'."), *FinalAssetPath);

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

    // 2. Load or create the target Widget Blueprint asset.
    UE_LOG(LogUmgMcp, Log, TEXT("ApplyJsonToUmgAsset_GameThread: Loading Widget Blueprint from '%s'."), *FinalAssetPath);
    UWidgetBlueprint* WidgetBlueprint = Cast<UWidgetBlueprint>(StaticLoadObject(UWidgetBlueprint::StaticClass(), nullptr, *FinalAssetPath));
    
    bool bIsNewlyCreated = false;  // Track if this is a newly created blueprint
    
    if (!WidgetBlueprint)
    {
        UE_LOG(LogUmgMcp, Warning, TEXT("ApplyJsonToUmgAsset_GameThread: Widget Blueprint at '%s' not found. Attempting to create new asset."), *FinalAssetPath);
        
        // Extract package path and asset name from FinalAssetPath
        // FinalAssetPath format: /Game/FolderName/AssetName.AssetName
        FString PackagePath, AssetName;
        if (FinalAssetPath.Split(TEXT("."), &PackagePath, &AssetName, ESearchCase::IgnoreCase, ESearchDir::FromEnd))
        {
            UE_LOG(LogUmgMcp, Log, TEXT("ApplyJsonToUmgAsset_GameThread: Package: '%s', Asset: '%s'"), *PackagePath, *AssetName);
            
            // Create a new package
            UPackage* Package = CreatePackage(*PackagePath);
            if (!Package)
            {
                UE_LOG(LogUmgMcp, Error, TEXT("ApplyJsonToUmgAsset_GameThread: Failed to create package at '%s'."), *PackagePath);
                return false;
            }
            
            // Create the Widget Blueprint using the factory
            UWidgetBlueprintFactory* Factory = NewObject<UWidgetBlueprintFactory>();
            WidgetBlueprint = Cast<UWidgetBlueprint>(Factory->FactoryCreateNew(
                UWidgetBlueprint::StaticClass(),
                Package,
                FName(*AssetName),
                RF_Public | RF_Standalone,
                nullptr,
                GWarn
            ));
            
            if (!WidgetBlueprint)
            {
                UE_LOG(LogUmgMcp, Error, TEXT("ApplyJsonToUmgAsset_GameThread: Failed to create new Widget Blueprint at '%s'."), *FinalAssetPath);
                return false;
            }
            
            bIsNewlyCreated = true;  // Mark as newly created
            
            // Mark package as dirty and notify asset registry
            Package->MarkPackageDirty();
            FAssetRegistryModule::AssetCreated(WidgetBlueprint);
            
            UE_LOG(LogUmgMcp, Log, TEXT("ApplyJsonToUmgAsset_GameThread: New Widget Blueprint created at '%s'."), *FinalAssetPath);
        }
        else
        {
            UE_LOG(LogUmgMcp, Error, TEXT("ApplyJsonToUmgAsset_GameThread: Invalid asset path format '%s'."), *FinalAssetPath);
            return false;
        }
    }
    else
    {
        UE_LOG(LogUmgMcp, Log, TEXT("ApplyJsonToUmgAsset_GameThread: Widget Blueprint loaded."));
    }

    // 3. Ensure the WidgetTree is valid.
    if (!WidgetBlueprint->WidgetTree)
    {
        UE_LOG(LogUmgMcp, Error, TEXT("ApplyJsonToUmgAsset_GameThread: WidgetTree is null in UWidgetBlueprint '%s'."), *FinalAssetPath);
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
        UE_LOG(LogUmgMcp, Error, TEXT("ApplyJsonToUmgAsset_GameThread: Failed to create root widget from JSON for asset '%s'."), *FinalAssetPath);
        return false;
    }
    UE_LOG(LogUmgMcp, Log, TEXT("ApplyJsonToUmgAsset_GameThread: New widget tree created."));

    // 7. Set the new root widget on the widget tree.
    UE_LOG(LogUmgMcp, Log, TEXT("ApplyJsonToUmgAsset_GameThread: Setting new root widget."));
    WidgetBlueprint->WidgetTree->RootWidget = NewRootWidget;
    UE_LOG(LogUmgMcp, Log, TEXT("ApplyJsonToUmgAsset_GameThread: New root widget set: %s"), *NewRootWidget->GetName());

    // 7.5. Verify widget tree integrity before marking as modified
    UE_LOG(LogUmgMcp, Log, TEXT("ApplyJsonToUmgAsset_GameThread: Verifying widget tree integrity."));
    if (!WidgetBlueprint->WidgetTree->RootWidget)
    {
        UE_LOG(LogUmgMcp, Error, TEXT("ApplyJsonToUmgAsset_GameThread: Root widget became null after assignment!"));
        return false;
    }
    
    // Ensure all widgets are properly owned by the WidgetTree
    TArray<UWidget*> AllWidgets;
    WidgetBlueprint->WidgetTree->GetAllWidgets(AllWidgets);
    UE_LOG(LogUmgMcp, Log, TEXT("ApplyJsonToUmgAsset_GameThread: Widget tree contains %d widgets."), AllWidgets.Num());

    // 8. Save the blueprint by marking package as dirty
    // Note: We don't call MarkBlueprintAsStructurallyModified because it causes crashes
    // in async context, even for existing blueprints. Instead, we just mark dirty and let
    // the user save manually or the editor will prompt on project close.
    UE_LOG(LogUmgMcp, Log, TEXT("ApplyJsonToUmgAsset_GameThread: Marking package as dirty."));
    
    UPackage* Package = WidgetBlueprint->GetOutermost();
    if (Package)
    {
        Package->MarkPackageDirty();
        FString PackageFileName = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
        UE_LOG(LogUmgMcp, Log, TEXT("ApplyJsonToUmgAsset_GameThread: Package marked dirty: %s"), *PackageFileName);
        
        if (bIsNewlyCreated)
        {
            UE_LOG(LogUmgMcp, Log, TEXT("ApplyJsonToUmgAsset_GameThread: New asset created. Please save manually (Ctrl+S) or it will be saved on project close."));
        }
        else
        {
            UE_LOG(LogUmgMcp, Log, TEXT("ApplyJsonToUmgAsset_GameThread: Existing asset modified. Changes applied. If editor is open, close and reopen the asset to see changes, or press Compile."));
        }
    }
    
    UE_LOG(LogUmgMcp, Log, TEXT("Successfully applied JSON to UMG asset '%s'."), *FinalAssetPath);

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

    // Extract Slot properties early (before adding to parent) for later processing
    TSharedPtr<FJsonObject> SlotPropertiesObj = nullptr;
    const TSharedPtr<FJsonObject>* PropertiesJsonObj;
    if (WidgetJson->TryGetObjectField(TEXT("properties"), PropertiesJsonObj))
    {
        const TSharedPtr<FJsonObject>* SlotJsonObj;
        if ((*PropertiesJsonObj)->TryGetObjectField(TEXT("Slot"), SlotJsonObj))
        {
            SlotPropertiesObj = *SlotJsonObj;
        }
    }

    // Add to parent FIRST so that Slot object is created
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

    // Apply default properties
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

    // Apply custom properties (excluding Slot properties which we handle separately)
    if (WidgetJson->TryGetObjectField(TEXT("properties"), PropertiesJsonObj))
    {
        UE_LOG(LogUmgMcp, Log, TEXT("CreateWidgetFromJson: Applying custom properties for '%s'."), *WidgetName);
        for (const auto& Pair : (*PropertiesJsonObj)->Values)
        {
            const FString& PropertyName = Pair.Key;
            const TSharedPtr<FJsonValue>& JsonVal = Pair.Value;

            // Skip Slot property here - we handle it later
            if (PropertyName == TEXT("Slot"))
            {
                continue;
            }

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
        UE_LOG(LogUmgMcp, Log, TEXT("CreateWidgetFromJson: Custom properties applied for '%s'."), *WidgetName);
    }
    else
    {
        UE_LOG(LogUmgMcp, Log, TEXT("CreateWidgetFromJson: No custom properties to apply for '%s'."), *WidgetName);
    }

    // NOW apply Slot properties after widget is added to parent
    if (SlotPropertiesObj.IsValid() && NewWidget->Slot)
    {
        UE_LOG(LogUmgMcp, Log, TEXT("CreateWidgetFromJson: Applying Slot properties for '%s'."), *WidgetName);
        for (const auto& SlotPair : SlotPropertiesObj->Values)
        {
            const FString& SlotPropertyName = SlotPair.Key;
            const TSharedPtr<FJsonValue>& SlotJsonVal = SlotPair.Value;
            
            FProperty* SlotProperty = FindFProperty<FProperty>(NewWidget->Slot->GetClass(), *SlotPropertyName);
            if (SlotProperty && SlotJsonVal.IsValid())
            {
                UE_LOG(LogUmgMcp, Log, TEXT("CreateWidgetFromJson: Applying Slot property '%s' for '%s'."), *SlotPropertyName, *WidgetName);
                FJsonObjectConverter::JsonValueToUProperty(SlotJsonVal, SlotProperty, NewWidget->Slot, 0, 0);
            }
            else if (!SlotProperty)
            {
                UE_LOG(LogUmgMcp, Warning, TEXT("CreateWidgetFromJson: Slot property '%s' not found on Slot class '%s' for widget '%s'."), 
                    *SlotPropertyName, 
                    *NewWidget->Slot->GetClass()->GetName(), 
                    *WidgetName);
            }
        }
        UE_LOG(LogUmgMcp, Log, TEXT("CreateWidgetFromJson: Slot properties applied for '%s'."), *WidgetName);
    }
    else if (SlotPropertiesObj.IsValid() && !NewWidget->Slot)
    {
        UE_LOG(LogUmgMcp, Warning, TEXT("CreateWidgetFromJson: Slot properties specified but NewWidget->Slot is null for '%s'. Widget may not have been added to a parent panel."), *WidgetName);
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