// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#include "FileManage/UmgFileTransformation.h"
#include "UmgMcp.h"
#include "PropertyNameMappings.h"
#include "FileManage/UmgAttentionSubsystem.h"

#include "Blueprint/UserWidget.h"
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetTree.h"
#include "Components/PanelWidget.h"
#include "Components/PanelSlot.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/VerticalBox.h"
#include "Async/Async.h"
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
#include "Preview/UmgPreviewRenderUtils.h"
#include "Widget/UmgMcpPropertyJsonUtils.h"
// Forward declarations
static UWidget* CreateWidgetFromJson(const TSharedPtr<FJsonObject>& WidgetJson, UWidgetTree* WidgetTree, UWidget* ParentWidget);
static void ApplyPropertiesToExistingWidget(const TSharedPtr<FJsonObject>& WidgetJson, UWidget* TargetWidget);
static void MergeChildrenAppendOnly(const TSharedPtr<FJsonObject>& WidgetJson, UWidgetTree* WidgetTree, UWidget* TargetWidget);
static void UpsertWidgetFromJsonAppendOnly(const TSharedPtr<FJsonObject>& WidgetJson, UWidgetTree* WidgetTree, UWidget* TargetWidget);

/** Extract slot properties from properties.Slot and/or top-level SlotData. */
static TSharedPtr<FJsonObject> ExtractSlotPropsFromWidgetJson(const TSharedPtr<FJsonObject>& WidgetJson)
{
    if (!WidgetJson.IsValid())
    {
        return nullptr;
    }

    TSharedPtr<FJsonObject> SlotProps = nullptr;

    const TSharedPtr<FJsonObject>* SlotDataPtr = nullptr;
    if (WidgetJson->TryGetObjectField(TEXT("SlotData"), SlotDataPtr) ||
        WidgetJson->TryGetObjectField(TEXT("slot_data"), SlotDataPtr))
    {
        SlotProps = MakeShared<FJsonObject>(*(*SlotDataPtr));
    }

    const TSharedPtr<FJsonObject>* PropertiesJsonObjPtr = nullptr;
    if (WidgetJson->TryGetObjectField(TEXT("properties"), PropertiesJsonObjPtr))
    {
        const TSharedPtr<FJsonObject>* SlotObjPtr = nullptr;
        if ((*PropertiesJsonObjPtr)->TryGetObjectField(TEXT("Slot"), SlotObjPtr))
        {
            if (!SlotProps.IsValid())
            {
                SlotProps = MakeShared<FJsonObject>(*(*SlotObjPtr));
            }
            else
            {
                for (const auto& Pair : (*SlotObjPtr)->Values)
                {
                    SlotProps->SetField(Pair.Key, Pair.Value);
                }
            }
        }
    }

    return SlotProps;
}

