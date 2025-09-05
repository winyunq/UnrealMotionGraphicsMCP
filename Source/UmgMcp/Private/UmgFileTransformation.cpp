// UmgFileTransformation.cpp (v1.1 - Enhanced with Slot inlining and compile fix)

#include "UmgFileTransformation.h"
#include "Blueprint/UserWidget.h"
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetTree.h"
#include "Components/PanelWidget.h"
#include "Components/PanelSlot.h" // Required for UPanelSlot
#include "JsonObjectConverter.h"
#include "Serialization/JsonWriter.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "UObject/UnrealType.h" // Required for FProperty
#include "Misc/PackageName.h"
#include "UObject/ObjectMacros.h"    

// Log category for this module
DEFINE_LOG_CATEGORY_STATIC(LogUmgMcp, Log, All);

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

            // THE CORE CHANGE: Only serialize if the property value is not identical to the default.
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

                                // Also check for default values on slot properties
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
                else // Handle all other non-default properties
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
    
    // Only add the properties object if it's not empty
    if (PropertiesJson->Values.Num() > 0)
    {
        WidgetJson->SetObjectField(TEXT("properties"), PropertiesJson);
    }


    // 3. Recursively process child widgets
    TArray<TSharedPtr<FJsonValue>> ChildrenJsonArray;
    if (UPanelWidget* PanelWidget = Cast<UPanelWidget>(Widget))
    {
        for (int32 i = 0; i < PanelWidget->GetChildrenCount(); ++i)
        {
            if (UWidget* ChildWidget = PanelWidget->GetChildAt(i))
            {
                TSharedPtr<FJsonObject> ChildJson = ExportWidgetToJson(ChildWidget); // Recursive call
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
    
    // Use StaticLoadObject which is safer in editor utility contexts.
    UWidgetBlueprint* WidgetBlueprint = Cast<UWidgetBlueprint>(StaticLoadObject(UWidgetBlueprint::StaticClass(), nullptr, *PackageName));

    if (!WidgetBlueprint)
    {
        UE_LOG(LogUmgMcp, Error, TEXT("ExportUmgAssetToJsonString: Failed to load UWidgetBlueprint from path '%s'."), *AssetPath);
        return FString();
    }
    
    // Ensure the WidgetTree is valid
    if (!WidgetBlueprint->WidgetTree)
    {
        UE_LOG(LogUmgMcp, Error, TEXT("ExportUmgAssetToJsonString: WidgetTree is null in UWidgetBlueprint '%s'."), *AssetPath);
        return FString();
    }

    UWidget* RootWidget = WidgetBlueprint->WidgetTree->RootWidget;

    if (!RootWidget)
    {
        UE_LOG(LogUmgMcp, Warning, TEXT("ExportUmgAssetToJsonString: Root widget not found in UWidgetBlueprint '%s'. This may be an empty UI."), *AssetPath);
        // For an empty UI, we can return an empty JSON object or a representation of the empty tree.
        // Returning an empty string for now to indicate potential issue.
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

#include "WidgetBlueprintFactory.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Kismet2/BlueprintEditorUtils.h"

// Forward declaration for our recursive helper function
static UWidget* CreateWidgetFromJson(const TSharedPtr<FJsonObject>& WidgetJson, UWidgetTree* WidgetTree, UWidget* ParentWidget);

// Recursive helper function to build the widget tree from JSON data
static UWidget* CreateWidgetFromJson(const TSharedPtr<FJsonObject>& WidgetJson, UWidgetTree* WidgetTree, UWidget* ParentWidget)
{
    if (!WidgetJson.IsValid() || !WidgetTree)
    {
        return nullptr;
    }

    FString WidgetClassPath, WidgetName;
    if (!WidgetJson->TryGetStringField(TEXT("widget_class"), WidgetClassPath) || !WidgetJson->TryGetStringField(TEXT("widget_name"), WidgetName))
    {
        UE_LOG(LogUmgMcp, Error, TEXT("CreateWidgetFromJson: JSON is missing widget_class or widget_name."));
        return nullptr;
    }

    UClass* WidgetClass = FindObject<UClass>(ANY_PACKAGE, *WidgetClassPath);
    if (!WidgetClass)
    {
        UE_LOG(LogUmgMcp, Error, TEXT("CreateWidgetFromJson: Failed to find widget class '%s'."), *WidgetClassPath);
        return nullptr;
    }

    UWidget* NewWidget = NewObject<UWidget>(WidgetTree, WidgetClass, FName(*WidgetName));
    if (!NewWidget)
    {
        UE_LOG(LogUmgMcp, Error, TEXT("CreateWidgetFromJson: Failed to create widget of class '%s'."), *WidgetClassPath);
        return nullptr;
    }

    // Add to parent first, which is necessary for the Slot object to be created.
    if (UPanelWidget* ParentPanel = Cast<UPanelWidget>(ParentWidget))
    {
        ParentPanel->AddChild(NewWidget);
    }

    // Apply properties from the JSON
    const TSharedPtr<FJsonObject>* PropertiesJsonObj;
    if (WidgetJson->TryGetObjectField(TEXT("properties"), PropertiesJsonObj))
    {
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
                        // Apply properties directly to the slot object
                        for (const auto& SlotPair : (*SlotJsonObj)->Values)
                        {
                            FProperty* SlotProperty = FindFProperty<FProperty>(NewWidget->Slot->GetClass(), *SlotPair.Key);
                            if(SlotProperty)
                            {
                                FJsonObjectConverter::JsonValueToUProperty(SlotPair.Value, SlotProperty, NewWidget->Slot, 0, 0);
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
                    FJsonObjectConverter::JsonValueToUProperty(JsonVal, Property, NewWidget, 0, 0);
                }
            }
        }
    }

    // Recursively create children
    const TArray<TSharedPtr<FJsonValue>>* ChildrenJsonArray;
    if (WidgetJson->TryGetArrayField(TEXT("children"), ChildrenJsonArray))
    {
        for (const TSharedPtr<FJsonValue>& ChildValue : *ChildrenJsonArray)
        {
            const TSharedPtr<FJsonObject>* ChildObject;
            if (ChildValue->TryGetObject(ChildObject))
            {
                CreateWidgetFromJson(*ChildObject, WidgetTree, NewWidget);
            }
        }
    }

    return NewWidget;
}

bool UUmgFileTransformation::ApplyJsonStringToUmgAsset(const FString& AssetPath, const FString& JsonData)
{
    // 1. Parse the JSON string
    TSharedPtr<FJsonObject> RootJsonObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonData);
    if (!FJsonSerializer::Deserialize(Reader, RootJsonObject) || !RootJsonObject.IsValid())
    {
        UE_LOG(LogUmgMcp, Error, TEXT("ApplyJsonStringToUmgAsset: Failed to parse JSON data."));
        return false;
    }

    // 2. Find or Create the Widget Blueprint asset
    FString PackageName = FPackageName::ObjectPathToPackageName(AssetPath);
    UPackage* Package = CreatePackage(*PackageName);
    if (!Package)
    {
        UE_LOG(LogUmgMcp, Error, TEXT("ApplyJsonStringToUmgAsset: Failed to create package '%s'."), *PackageName);
        return false;
    }
    
    FString AssetName = FPackageName::GetLongPackageAssetName(PackageName);
    UWidgetBlueprint* WidgetBlueprint = FindObject<UWidgetBlueprint>(Package, *AssetName);

    if (!WidgetBlueprint)
    {
        auto Factory = NewObject<UWidgetBlueprintFactory>();
        WidgetBlueprint = Cast<UWidgetBlueprint>(Factory->FactoryCreateNew(UWidgetBlueprint::StaticClass(), Package, FName(*AssetName), RF_Public | RF_Standalone, nullptr, GWarn));

        if (!WidgetBlueprint)
        {
            UE_LOG(LogUmgMcp, Error, TEXT("ApplyJsonStringToUmgAsset: Failed to create new Widget Blueprint at '%s'."), *AssetPath);
            return false;
        }
        FAssetRegistryModule::AssetCreated(WidgetBlueprint);
    }

    if (!WidgetBlueprint->WidgetTree)
    {
        UE_LOG(LogUmgMcp, Error, TEXT("ApplyJsonStringToUmgAsset: WidgetTree is null in UWidgetBlueprint '%s'."), *AssetPath);
        return false;
    }

    // 3. Rebuild the widget tree from JSON
    WidgetBlueprint->Modify(); // Mark the asset as dirty before changing it
    if (WidgetBlueprint->WidgetTree->RootWidget)
    {
        WidgetBlueprint->WidgetTree->RemoveWidget(WidgetBlueprint->WidgetTree->RootWidget);
    }
    
    UWidget* NewRootWidget = CreateWidgetFromJson(RootJsonObject, WidgetBlueprint->WidgetTree, nullptr);

    if (!NewRootWidget)
    {
        UE_LOG(LogUmgMcp, Error, TEXT("ApplyJsonStringToUmgAsset: Failed to create root widget from JSON for asset '%s'."), *AssetPath);
        return false;
    }

    WidgetBlueprint->WidgetTree->RootWidget = NewRootWidget;

    // 4. Mark the asset as structurally modified to ensure editor updates
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBlueprint);
    UE_LOG(LogUmgMcp, Log, TEXT("Successfully applied JSON to UMG asset '%s'."), *AssetPath);

    return true;
}