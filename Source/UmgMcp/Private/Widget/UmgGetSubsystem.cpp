// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#include "Widget/UmgGetSubsystem.h"
#include "FileManage/UmgAttentionSubsystem.h"
#include "Editor.h"

// --- Necessary Includes ---
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetTree.h"
#include "Components/PanelWidget.h"
#include "Components/PanelSlot.h"
#include "Components/CanvasPanelSlot.h"
#include "FileManage/UmgFileTransformation.h"
#include "JsonObjectConverter.h"
#include "Serialization/JsonWriter.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "UObject/UnrealType.h"
#include "Misc/PackageName.h"
#include "Widgets/SWidget.h"
#include "Layout/Geometry.h"
// --- End Includes ---

DEFINE_LOG_CATEGORY(LogUmgGet);

// --- Helper function for recursive simplified tree export ---
// Exports only widget_name, widget_type, and children fields (no property payloads).
static TSharedPtr<FJsonObject> ExportWidgetToSimplifiedTree(UWidget* Widget)
{
    if (!Widget)
    {
        return nullptr;
    }

    TSharedPtr<FJsonObject> WidgetJson = MakeShared<FJsonObject>();
    WidgetJson->SetStringField(TEXT("widget_name"), Widget->GetName());
    WidgetJson->SetStringField(TEXT("widget_type"), Widget->GetClass()->GetName());

    TArray<TSharedPtr<FJsonValue>> ChildrenJsonArray;
    if (UPanelWidget* PanelWidget = Cast<UPanelWidget>(Widget))
    {
        for (int32 i = 0; i < PanelWidget->GetChildrenCount(); ++i)
        {
            if (UWidget* ChildWidget = PanelWidget->GetChildAt(i))
            {
                TSharedPtr<FJsonObject> ChildJson = ExportWidgetToSimplifiedTree(ChildWidget);
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


// --- Subsystem Implementation ---

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

FString UUmgGetSubsystem::GetWidgetTree(UWidgetBlueprint* WidgetBlueprint)
{
    if (!WidgetBlueprint)
    {
        UE_LOG(LogUmgGet, Error, TEXT("GetWidgetTree: Received a null WidgetBlueprint."));
        return FString();
    }
    
    UWidgetTree* WidgetTree = WidgetBlueprint->WidgetTree;
    if (!WidgetTree)
    {
        UE_LOG(LogUmgGet, Error, TEXT("GetWidgetTree: WidgetTree is null in UWidgetBlueprint '%s'."), *WidgetBlueprint->GetPathName());
        return FString();
    }

    UWidget* RootWidget = WidgetTree->RootWidget;
    if (!RootWidget)
    {
        UE_LOG(LogUmgGet, Warning, TEXT("GetWidgetTree: Root widget not found in UWidgetBlueprint '%s'. The UMG asset might be empty."), *WidgetBlueprint->GetPathName());
        TSharedPtr<FJsonObject> EmptyTreeJson = MakeShared<FJsonObject>();
        EmptyTreeJson->SetStringField(TEXT("widget_name"), TEXT("EmptyWidgetTree"));
        EmptyTreeJson->SetStringField(TEXT("widget_type"), TEXT("Empty"));
        EmptyTreeJson->SetArrayField(TEXT("children"), {});

        FString EmptyJsonString;
        TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> EmptyWriter = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&EmptyJsonString);
        if (FJsonSerializer::Serialize(EmptyTreeJson.ToSharedRef(), EmptyWriter))
        {
            EmptyWriter->Close();
            return EmptyJsonString;
        }
        else
        {
            EmptyWriter->Close();
            UE_LOG(LogUmgGet, Error, TEXT("GetWidgetTree: Failed to serialize empty widget tree JSON for '%s'."), *WidgetBlueprint->GetPathName());
            return FString();
        }
    }

    UWidget* TreeStartWidget = RootWidget;
    if (GEditor)
    {
        if (UUmgAttentionSubsystem* AttentionSubsystem = GEditor->GetEditorSubsystem<UUmgAttentionSubsystem>())
        {
            const FString TargetWidgetName = AttentionSubsystem->GetTargetWidget();
            if (!TargetWidgetName.IsEmpty())
            {
                if (UWidget* TargetWidget = WidgetTree->FindWidget(FName(*TargetWidgetName)))
                {
                    TreeStartWidget = TargetWidget;
                }
                else
                {
                    UE_LOG(LogUmgGet, Warning, TEXT("GetWidgetTree: Target widget '%s' not found in '%s'. Falling back to root widget. Verify widget name or call the set_target_widget command with a valid widget."), *TargetWidgetName, *WidgetBlueprint->GetPathName());
                }
            }
        }
    }

    TSharedPtr<FJsonObject> RootJsonObject = ExportWidgetToSimplifiedTree(TreeStartWidget);
    if (!RootJsonObject.IsValid())
    {
        UE_LOG(LogUmgGet, Error, TEXT("GetWidgetTree: Failed to convert root widget of '%s' to FJsonObject."), *WidgetBlueprint->GetPathName());
        return FString();
    }

    FString JsonString;
    // Use condensed print policy to reduce data size
    TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> JsonWriter = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&JsonString);
    if (FJsonSerializer::Serialize(RootJsonObject.ToSharedRef(), JsonWriter))
    {
        JsonWriter->Close();
        return JsonString;
    }
    
    JsonWriter->Close();
    UE_LOG(LogUmgGet, Error, TEXT("GetWidgetTree: Failed to serialize FJsonObject to string for asset '%s'."), *WidgetBlueprint->GetPathName());
    return FString();
}

FString UUmgGetSubsystem::QueryWidgetProperties(UWidgetBlueprint* WidgetBlueprint, const FString& WidgetName, const TArray<FString>& Properties)
{
    if (!WidgetBlueprint)
    {
        UE_LOG(LogUmgGet, Error, TEXT("QueryWidgetProperties: Received a null WidgetBlueprint."));
        return FString();
    }

    if (!WidgetBlueprint->WidgetTree)
    {
        UE_LOG(LogUmgGet, Error, TEXT("QueryWidgetProperties: WidgetTree is null for asset '%s'."), *WidgetBlueprint->GetPathName());
        return FString();
    }

    UWidget* FoundWidget = WidgetBlueprint->WidgetTree->FindWidget(FName(*WidgetName));
    if (!FoundWidget)
    {
        UE_LOG(LogUmgGet, Error, TEXT("QueryWidgetProperties: Failed to find widget '%s' in asset '%s'."), *WidgetName, *WidgetBlueprint->GetPathName());
        return FString();
    }

    TSharedPtr<FJsonObject> PropertiesJson = MakeShared<FJsonObject>();
    for (const FString& PropPath : Properties)
    {
        TArray<FString> Parts;
        PropPath.ParseIntoArray(Parts, TEXT("."));
        
        UObject* CurrentObject = FoundWidget;
        int32 PartIndex = 0;

        // If path starts with "Slot", we switch to the Slot object
        if (Parts.Num() > 1 && Parts[0].Equals(TEXT("Slot"), ESearchCase::IgnoreCase))
        {
            CurrentObject = FoundWidget->Slot;
            PartIndex = 1;
            
            if (!CurrentObject)
            {
                UE_LOG(LogUmgGet, Warning, TEXT("QueryWidgetProperties: Widget '%s' has no slot, but 'Slot' property requested."), *WidgetName);
                continue;
            }

            // --- ALIAS MAPPING FOR QUERY ---
            if (Parts.Num() == 2)
            {
                if (Parts[1].Equals(TEXT("Position"), ESearchCase::IgnoreCase))
                {
                    // Special case: return [Left, Top] from LayoutData.Offsets
                    if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(CurrentObject))
                    {
                        FVector2D Pos = CanvasSlot->GetPosition();
                        TArray<TSharedPtr<FJsonValue>> PosArr;
                        PosArr.Add(MakeShared<FJsonValueNumber>(Pos.X));
                        PosArr.Add(MakeShared<FJsonValueNumber>(Pos.Y));
                        PropertiesJson->SetArrayField(PropPath, PosArr);
                        UE_LOG(LogUmgGet, Log, TEXT("Query: Mapped 'Slot.Position' to [%f, %f]"), Pos.X, Pos.Y);
                        continue;
                    }
                    else
                    {
                        UE_LOG(LogUmgGet, Warning, TEXT("Query: 'Slot.Position' requested but slot is not a CanvasPanelSlot (Type: %s)"), *CurrentObject->GetClass()->GetName());
                    }
                }
                else if (Parts[1].Equals(TEXT("Size"), ESearchCase::IgnoreCase))
                {
                    if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(CurrentObject))
                    {
                        FVector2D Size = CanvasSlot->GetSize();
                        TArray<TSharedPtr<FJsonValue>> SizeArr;
                        SizeArr.Add(MakeShared<FJsonValueNumber>(Size.X));
                        SizeArr.Add(MakeShared<FJsonValueNumber>(Size.Y));
                        PropertiesJson->SetArrayField(PropPath, SizeArr);
                        UE_LOG(LogUmgGet, Log, TEXT("Query: Mapped 'Slot.Size' to [%f, %f]"), Size.X, Size.Y);
                        continue;
                    }
                }
                else if (Parts[1].Equals(TEXT("Anchors"), ESearchCase::IgnoreCase))
                {
                    Parts.Reset();
                    Parts.Add(TEXT("Slot"));
                    Parts.Add(TEXT("LayoutData"));
                    Parts.Add(TEXT("Anchors"));
                    PartIndex = 1; 
                }
                else if (Parts[1].Equals(TEXT("Alignment"), ESearchCase::IgnoreCase))
                {
                    Parts.Reset();
                    Parts.Add(TEXT("Slot"));
                    Parts.Add(TEXT("LayoutData"));
                    Parts.Add(TEXT("Alignment"));
                    PartIndex = 1;
                }
            }
        }

        // Traverse the path for properties or struct fields
        void* CurrentValuePtr = CurrentObject;
        UStruct* CurrentStruct = CurrentObject ? CurrentObject->GetClass() : nullptr;
        FProperty* LastProperty = nullptr;

        while (PartIndex < Parts.Num() && CurrentStruct)
        {
            FProperty* Property = CurrentStruct->FindPropertyByName(FName(*Parts[PartIndex]));
            if (!Property)
            {
                // Try Case-Insensitive search as a fallback
                Property = nullptr;
                for (TFieldIterator<FProperty> It(CurrentStruct); It; ++It)
                {
                    if (It->GetName().Equals(Parts[PartIndex], ESearchCase::IgnoreCase))
                    {
                        Property = *It;
                        break;
                    }
                }
            }

            if (Property)
            {
                LastProperty = Property;
                CurrentValuePtr = Property->ContainerPtrToValuePtr<void>(CurrentValuePtr);
                
                if (PartIndex == Parts.Num() - 1)
                {
                    // Found the target property
                    TSharedPtr<FJsonValue> PropertyJsonValue = FJsonObjectConverter::UPropertyToJsonValue(Property, CurrentValuePtr);
                    if (PropertyJsonValue.IsValid())
                    {
                        PropertiesJson->SetField(PropPath, PropertyJsonValue);
                    }
                }
                else
                {
                    // Move into struct
                    if (FStructProperty* StructProp = CastField<FStructProperty>(Property))
                    {
                        CurrentStruct = StructProp->Struct;
                    }
                    else if (FObjectProperty* ObjProp = CastField<FObjectProperty>(Property))
                    {
                        CurrentObject = ObjProp->GetObjectPropertyValue(CurrentValuePtr);
                        CurrentStruct = CurrentObject ? CurrentObject->GetClass() : nullptr;
                        CurrentValuePtr = CurrentObject;
                    }
                    else
                    {
                        break; // Cannot traverse into non-struct/object
                    }
                }
            }
            else
            {
                break; // Property not found
            }
            PartIndex++;
        }
    }

    FString JsonString;
    TSharedRef<TJsonWriter<>> JsonWriter = TJsonWriterFactory<>::Create(&JsonString);
    FJsonSerializer::Serialize(PropertiesJson.ToSharedRef(), JsonWriter);
    JsonWriter->Close(); // Explicitly close
    return JsonString;
}