/** Apply slot JSON to the widget's current UPanelSlot with parent-type validation. */
static bool ApplySlotPropertiesToWidget(UWidget* Widget, const TSharedPtr<FJsonObject>& SlotProps, FString* OutRejectedKeys = nullptr)
{
    if (!Widget || !SlotProps.IsValid() || !Widget->Slot)
    {
        return true;
    }

    UPanelSlot* Slot = Widget->Slot;
    TSharedPtr<FJsonObject> NormalizedSlotProps = UUmgFileTransformation::NormalizeJsonKeysToPascalCase(SlotProps);

    // Canvas aliases: Size[w,h] -> LayoutData.Offsets.Right/Bottom; Anchors -> LayoutData.Anchors
    if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Slot))
    {
        TSharedPtr<FJsonValue> SizeVal = NormalizedSlotProps->TryGetField(TEXT("Size"));
        if (SizeVal.IsValid() && SizeVal->Type == EJson::Array)
        {
            const TArray<TSharedPtr<FJsonValue>>& Arr = SizeVal->AsArray();
            if (Arr.Num() >= 2)
            {
                TSharedPtr<FJsonObject> LayoutData = NormalizedSlotProps->HasField(TEXT("LayoutData"))
                    ? MakeShared<FJsonObject>(*NormalizedSlotProps->GetObjectField(TEXT("LayoutData")))
                    : MakeShared<FJsonObject>();

                TSharedPtr<FJsonObject> Offsets = LayoutData->HasField(TEXT("Offsets"))
                    ? MakeShared<FJsonObject>(*LayoutData->GetObjectField(TEXT("Offsets")))
                    : MakeShared<FJsonObject>();

                if (!Offsets->HasField(TEXT("Left"))) { Offsets->SetNumberField(TEXT("Left"), 0.0); }
                if (!Offsets->HasField(TEXT("Top"))) { Offsets->SetNumberField(TEXT("Top"), 0.0); }
                Offsets->SetNumberField(TEXT("Right"), Arr[0]->AsNumber());
                Offsets->SetNumberField(TEXT("Bottom"), Arr[1]->AsNumber());
                LayoutData->SetObjectField(TEXT("Offsets"), Offsets);
                NormalizedSlotProps->SetObjectField(TEXT("LayoutData"), LayoutData);
                NormalizedSlotProps->RemoveField(TEXT("Size"));
            }
        }

        if (NormalizedSlotProps->HasField(TEXT("Anchors")))
        {
            TSharedPtr<FJsonObject> LayoutData = NormalizedSlotProps->HasField(TEXT("LayoutData"))
                ? MakeShared<FJsonObject>(*NormalizedSlotProps->GetObjectField(TEXT("LayoutData")))
                : MakeShared<FJsonObject>();
            LayoutData->SetObjectField(TEXT("Anchors"), NormalizedSlotProps->GetObjectField(TEXT("Anchors")));
            NormalizedSlotProps->SetObjectField(TEXT("LayoutData"), LayoutData);
            NormalizedSlotProps->RemoveField(TEXT("Anchors"));
        }
    }

    // Whitelist filter per slot class
    static TMap<FString, TSet<FString>> AllowedSlotKeys;
    if (AllowedSlotKeys.Num() == 0)
    {
        AllowedSlotKeys.Add(TEXT("VerticalBoxSlot"), {"Size", "Padding", "HorizontalAlignment", "VerticalAlignment"});
        AllowedSlotKeys.Add(TEXT("HorizontalBoxSlot"), {"Size", "Padding", "HorizontalAlignment", "VerticalAlignment"});
        AllowedSlotKeys.Add(TEXT("CanvasPanelSlot"), {"LayoutData", "Alignment", "AutoSize", "ZOrder"});
        AllowedSlotKeys.Add(TEXT("GridSlot"), {"Row", "Column", "RowSpan", "ColumnSpan", "HorizontalAlignment", "VerticalAlignment", "Padding"});
        AllowedSlotKeys.Add(TEXT("OverlaySlot"), {"HorizontalAlignment", "VerticalAlignment", "Padding"});
        AllowedSlotKeys.Add(TEXT("ScrollBoxSlot"), {"Padding", "HorizontalAlignment", "VerticalAlignment"});
        AllowedSlotKeys.Add(TEXT("WrapBoxSlot"), {"Padding", "FillEmptySpace", "HorizontalAlignment", "VerticalAlignment"});
    }

    const FString SlotClassName = Slot->GetClass()->GetName();
    if (const TSet<FString>* Allowed = AllowedSlotKeys.Find(SlotClassName))
    {
        TSharedPtr<FJsonObject> Filtered = MakeShared<FJsonObject>();
        TArray<FString> Rejected;
        for (const auto& Pair : NormalizedSlotProps->Values)
        {
            const FString Key(Pair.Key.ToView());
            if (Allowed->Contains(Key))
            {
                Filtered->SetField(Pair.Key, Pair.Value);
            }
            else
            {
                Rejected.Add(Key);
            }
        }
        if (Rejected.Num() > 0)
        {
            UE_LOG(LogUmgMcp, Warning, TEXT("ApplySlotProperties: Rejected keys for '%s' (%s): %s"),
                *Widget->GetName(), *SlotClassName, *FString::Join(Rejected, TEXT(", ")));
            if (OutRejectedKeys)
            {
                *OutRejectedKeys = FString::Join(Rejected, TEXT(", "));
            }
        }
        NormalizedSlotProps = Filtered;
    }

    const bool bHasCanvasOnly =
        NormalizedSlotProps->HasField(TEXT("Anchors")) ||
        NormalizedSlotProps->HasField(TEXT("LayoutData")) ||
        NormalizedSlotProps->HasField(TEXT("Position"));

    if (bHasCanvasOnly && !Cast<UCanvasPanelSlot>(Slot))
    {
        UE_LOG(LogUmgMcp, Warning,
            TEXT("ApplySlotProperties: Discarding canvas-only keys for '%s' (slot type: %s)."),
            *Widget->GetName(), *Slot->GetClass()->GetName());

        TSharedPtr<FJsonObject> Filtered = MakeShared<FJsonObject>();
        for (const auto& Pair : NormalizedSlotProps->Values)
        {
            const FString Key(Pair.Key.ToView());
            if (Key != TEXT("Anchors") && Key != TEXT("LayoutData") && Key != TEXT("Position") && Key != TEXT("Alignment"))
            {
                Filtered->SetField(Pair.Key, Pair.Value);
            }
        }
        NormalizedSlotProps = Filtered;
    }

    if (NormalizedSlotProps->Values.Num() == 0)
    {
        return true;
    }

    Slot->Modify();
    if (!FJsonObjectConverter::JsonObjectToUStruct(NormalizedSlotProps.ToSharedRef(), Slot->GetClass(), Slot, 0, 0))
    {
        UE_LOG(LogUmgMcp, Warning,
            TEXT("ApplySlotProperties: Issues applying slot properties to '%s' (slot: %s)."),
            *Widget->GetName(), *Slot->GetClass()->GetName());
        return false;
    }
    return true;
}

static FApplyJsonToUmgResult MakeApplyError(const FString& Message)
{
    FApplyJsonToUmgResult Result;
    Result.bSuccess = false;
    Result.bApplied = false;
    Result.ErrorMessage = Message;
    return Result;
}

static FApplyJsonToUmgResult MakeApplySuccess(TArray<FString>&& Reparented = {})
{
    FApplyJsonToUmgResult Result;
    Result.bSuccess = true;
    Result.bApplied = true;
    Result.ReparentedWidgets = MoveTemp(Reparented);
    return Result;
}

struct FDeferredReparent
{
    FString WidgetName;
    FString ExpectedParentName;
};

static bool ExecuteReparent(UWidgetBlueprint* WidgetBlueprint, const FString& WidgetName, const FString& ExpectedParentName, FString& OutError)
{
    UWidget* WidgetToMove = WidgetBlueprint->WidgetTree->FindWidget(FName(*WidgetName));
    if (!WidgetToMove)
    {
        OutError = FString::Printf(TEXT("Widget '%s' not found for reparent."), *WidgetName);
        return false;
    }

    UPanelWidget* NewParent = Cast<UPanelWidget>(WidgetBlueprint->WidgetTree->FindWidget(FName(*ExpectedParentName)));
    if (!NewParent)
    {
        OutError = FString::Printf(TEXT("Expected parent '%s' not found for widget '%s'."), *ExpectedParentName, *WidgetName);
        return false;
    }

    if (WidgetToMove->GetParent() == NewParent)
    {
        return true;
    }

    WidgetBlueprint->Modify();
    if (UPanelWidget* OldParent = WidgetToMove->GetParent())
    {
        OldParent->RemoveChild(WidgetToMove);
    }
    NewParent->AddChild(WidgetToMove);
    return true;
}

static bool IsSingleChildPanelWidget(UWidget* Widget)
{
    if (!Widget)
    {
        return false;
    }
    const FName ClassName = Widget->GetClass()->GetFName();
    return ClassName == FName(TEXT("Button")) || ClassName == FName(TEXT("Border")) || ClassName == FName(TEXT("SizeBox"));
}

static UPanelWidget* EnsureSingleChildCapacity(UPanelWidget* ParentPanel, UWidgetTree* WidgetTree)
{
    if (!ParentPanel || !WidgetTree || !IsSingleChildPanelWidget(ParentPanel))
    {
        return ParentPanel;
    }

    if (ParentPanel->GetChildrenCount() == 0)
    {
        return ParentPanel;
    }

    if (UVerticalBox* ExistingVBox = Cast<UVerticalBox>(ParentPanel->GetChildAt(0)))
    {
        return ExistingVBox;
    }

    WidgetTree->Modify();
    ParentPanel->Modify();

    const FString WrapperName = ParentPanel->GetName() + TEXT("_Wrap");
    UVerticalBox* Wrapper = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), *WrapperName);
    if (!Wrapper)
    {
        UE_LOG(LogUmgMcp, Warning, TEXT("EnsureSingleChildCapacity: Failed to create wrapper for '%s'."), *ParentPanel->GetName());
        return ParentPanel;
    }

    TArray<UWidget*> ChildrenToMove;
    for (int32 Idx = 0; Idx < ParentPanel->GetChildrenCount(); ++Idx)
    {
        ChildrenToMove.Add(ParentPanel->GetChildAt(Idx));
    }

    for (UWidget* Child : ChildrenToMove)
    {
        ParentPanel->RemoveChild(Child);
        Wrapper->AddChild(Child);
    }

    ParentPanel->AddChild(Wrapper);
    UE_LOG(LogUmgMcp, Log, TEXT("EnsureSingleChildCapacity: Wrapped %d children of '%s' into '%s'."),
        ChildrenToMove.Num(), *ParentPanel->GetName(), *WrapperName);

    return Wrapper;
}

struct FJsonWidgetInfo
{
    FString Name;
    FString ClassPath;
    FString ParentName;
};

static void CollectJsonWidgetInfos(const TSharedPtr<FJsonObject>& JsonNode, const FString& ParentName, TMap<FString, FJsonWidgetInfo>& OutMap)
{
    if (!JsonNode.IsValid()) return;

    FJsonWidgetInfo Info;
    JsonNode->TryGetStringField(TEXT("widget_name"), Info.Name);
    JsonNode->TryGetStringField(TEXT("widget_class"), Info.ClassPath);
    Info.ParentName = ParentName;

    if (!Info.Name.IsEmpty())
    {
        OutMap.Add(Info.Name, Info);
    }

    const TArray<TSharedPtr<FJsonValue>>* ChildrenArray = nullptr;
    if (JsonNode->TryGetArrayField(TEXT("children"), ChildrenArray))
    {
        for (const auto& ChildVal : *ChildrenArray)
        {
            const TSharedPtr<FJsonObject>* ChildObj;
            if (ChildVal->TryGetObject(ChildObj))
            {
                CollectJsonWidgetInfos(*ChildObj, Info.Name, OutMap);
            }
        }
    }
}

FApplyJsonToUmgResult ApplyJsonToUmgAsset_GameThread(const FString& AssetPath, const FString& JsonData, const FString& TargetWidgetName);