FString UUmgGetSubsystem::GetLayoutData(UWidgetBlueprint* WidgetBlueprint, int32 ResolutionWidth, int32 ResolutionHeight)
{
    if (!WidgetBlueprint)
    {
        UE_LOG(LogUmgGet, Error, TEXT("GetLayoutData: Received a null WidgetBlueprint."));
        return FString();
    }

    if (!WidgetBlueprint->WidgetTree)
    {
        UE_LOG(LogUmgGet, Error, TEXT("GetLayoutData: WidgetTree is null for asset '%s'."), *WidgetBlueprint->GetPathName());
        return FString();
    }

    TArray<UWidget*> AllWidgets;
    WidgetBlueprint->WidgetTree->GetAllWidgets(AllWidgets);

    TArray<TSharedPtr<FJsonValue>> LayoutDataArray;

    for (UWidget* Widget : AllWidgets)
    {
        if (Widget && Widget->GetCachedWidget().IsValid())
        {
            FGeometry WidgetGeometry = Widget->GetCachedWidget()->GetTickSpaceGeometry();
            FSlateRect BoundingBox = WidgetGeometry.GetLayoutBoundingRect();

            TSharedPtr<FJsonObject> WidgetLayoutJson = MakeShared<FJsonObject>();
            WidgetLayoutJson->SetStringField(TEXT("widget_id"), Widget->GetPathName());
            WidgetLayoutJson->SetNumberField(TEXT("left"), BoundingBox.Left);
            WidgetLayoutJson->SetNumberField(TEXT("top"), BoundingBox.Top);
            WidgetLayoutJson->SetNumberField(TEXT("right"), BoundingBox.Right);
            WidgetLayoutJson->SetNumberField(TEXT("bottom"), BoundingBox.Bottom);

            LayoutDataArray.Add(MakeShared<FJsonValueObject>(WidgetLayoutJson));
        }
    }

    FString JsonString;
    TSharedRef<TJsonWriter<>> JsonWriter = TJsonWriterFactory<>::Create(&JsonString);
    FJsonSerializer::Serialize(LayoutDataArray, JsonWriter);
    JsonWriter->Close(); // Explicitly close
    return JsonString;
}