TSharedPtr<FJsonObject> UUmgFileTransformation::NormalizeJsonKeysToPascalCase(const TSharedPtr<FJsonObject>& SourceJson)
{
    if (!SourceJson.IsValid())
    {
        return nullptr;
    }
    
    TSharedPtr<FJsonObject> NormalizedJson = MakeShared<FJsonObject>();
    
    for (const auto& Pair : SourceJson->Values)
    {
        FString OriginalKey(Pair.Key.ToView());
        FString NormalizedKey;

        // Handle dotted keys (e.g. "slot.position")
        if (OriginalKey.Contains(TEXT(".")))
        {
            TArray<FString> Parts;
            OriginalKey.ParseIntoArray(Parts, TEXT("."));
            for (int32 i = 0; i < Parts.Num(); ++i)
            {
                Parts[i] = NormalizePropertyName(Parts[i]);
            }
            NormalizedKey = FString::Join(Parts, TEXT("."));
        }
        else
        {
            NormalizedKey = NormalizePropertyName(OriginalKey);
        }

        if (NormalizedKey != OriginalKey)
        {
            UE_LOG(LogUmgMcp, Verbose, TEXT("NormalizeJsonKeys: '%s' → '%s'"), *OriginalKey, *NormalizedKey);
        }

        // Recursively process nested objects and arrays
        TSharedPtr<FJsonValue> Value = Pair.Value;
        if (Value->Type == EJson::Object)
        {
            TSharedPtr<FJsonObject> NestedObj = Value->AsObject();
            TSharedPtr<FJsonObject> NormalizedNestedObj = NormalizeJsonKeysToPascalCase(NestedObj);
            NormalizedJson->SetObjectField(NormalizedKey, NormalizedNestedObj);
        }
        else if (Value->Type == EJson::Array)
        {
            // Process objects within arrays
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
            // Primitive types: copy directly
            NormalizedJson->SetField(NormalizedKey, Value);
        }
    }
    
    return NormalizedJson;
}

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
                    if (TSharedPtr<FJsonObject> SlotPropertiesJson = FUmgMcpPropertyJsonUtils::SerializePanelSlotProperties(Widget->Slot))
                    {
                        PropertiesJson->SetObjectField(TEXT("Slot"), SlotPropertiesJson);
                    }
                }
                else
                {
                    TSharedPtr<FJsonValue> PropertyJsonValue = FUmgMcpPropertyJsonUtils::PropertyToJsonValue(Property, ValuePtr);
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

FString UUmgFileTransformation::ExportUmgAssetToJsonString(const FString& AssetPath, const FString& TargetWidgetName)
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

    UWidget* TargetWidget = nullptr;
    if (TargetWidgetName.IsEmpty() || TargetWidgetName.Equals(TEXT("Root"), ESearchCase::IgnoreCase))
    {
        TargetWidget = WidgetBlueprint->WidgetTree->RootWidget;
        if (!TargetWidget)
        {
            UE_LOG(LogUmgMcp, Warning, TEXT("ExportUmgAssetToJsonString: Root widget not found in UWidgetBlueprint '%s'. This may be an empty UI."), *AssetPath);
            return FString();
        }
    }
    else
    {
        TargetWidget = WidgetBlueprint->WidgetTree->FindWidget(FName(*TargetWidgetName));
        if (!TargetWidget)
        {
             UE_LOG(LogUmgMcp, Error, TEXT("ExportUmgAssetToJsonString: Target widget '%s' not found in '%s'."), *TargetWidgetName, *AssetPath);
             return FString();
        }
    }

    TSharedPtr<FJsonObject> RootJsonObject = ExportWidgetToJson(TargetWidget);

    if (!RootJsonObject.IsValid())
    {
        UE_LOG(LogUmgMcp, Error, TEXT("ExportUmgAssetToJsonString: Failed to convert widget '%s' of '%s' to FJsonObject."), *TargetWidget->GetName(), *AssetPath);
        return FString();
    }

    FString JsonString;
    TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> JsonWriter = TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&JsonString);
    
    if (FJsonSerializer::Serialize(RootJsonObject.ToSharedRef(), JsonWriter))
    {
        JsonWriter->Close();
        UE_LOG(LogUmgMcp, Log, TEXT("Successfully exported UMG asset '%s' (Target: %s) to JSON."), *AssetPath, *TargetWidget->GetName());
        return JsonString;
    }
    else
    {
        JsonWriter->Close();
        UE_LOG(LogUmgMcp, Error, TEXT("ExportUmgAssetToJsonString: Failed to serialize FJsonObject to string for asset '%s'."), *AssetPath);
        return FString();
    }
}

FApplyJsonToUmgResult UUmgFileTransformation::ApplyJsonStringToUmgAsset(const FString& AssetPath, const FString& JsonData, const FString& TargetWidgetName)
{
    if (IsInGameThread())
    {
        return ApplyJsonToUmgAsset_GameThread(AssetPath, JsonData, TargetWidgetName);
    }

    TPromise<FApplyJsonToUmgResult> Promise;
    TFuture<FApplyJsonToUmgResult> Future = Promise.GetFuture();

    AsyncTask(ENamedThreads::GameThread, [AssetPath, JsonData, TargetWidgetName, Promise = MoveTemp(Promise)]() mutable
    {
        Promise.SetValue(ApplyJsonToUmgAsset_GameThread(AssetPath, JsonData, TargetWidgetName));
    });

    if (Future.WaitFor(FTimespan::FromSeconds(60.0)))
    {
        return Future.Get();
    }

    return MakeApplyError(TEXT("Apply JSON timed out waiting for GameThread (60s)."));
}