bool UUmgGetSubsystem::CheckWidgetOverlap(UWidgetBlueprint* WidgetBlueprint, const TArray<FString>& WidgetIds)
{
    if (!WidgetBlueprint)
    {
        UE_LOG(LogUmgGet, Error, TEXT("CheckWidgetOverlap: Received a null WidgetBlueprint."));
        return false;
    }

    if (!WidgetBlueprint->WidgetTree)
    {
        UE_LOG(LogUmgGet, Error, TEXT("CheckWidgetOverlap: WidgetTree is null for asset '%s'."), *WidgetBlueprint->GetPathName());
        return false;
    }

    TArray<UWidget*> AllWidgets;
    WidgetBlueprint->WidgetTree->GetAllWidgets(AllWidgets);

    TArray<FSlateRect> BoundingBoxes;
    for (UWidget* Widget : AllWidgets)
    {
        if (Widget && Widget->GetCachedWidget().IsValid())
        {
            BoundingBoxes.Add(Widget->GetCachedWidget()->GetTickSpaceGeometry().GetLayoutBoundingRect());
        }
    }

    for (int32 i = 0; i < BoundingBoxes.Num(); ++i)
    {
        for (int32 j = i + 1; j < BoundingBoxes.Num(); ++j)
        {
            if (FSlateRect::DoRectanglesIntersect(BoundingBoxes[i], BoundingBoxes[j]))
            {
                UE_LOG(LogUmgGet, Warning, TEXT("CheckWidgetOverlap: Overlap detected in %s."), *WidgetBlueprint->GetPathName());
                return true; // Found an overlap
            }
        }
    }

    return false; // No overlaps found
}