// This function is executed on the game thread to ensure thread safety when dealing with UObjects.
FApplyJsonToUmgResult ApplyJsonToUmgAsset_GameThread(const FString& AssetPath, const FString& JsonData, const FString& TargetWidgetName)
{
    TArray<FString> ReparentedWidgets;
    TArray<FDeferredReparent> PendingReparents;

    // 0. Handle default workspace: if AssetPath is empty, try to get target from attention subsystem
    FString FinalAssetPath = AssetPath;
    if (FinalAssetPath.IsEmpty() || FinalAssetPath.TrimStartAndEnd().IsEmpty())
    {
        if (GEditor)
        {
            if (UUmgAttentionSubsystem* AttentionSubsystem = GEditor->GetEditorSubsystem<UUmgAttentionSubsystem>())
            {
                FinalAssetPath = AttentionSubsystem->GetTargetUmgAsset();
            }
        }

        if (FinalAssetPath.IsEmpty() || FinalAssetPath.TrimStartAndEnd().IsEmpty())
        {
            FinalAssetPath = TEXT("/Game/UnrealMotionGraphicsMCP.UnrealMotionGraphicsMCP");
            UE_LOG(LogUmgMcp, Log, TEXT("ApplyJsonToUmgAsset_GameThread: No asset path provided or active target found. Using default workspace: '%s'."), *FinalAssetPath);
        }
        else
        {
            UE_LOG(LogUmgMcp, Log, TEXT("ApplyJsonToUmgAsset_GameThread: No asset path provided. Using active target asset: '%s'."), *FinalAssetPath);
        }
    }
    
    UE_LOG(LogUmgMcp, Log, TEXT("ApplyJsonToUmgAsset_GameThread: Starting for asset '%s'."), *FinalAssetPath);

    // 1. Parse the incoming JSON data.
    UE_LOG(LogUmgMcp, Log, TEXT("ApplyJsonToUmgAsset_GameThread: Parsing JSON data."));
    TSharedPtr<FJsonObject> RootJsonObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonData);
    if (!FJsonSerializer::Deserialize(Reader, RootJsonObject) || !RootJsonObject.IsValid())
    {
        UE_LOG(LogUmgMcp, Error, TEXT("ApplyJsonToUmgAsset_GameThread: Failed to parse JSON data."));
        return MakeApplyError(TEXT("Failed to parse JSON data."));
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
                return MakeApplyError(FString::Printf(TEXT("Failed to create package at '%s'."), *PackagePath));
            }

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
                return MakeApplyError(FString::Printf(TEXT("Failed to create new Widget Blueprint at '%s'."), *FinalAssetPath));
            }

            bIsNewlyCreated = true;

            Package->MarkPackageDirty();
            FAssetRegistryModule::AssetCreated(WidgetBlueprint);

            UE_LOG(LogUmgMcp, Log, TEXT("ApplyJsonToUmgAsset_GameThread: New Widget Blueprint created at '%s'."), *FinalAssetPath);
        }
        else
        {
            UE_LOG(LogUmgMcp, Error, TEXT("ApplyJsonToUmgAsset_GameThread: Invalid asset path format '%s'."), *FinalAssetPath);
            return MakeApplyError(FString::Printf(TEXT("Invalid asset path format '%s'."), *FinalAssetPath));
        }
    }
    else
    {
        UE_LOG(LogUmgMcp, Log, TEXT("ApplyJsonToUmgAsset_GameThread: Widget Blueprint loaded."));
    }

    if (!WidgetBlueprint)
    {
        return MakeApplyError(TEXT("Widget Blueprint is null after load/create."));
    }

    // 3. Ensure the WidgetTree is valid.
    if (!WidgetBlueprint->WidgetTree)
    {
        UE_LOG(LogUmgMcp, Error, TEXT("ApplyJsonToUmgAsset_GameThread: WidgetTree is null in UWidgetBlueprint '%s'."), *FinalAssetPath);
        return MakeApplyError(FString::Printf(TEXT("WidgetTree is null in '%s'."), *FinalAssetPath));
    }
    UE_LOG(LogUmgMcp, Log, TEXT("ApplyJsonToUmgAsset_GameThread: WidgetTree is valid."));

    // 4. Mark the blueprint as modified so the editor knows to save it.
    UE_LOG(LogUmgMcp, Log, TEXT("ApplyJsonToUmgAsset_GameThread: Marking blueprint as modified."));
    WidgetBlueprint->Modify();
    UE_LOG(LogUmgMcp, Log, TEXT("ApplyJsonToUmgAsset_GameThread: Blueprint marked as modified."));

    // 5. Clear the existing widget tree OR find target widget
    UE_LOG(LogUmgMcp, Log, TEXT("ApplyJsonToUmgAsset_GameThread: Preparing to apply JSON. Target: %s"), *TargetWidgetName);

    UWidget* TargetWidget = nullptr;
    FString ResolvedTargetName = TargetWidgetName;

    if (ResolvedTargetName.IsEmpty() || ResolvedTargetName.Equals(TEXT("Root"), ESearchCase::IgnoreCase) || ResolvedTargetName.Equals(TEXT("None"), ESearchCase::IgnoreCase))
    {
        if (GEditor)
        {
            if (UUmgAttentionSubsystem* AttentionSubsystem = GEditor->GetEditorSubsystem<UUmgAttentionSubsystem>())
            {
                ResolvedTargetName = AttentionSubsystem->GetTargetWidget();
            }
        }
    }

    if (!ResolvedTargetName.IsEmpty() && !ResolvedTargetName.Equals(TEXT("Root"), ESearchCase::IgnoreCase) && !ResolvedTargetName.Equals(TEXT("None"), ESearchCase::IgnoreCase))
    {
        TargetWidget = WidgetBlueprint->WidgetTree->FindWidget(FName(*ResolvedTargetName));
    }

    if (!TargetWidget)
    {
        TargetWidget = WidgetBlueprint->WidgetTree->RootWidget;
    }

    // Collect JSON widgets and validate structure consistency
    TMap<FString, FJsonWidgetInfo> JsonWidgets;
    CollectJsonWidgetInfos(RootJsonObject, TargetWidget ? TargetWidget->GetName() : TEXT("Root"), JsonWidgets);

    bool bHasOverlap = false;
    for (const auto& Pair : JsonWidgets)
    {
        const FString& WidgetName = Pair.Key;
        const FJsonWidgetInfo& JsonInfo = Pair.Value;

        UWidget* ExistingWidget = WidgetBlueprint->WidgetTree->FindWidget(FName(*WidgetName));
        if (ExistingWidget)
        {
            bHasOverlap = true;

            // Class matching check
            FString ExistingClassPath = ExistingWidget->GetClass()->GetPathName();
            if (!JsonInfo.ClassPath.IsEmpty() && !JsonInfo.ClassPath.Equals(ExistingClassPath, ESearchCase::IgnoreCase))
            {
                UE_LOG(LogUmgMcp, Error, TEXT("ApplyJsonToUmgAsset_GameThread: Class mismatch for widget '%s' (JSON: %s, Existing: %s)."),
                    *WidgetName, *JsonInfo.ClassPath, *ExistingClassPath);
                return MakeApplyError(FString::Printf(
                    TEXT("Class mismatch for widget '%s' (JSON: %s, Existing: %s)."),
                    *WidgetName, *JsonInfo.ClassPath, *ExistingClassPath));
            }

            // Parent matching check
            UWidget* ExistingParent = ExistingWidget->GetParent();
            FString ExistingParentName = ExistingParent ? ExistingParent->GetName() : TEXT("Root");
            FString ExpectedParentName = JsonInfo.ParentName;

            bool bParentMatches = false;
            if (ExpectedParentName.Equals(ExistingParentName, ESearchCase::IgnoreCase))
            {
                bParentMatches = true;
            }
            else if (ExpectedParentName.Equals(TEXT("Root"), ESearchCase::IgnoreCase) && !ExistingParent)
            {
                bParentMatches = true;
            }
            else if (TargetWidget && ExpectedParentName.Equals(TargetWidget->GetName(), ESearchCase::IgnoreCase) && ExistingParent == TargetWidget)
            {
                bParentMatches = true;
            }

            if (!bParentMatches)
            {
                UE_LOG(LogUmgMcp, Log, TEXT("ApplyJsonToUmgAsset_GameThread: Deferring reparent for '%s' (JSON parent: %s, UE parent: %s)."),
                    *WidgetName, *ExpectedParentName, *ExistingParentName);
                PendingReparents.Add({WidgetName, ExpectedParentName});
            }
        }
    }

    if (!bHasOverlap)
    {
        // Unique names: apply_layout occurs on TargetWidget (which defaults to RootWidget)
        if (!TargetWidget)
        {
            UE_LOG(LogUmgMcp, Log, TEXT("ApplyJsonToUmgAsset_GameThread: Unique names and no root. Creating root widget."));
            UWidget* NewRootWidget = CreateWidgetFromJson(RootJsonObject, WidgetBlueprint->WidgetTree, nullptr);
            if (!NewRootWidget)
            {
                UE_LOG(LogUmgMcp, Error, TEXT("ApplyJsonToUmgAsset_GameThread: Failed to create root widget."));
                return MakeApplyError(TEXT("Failed to create root widget."));
            }
            WidgetBlueprint->WidgetTree->RootWidget = NewRootWidget;
        }
        else
        {
            UE_LOG(LogUmgMcp, Log, TEXT("ApplyJsonToUmgAsset_GameThread: Unique names. Creating subtree under TargetWidget '%s'."), *TargetWidget->GetName());
            UWidget* NewSubtree = CreateWidgetFromJson(RootJsonObject, WidgetBlueprint->WidgetTree, TargetWidget);
            if (!NewSubtree)
            {
                UE_LOG(LogUmgMcp, Error, TEXT("ApplyJsonToUmgAsset_GameThread: Failed to create subtree under TargetWidget '%s'."), *TargetWidget->GetName());
                return MakeApplyError(FString::Printf(TEXT("Failed to create subtree under '%s'."), *TargetWidget->GetName()));
            }
        }
    }
    else
    {
        // Overlap: incremental tree merging
        if (TargetWidget)
        {
            UpsertWidgetFromJsonAppendOnly(RootJsonObject, WidgetBlueprint->WidgetTree, TargetWidget);
        }
        else
        {
            // Fallback for empty tree (should not happen since we have overlap)
            UWidget* NewRootWidget = CreateWidgetFromJson(RootJsonObject, WidgetBlueprint->WidgetTree, nullptr);
            if (!NewRootWidget) return MakeApplyError(TEXT("Failed to create root widget (overlap fallback)."));
            WidgetBlueprint->WidgetTree->RootWidget = NewRootWidget;
        }
    }

    for (const FDeferredReparent& Item : PendingReparents)
    {
        FString ReparentError;
        if (!ExecuteReparent(WidgetBlueprint, Item.WidgetName, Item.ExpectedParentName, ReparentError))
        {
            return MakeApplyError(ReparentError);
        }
        ReparentedWidgets.Add(Item.WidgetName);
    }

    // 7.5. Verify widget tree integrity before marking as modified
    UE_LOG(LogUmgMcp, Log, TEXT("ApplyJsonToUmgAsset_GameThread: Verifying widget tree integrity."));
    if (!WidgetBlueprint->WidgetTree->RootWidget)
    {
        UE_LOG(LogUmgMcp, Error, TEXT("ApplyJsonToUmgAsset_GameThread: Root widget became null after assignment!"));
        return MakeApplyError(TEXT("Root widget became null after assignment."));
    }

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
    
    // 9. Refresh GeneratedClass so preview/lint see the new tree.
    // We intentionally avoid MarkBlueprintAsStructurallyModified here: the original author
    // documented it as crash-prone in this code path. A plain CompileBlueprint on the
    // GameThread is sufficient to regenerate the class without that risk.
    FUmgPreviewRenderUtils::CompileWidgetBlueprint(WidgetBlueprint);
    
    UE_LOG(LogUmgMcp, Log, TEXT("Successfully applied JSON to UMG asset '%s'."), *FinalAssetPath);

    return MakeApplySuccess(MoveTemp(ReparentedWidgets));
}

static void ApplyPropertiesToExistingWidget(const TSharedPtr<FJsonObject>& WidgetJson, UWidget* TargetWidget)
{
    if (!WidgetJson.IsValid() || !TargetWidget)
    {
        return;
    }

    const TSharedPtr<FJsonObject>* PropertiesJsonObjPtr;
    TSharedPtr<FJsonObject> WidgetProps = MakeShared<FJsonObject>();
    TSharedPtr<FJsonObject> SlotProps = ExtractSlotPropsFromWidgetJson(WidgetJson);

    if (WidgetJson->TryGetObjectField(TEXT("properties"), PropertiesJsonObjPtr))
    {
        TSharedPtr<FJsonObject> SourceProps = *PropertiesJsonObjPtr;
        for (auto& Pair : SourceProps->Values)
        {
            const FString Key(Pair.Key.ToView());
            if (Key != TEXT("Slot"))
            {
                WidgetProps->SetField(Key, Pair.Value);
            }
        }
    }

    if (WidgetProps->Values.Num() > 0)
    {
        TargetWidget->Modify();
        if (!FJsonObjectConverter::JsonObjectToUStruct(WidgetProps.ToSharedRef(), TargetWidget->GetClass(), TargetWidget, 0, 0))
        {
            UE_LOG(LogUmgMcp, Warning, TEXT("ApplyPropertiesToExistingWidget: Failed to apply properties to '%s'."), *TargetWidget->GetName());
        }
    }

    if (SlotProps.IsValid())
    {
        ApplySlotPropertiesToWidget(TargetWidget, SlotProps);
    }
}