FString UUmgGetSubsystem::GetAssetFileSystemPath(const FString& AssetPath)
{
    if (AssetPath.IsEmpty())
    {
        UE_LOG(LogUmgGet, Warning, TEXT("GetAssetFileSystemPath called with empty AssetPath."));
        return FString();
    }

    FString FileSystemPath;
    if (FPackageName::TryConvertLongPackageNameToFilename(AssetPath, FileSystemPath, FPackageName::GetAssetPackageExtension()))
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

FString UUmgGetSubsystem::GetWidgetSchema(const FString& WidgetType)
{
    UClass* WidgetClass = FindObject<UClass>(nullptr, *WidgetType);
    if (!WidgetClass)
    {
        WidgetClass = LoadObject<UClass>(nullptr, *WidgetType);
    }

    if (!WidgetClass)
    {
        UE_LOG(LogUmgGet, Error, TEXT("GetWidgetSchema: Failed to find or load widget class '%s'."), *WidgetType);
        return FString();
    }

    TSharedPtr<FJsonObject> SchemaJson = MakeShared<FJsonObject>();
    SchemaJson->SetStringField(TEXT("widget_type"), WidgetType);
    
    TSharedPtr<FJsonObject> PropertiesJson = MakeShared<FJsonObject>();

    for (TFieldIterator<FProperty> PropIt(WidgetClass); PropIt; ++PropIt)
    {
        FProperty* Property = *PropIt;
        bool bIsEditorOnly = false;
#if WITH_EDITOR
        bIsEditorOnly = Property->HasAnyPropertyFlags(CPF_EditorOnly);
#endif
        if (Property->HasAnyPropertyFlags(CPF_Edit) && !bIsEditorOnly)
        {
            TSharedPtr<FJsonObject> PropInfo = MakeShared<FJsonObject>();
            PropInfo->SetStringField(TEXT("type"), Property->GetCPPType());
            
            // Add more metadata if needed, e.g., tooltip, category
#if WITH_EDITOR
            PropInfo->SetStringField(TEXT("tooltip"), Property->GetToolTipText().ToString());
#endif
            PropertiesJson->SetObjectField(Property->GetName(), PropInfo);
        }
    }

    SchemaJson->SetObjectField(TEXT("properties"), PropertiesJson);

    FString JsonString;
    TSharedRef<TJsonWriter<>> JsonWriter = TJsonWriterFactory<>::Create(&JsonString);
    FJsonSerializer::Serialize(SchemaJson.ToSharedRef(), JsonWriter);
    return JsonString;
}