static void MergeChildrenAppendOnly(const TSharedPtr<FJsonObject>& WidgetJson, UWidgetTree* WidgetTree, UWidget* TargetWidget)
{
    if (!WidgetJson.IsValid() || !WidgetTree || !TargetWidget)
    {
        return;
    }

    UPanelWidget* ParentPanel = Cast<UPanelWidget>(TargetWidget);
    if (!ParentPanel)
    {
        return;
    }

    const TArray<TSharedPtr<FJsonValue>>* ChildrenJsonArray;
    if (!WidgetJson->TryGetArrayField(TEXT("children"), ChildrenJsonArray))
    {
        return;
    }

    TMap<FString, UWidget*> ExistingChildren;
    for (int32 Index = 0; Index < ParentPanel->GetChildrenCount(); ++Index)
    {
        if (UWidget* Child = ParentPanel->GetChildAt(Index))
        {
            ExistingChildren.Add(Child->GetName(), Child);
        }
    }

    int32 InsertIndex = 0;
    for (const TSharedPtr<FJsonValue>& ChildValue : *ChildrenJsonArray)
    {
        const TSharedPtr<FJsonObject>* ChildObject;
        if (!ChildValue->TryGetObject(ChildObject))
        {
            continue;
        }

        FString ChildName;
        (*ChildObject)->TryGetStringField(TEXT("widget_name"), ChildName);

        if (UWidget** ExistingPtr = ChildName.IsEmpty() ? nullptr : ExistingChildren.Find(ChildName))
        {
            UpsertWidgetFromJsonAppendOnly(*ChildObject, WidgetTree, *ExistingPtr);
            const int32 CurrentIndex = ParentPanel->GetChildIndex(*ExistingPtr);
            InsertIndex = FMath::Max(InsertIndex, CurrentIndex + 1);
            ExistingChildren.Remove(ChildName);
        }
        else
        {
            UPanelWidget* EffectiveParent = EnsureSingleChildCapacity(ParentPanel, WidgetTree);
            UWidget* NewChild = CreateWidgetFromJson(*ChildObject, WidgetTree, EffectiveParent);
            if (NewChild && EffectiveParent == ParentPanel)
            {
                const int32 CurrentIndex = ParentPanel->GetChildIndex(NewChild);
                if (CurrentIndex != InsertIndex && CurrentIndex != INDEX_NONE)
                {
                    ParentPanel->InsertChildAt(InsertIndex, NewChild);
                }
                InsertIndex = ParentPanel->GetChildIndex(NewChild) + 1;
            }
        }
    }
}

static void UpsertWidgetFromJsonAppendOnly(const TSharedPtr<FJsonObject>& WidgetJson, UWidgetTree* WidgetTree, UWidget* TargetWidget)
{
    if (!WidgetJson.IsValid() || !WidgetTree || !TargetWidget)
    {
        return;
    }

    FString DeclaredName;
    WidgetJson->TryGetStringField(TEXT("widget_name"), DeclaredName);
    if (!DeclaredName.IsEmpty() && DeclaredName != TargetWidget->GetName())
    {
        UE_LOG(LogUmgMcp, Warning, TEXT("UpsertWidgetFromJsonAppendOnly: JSON widget '%s' mapped to existing widget '%s'. Keeping existing instance to avoid implicit deletion."), *DeclaredName, *TargetWidget->GetName());
    }

    FString DeclaredClass;
    WidgetJson->TryGetStringField(TEXT("widget_class"), DeclaredClass);
    if (!DeclaredClass.IsEmpty() && DeclaredClass != TargetWidget->GetClass()->GetPathName())
    {
        UE_LOG(LogUmgMcp, Warning, TEXT("UpsertWidgetFromJsonAppendOnly: Class mismatch for widget '%s' (JSON: %s, Existing: %s). Preserving existing class."), *TargetWidget->GetName(), *DeclaredClass, *TargetWidget->GetClass()->GetPathName());
    }

    ApplyPropertiesToExistingWidget(WidgetJson, TargetWidget);
    MergeChildrenAppendOnly(WidgetJson, WidgetTree, TargetWidget);
}

static UWidget* CreateWidgetFromJson(const TSharedPtr<FJsonObject>& WidgetJson, UWidgetTree* WidgetTree, UWidget* ParentWidget)
{
    FString ParentName = ParentWidget ? ParentWidget->GetName() : TEXT("None");
    // UE_LOG(LogUmgMcp, Log, TEXT("CreateWidgetFromJson: Starting for Parent: %s"), *ParentName);

    if (!WidgetJson.IsValid() || !WidgetTree)
    {
        UE_LOG(LogUmgMcp, Error, TEXT("CreateWidgetFromJson: Invalid WidgetJson or WidgetTree."));
        return nullptr;
    }

    FString WidgetClassPath, WidgetName;
    if (!WidgetJson->TryGetStringField(TEXT("widget_class"), WidgetClassPath) || !WidgetJson->TryGetStringField(TEXT("widget_name"), WidgetName))
    {
        UE_LOG(LogUmgMcp, Error, TEXT("CreateWidgetFromJson: JSON is missing widget_class or widget_name."));
        return nullptr;
    }

    UClass* WidgetClass = StaticLoadClass(UWidget::StaticClass(), nullptr, *WidgetClassPath);
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

    // 1. Add to parent to create the Slot
    UPanelSlot* NewSlot = nullptr;
    if (ParentWidget)
    {
        UPanelWidget* ParentPanel = Cast<UPanelWidget>(ParentWidget);
        if (ParentPanel)
        {
            NewSlot = ParentPanel->AddChild(NewWidget);
            if (!NewSlot)
            {
                UE_LOG(LogUmgMcp, Warning, TEXT("CreateWidgetFromJson: AddChild returned null slot for '%s' in '%s'."), *WidgetName, *ParentName);
            }
        }
        else
        {
            UE_LOG(LogUmgMcp, Warning, TEXT("CreateWidgetFromJson: Parent '%s' is not a UPanelWidget, cannot add child '%s'."), *ParentName, *WidgetName);
        }
    }

    // 2. Prepare Property JSONs
    const TSharedPtr<FJsonObject>* PropertiesJsonObjPtr;
    TSharedPtr<FJsonObject> WidgetProps = MakeShared<FJsonObject>();
    TSharedPtr<FJsonObject> SlotProps = ExtractSlotPropsFromWidgetJson(WidgetJson);

    if (WidgetJson->TryGetObjectField(TEXT("properties"), PropertiesJsonObjPtr))
    {
        TSharedPtr<FJsonObject> SourceProps = *PropertiesJsonObjPtr;
        for (auto& Pair : SourceProps->Values)
        {
            const FString Key(Pair.Key.ToView());
            if (Key != TEXT("Slot"))
            {
                WidgetProps->SetField(Key, Pair.Value);
            }
        }
    }

    // 3. Apply Widget Properties using JsonObjectToUStruct (Safe & Robust)
    if (WidgetProps->Values.Num() > 0)
    {
        if (!FJsonObjectConverter::JsonObjectToUStruct(WidgetProps.ToSharedRef(), NewWidget->GetClass(), NewWidget, 0, 0))
        {
             UE_LOG(LogUmgMcp, Warning, TEXT("CreateWidgetFromJson: Issues applying properties to '%s'."), *WidgetName);
        }
    }

    // 4. Apply Slot Properties (properties.Slot or top-level SlotData)
    if (SlotProps.IsValid())
    {
        UE_LOG(LogUmgMcp, Log, TEXT("CreateWidgetFromJson: Processing Slot properties for widget '%s'"), *WidgetName);
        if (NewSlot)
        {
            ApplySlotPropertiesToWidget(NewWidget, SlotProps);
        }
        else
        {
            UE_LOG(LogUmgMcp, Warning, TEXT("CreateWidgetFromJson: Slot properties specified but no valid Slot found for '%s'."), *WidgetName);
        }
    }

    // 5. Process Children
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
