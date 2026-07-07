// Copyright (c) 2025-2026 Winyunq. All rights reserved.

#include "Widget/UmgSetSubsystem.h"
#include "Bridge/UmgMcpJsonCompat.h"
#include "FileManage/UmgAttentionSubsystem.h"
#include "Editor.h"
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetTree.h"
#include "Components/PanelWidget.h"
#include "JsonObjectConverter.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "FileHelpers.h"
#include "FileManage/UmgFileTransformation.h"
#include "Components/CanvasPanelSlot.h"

DEFINE_LOG_CATEGORY(LogUmgSet);

namespace
{
    struct FWidgetOrderNode
    {
        FString Name;
        TArray<FWidgetOrderNode> Children;
    };

    static bool GetStringFieldAny(const TSharedPtr<FJsonObject>& Object, const TArray<FString>& Keys, FString& OutValue)
    {
        if (!Object.IsValid())
        {
            return false;
        }

        for (const FString& Key : Keys)
        {
            if (Object->TryGetStringField(Key, OutValue))
            {
                OutValue.TrimStartAndEndInline();
                return !OutValue.IsEmpty();
            }
        }

        return false;
    }

    static bool ParseOrderNodeValue(const TSharedPtr<FJsonValue>& Value, FWidgetOrderNode& OutNode, FString& OutError);

    static bool ParseOrderNodeArray(const TArray<TSharedPtr<FJsonValue>>& Values, TArray<FWidgetOrderNode>& OutNodes, FString& OutError)
    {
        for (const TSharedPtr<FJsonValue>& ChildValue : Values)
        {
            FWidgetOrderNode ChildNode;
            if (!ParseOrderNodeValue(ChildValue, ChildNode, OutError))
            {
                return false;
            }

            OutNodes.Add(MoveTemp(ChildNode));
        }

        return true;
    }

    static bool TryGetChildrenArray(const TSharedPtr<FJsonObject>& Object, const TArray<TSharedPtr<FJsonValue>>*& OutChildren)
    {
        return Object.IsValid() &&
            (Object->TryGetArrayField(TEXT("children"), OutChildren) ||
             Object->TryGetArrayField(TEXT("widgets"), OutChildren) ||
             Object->TryGetArrayField(TEXT("tree"), OutChildren));
    }

    static bool ParseOrderNodeObject(const TSharedPtr<FJsonObject>& Object, FWidgetOrderNode& OutNode, FString& OutError)
    {
        GetStringFieldAny(Object, {TEXT("name"), TEXT("widget_name"), TEXT("widget"), TEXT("root")}, OutNode.Name);

        const TArray<TSharedPtr<FJsonValue>>* Children = nullptr;
        if (TryGetChildrenArray(Object, Children))
        {
            if (!ParseOrderNodeArray(*Children, OutNode.Children, OutError))
            {
                return false;
            }
        }

        if (OutNode.Name.IsEmpty() && OutNode.Children.Num() == 0)
        {
            OutError = TEXT("Tree object must contain a widget name or children array.");
            return false;
        }

        return true;
    }

    static bool ParseOrderNodeValue(const TSharedPtr<FJsonValue>& Value, FWidgetOrderNode& OutNode, FString& OutError)
    {
        if (!Value.IsValid())
        {
            OutError = TEXT("Tree contains an invalid/null entry.");
            return false;
        }

        if (Value->Type == EJson::String)
        {
            OutNode.Name = Value->AsString();
            OutNode.Name.TrimStartAndEndInline();
            if (OutNode.Name.IsEmpty())
            {
                OutError = TEXT("Tree contains an empty widget name.");
                return false;
            }
            return true;
        }

        if (Value->Type == EJson::Object)
        {
            return ParseOrderNodeObject(Value->AsObject(), OutNode, OutError);
        }

        OutError = TEXT("Tree entries must be widget-name strings or objects with name/children.");
        return false;
    }

    static FString NormalizeTreeLine(const FString& RawLine)
    {
        FString Line = RawLine.TrimStartAndEnd();
        while (Line.StartsWith(TEXT("-")) || Line.StartsWith(TEXT("*")))
        {
            Line.RightChopInline(1);
            Line.TrimStartAndEndInline();
        }
        return Line;
    }

    static int32 CountLeadingIndent(const FString& RawLine)
    {
        int32 Indent = 0;
        for (TCHAR Ch : RawLine)
        {
            if (Ch == TEXT(' '))
            {
                ++Indent;
            }
            else if (Ch == TEXT('\t'))
            {
                Indent += 4;
            }
            else
            {
                break;
            }
        }
        return Indent;
    }

    struct FTreeLine
    {
        int32 Indent = 0;
        FString Name;
    };

    static bool ParseTextTree(const FString& Text, const FString& RootName, FWidgetOrderNode& OutRoot, FString& OutError)
    {
        TArray<FString> RawLines;
        Text.Replace(TEXT("\r"), TEXT("")).ParseIntoArrayLines(RawLines, true);

        TArray<FTreeLine> Lines;
        for (const FString& RawLine : RawLines)
        {
            FString Name = NormalizeTreeLine(RawLine);
            if (!Name.IsEmpty())
            {
                FTreeLine& Line = Lines.AddDefaulted_GetRef();
                Line.Indent = CountLeadingIndent(RawLine);
                Line.Name = MoveTemp(Name);
            }
        }

        if (Lines.Num() == 0)
        {
            OutError = TEXT("Tree text is empty.");
            return false;
        }

        int32 StartIndex = 0;
        int32 RootIndent = -1;
        if (!RootName.IsEmpty())
        {
            OutRoot.Name = RootName;
            if (Lines[0].Name.Equals(RootName, ESearchCase::IgnoreCase))
            {
                RootIndent = Lines[0].Indent;
                StartIndex = 1;
            }
        }
        else
        {
            OutRoot.Name = Lines[0].Name;
            RootIndent = Lines[0].Indent;
            StartIndex = 1;
        }

        if (OutRoot.Name.IsEmpty())
        {
            OutError = TEXT("Tree root is empty. Provide root or a first tree line.");
            return false;
        }

        TArray<TPair<int32, FWidgetOrderNode*>> Stack;
        Stack.Add(TPair<int32, FWidgetOrderNode*>(RootIndent, &OutRoot));

        for (int32 Index = StartIndex; Index < Lines.Num(); ++Index)
        {
            const FTreeLine& Line = Lines[Index];
            while (Stack.Num() > 1 && Line.Indent <= Stack.Last().Key)
            {
                Stack.Pop();
            }

            FWidgetOrderNode* ParentNode = Stack.Last().Value;
            FWidgetOrderNode& NewNode = ParentNode->Children.AddDefaulted_GetRef();
            NewNode.Name = Line.Name;
            Stack.Add(TPair<int32, FWidgetOrderNode*>(Line.Indent, &NewNode));
        }

        return true;
    }

    static bool BuildOrderRoot(const FString& TreeSpec, const FString& RootName, FWidgetOrderNode& OutRoot, FString& OutError)
    {
        TSharedPtr<FJsonValue> ParsedValue;
        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(TreeSpec);
        if (FJsonSerializer::Deserialize(Reader, ParsedValue) && ParsedValue.IsValid())
        {
            if (ParsedValue->Type == EJson::Array)
            {
                OutRoot.Name = RootName;
                return ParseOrderNodeArray(ParsedValue->AsArray(), OutRoot.Children, OutError);
            }

            if (ParsedValue->Type == EJson::Object)
            {
                FWidgetOrderNode ParsedNode;
                if (!ParseOrderNodeObject(ParsedValue->AsObject(), ParsedNode, OutError))
                {
                    return false;
                }

                if (!RootName.IsEmpty() && ParsedNode.Name.IsEmpty())
                {
                    OutRoot.Name = RootName;
                    OutRoot.Children = MoveTemp(ParsedNode.Children);
                }
                else if (RootName.IsEmpty() || ParsedNode.Name.Equals(RootName, ESearchCase::IgnoreCase))
                {
                    OutRoot = MoveTemp(ParsedNode);
                }
                else
                {
                    OutRoot.Name = RootName;
                    OutRoot.Children.Add(MoveTemp(ParsedNode));
                }
                return true;
            }

            if (ParsedValue->Type == EJson::String)
            {
                return ParseTextTree(ParsedValue->AsString(), RootName, OutRoot, OutError);
            }

            OutError = TEXT("Tree must be an array, object, string, or outline text.");
            return false;
        }

        return ParseTextTree(TreeSpec, RootName, OutRoot, OutError);
    }

    static UPanelWidget* ResolvePanelByName(UWidgetBlueprint* WidgetBlueprint, const FString& PanelName, FString& OutError)
    {
        if (!WidgetBlueprint || !WidgetBlueprint->WidgetTree)
        {
            OutError = TEXT("Widget blueprint or widget tree is invalid.");
            return nullptr;
        }

        UWidget* Widget = nullptr;
        if (!PanelName.IsEmpty())
        {
            Widget = WidgetBlueprint->WidgetTree->FindWidget(FName(*PanelName));
        }

        if (!Widget && (PanelName.IsEmpty() || PanelName.Equals(TEXT("root"), ESearchCase::IgnoreCase)))
        {
            Widget = WidgetBlueprint->WidgetTree->RootWidget;
        }

        if (!Widget)
        {
            OutError = FString::Printf(TEXT("Tree parent '%s' was not found."), *PanelName);
            return nullptr;
        }

        UPanelWidget* Panel = Cast<UPanelWidget>(Widget);
        if (!Panel)
        {
            OutError = FString::Printf(TEXT("Tree parent '%s' is not a panel widget."), *Widget->GetName());
            return nullptr;
        }

        return Panel;
    }

    static bool EnsureSourceWidgetGuids(UWidgetBlueprint* WidgetBlueprint)
    {
#if WITH_EDITORONLY_DATA
        if (!WidgetBlueprint)
        {
            return false;
        }

        bool bModifiedBlueprint = false;
        bool bChanged = false;
        TSet<FName> SourceWidgetNames;
        WidgetBlueprint->ForEachSourceWidget([WidgetBlueprint, &bModifiedBlueprint, &bChanged, &SourceWidgetNames](UWidget* Widget)
        {
            if (!Widget)
            {
                return;
            }

            const FName WidgetName = Widget->GetFName();
            if (WidgetName.IsNone())
            {
                return;
            }

            SourceWidgetNames.Add(WidgetName);
            FGuid* ExistingGuid = WidgetBlueprint->WidgetVariableNameToGuidMap.Find(WidgetName);
            if (!ExistingGuid || !ExistingGuid->IsValid())
            {
                if (!bModifiedBlueprint)
                {
                    WidgetBlueprint->Modify();
                    bModifiedBlueprint = true;
                }

                WidgetBlueprint->WidgetVariableNameToGuidMap.Add(WidgetName, FGuid::NewGuid());
                bChanged = true;
                UE_LOG(LogUmgSet, Verbose, TEXT("Ensured GUID for source widget '%s' in '%s'."), *WidgetName.ToString(), *WidgetBlueprint->GetPathName());
            }
        });

        TArray<FName> StaleNames;
        for (const TPair<FName, FGuid>& Pair : WidgetBlueprint->WidgetVariableNameToGuidMap)
        {
            if (!SourceWidgetNames.Contains(Pair.Key))
            {
                StaleNames.Add(Pair.Key);
            }
        }

        for (const FName& StaleName : StaleNames)
        {
            if (!bModifiedBlueprint)
            {
                WidgetBlueprint->Modify();
                bModifiedBlueprint = true;
            }

            WidgetBlueprint->WidgetVariableNameToGuidMap.Remove(StaleName);
            bChanged = true;
            UE_LOG(LogUmgSet, Log, TEXT("Removed stale source widget GUID '%s' from '%s'."), *StaleName.ToString(), *WidgetBlueprint->GetPathName());
        }

        return bChanged;
#else
        return false;
#endif
    }

    static UWidget* FindDirectChildByName(UPanelWidget* ParentPanel, const FString& ChildName, int32& OutIndex)
    {
        OutIndex = INDEX_NONE;
        if (!ParentPanel)
        {
            return nullptr;
        }

        for (int32 Index = 0; Index < ParentPanel->GetChildrenCount(); ++Index)
        {
            UWidget* Child = ParentPanel->GetChildAt(Index);
            if (Child && Child->GetName().Equals(ChildName, ESearchCase::IgnoreCase))
            {
                OutIndex = Index;
                return Child;
            }
        }

        return nullptr;
    }

    static void ApplyOrderNode(UPanelWidget* ParentPanel, const FWidgetOrderNode& ParentOrder, TArray<FString>& OutReorderedWidgets, TArray<FString>& OutWarnings)
    {
        if (!ParentPanel)
        {
            return;
        }

        TSet<FString> SeenNames;
        int32 TargetIndex = 0;
        for (const FWidgetOrderNode& ChildOrder : ParentOrder.Children)
        {
            if (ChildOrder.Name.IsEmpty())
            {
                continue;
            }

            const FString Key = ChildOrder.Name.ToLower();
            if (SeenNames.Contains(Key))
            {
                OutWarnings.Add(FString::Printf(TEXT("Duplicate child '%s' under '%s' ignored."), *ChildOrder.Name, *ParentPanel->GetName()));
                continue;
            }
            SeenNames.Add(Key);

            int32 CurrentIndex = INDEX_NONE;
            UWidget* Child = FindDirectChildByName(ParentPanel, ChildOrder.Name, CurrentIndex);
            if (!Child)
            {
                OutWarnings.Add(FString::Printf(TEXT("Child '%s' is not a direct child of '%s'; skipped."), *ChildOrder.Name, *ParentPanel->GetName()));
                continue;
            }

            if (CurrentIndex != TargetIndex)
            {
                ParentPanel->Modify();
                Child->Modify();
                if (Child->Slot)
                {
                    Child->Slot->Modify();
                }
                ParentPanel->ShiftChild(TargetIndex, Child);
                OutReorderedWidgets.Add(FString::Printf(TEXT("%s -> %s[%d]"), *Child->GetName(), *ParentPanel->GetName(), TargetIndex));
            }

            ++TargetIndex;
        }

        for (const FWidgetOrderNode& ChildOrder : ParentOrder.Children)
        {
            if (ChildOrder.Children.Num() == 0)
            {
                continue;
            }

            int32 ChildIndex = INDEX_NONE;
            UWidget* Child = FindDirectChildByName(ParentPanel, ChildOrder.Name, ChildIndex);
            UPanelWidget* ChildPanel = Cast<UPanelWidget>(Child);
            if (!ChildPanel)
            {
                OutWarnings.Add(FString::Printf(TEXT("Child '%s' under '%s' has nested order entries but is not a panel."), *ChildOrder.Name, *ParentPanel->GetName()));
                continue;
            }

            ApplyOrderNode(ChildPanel, ChildOrder, OutReorderedWidgets, OutWarnings);
        }
    }
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

// Removed redundant NormalizeJsonKeysToPascalCase implementation, now using UUmgFileTransformation::NormalizeJsonKeysToPascalCase

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
    TSharedPtr<FJsonObject> NormalizedProperties = UUmgFileTransformation::NormalizeJsonKeysToPascalCase(PropertiesJsonObject);
    
    // 2. Extract and expand aliases in NormalizedProperties
    TArray<FString> CurrentKeys;
    auto RefreshCurrentKeys = [&CurrentKeys, &NormalizedProperties]()
    {
        CurrentKeys.Reset();
        for (const auto& Field : NormalizedProperties->Values)
        {
            CurrentKeys.Add(UmgMcpJsonCompat::KeyToString(Field.Key));
        }
    };

    RefreshCurrentKeys();
    for (const FString& Key : CurrentKeys)
    {
        if (Key.StartsWith(TEXT("Slot."), ESearchCase::IgnoreCase))
        {
            // 动态解析出 Slot. 后的第一级属性名
            FString SubPath = Key.Mid(5); // 移除 "Slot."
            FString FirstPropName;
            if (!SubPath.Split(TEXT("."), &FirstPropName, nullptr))
            {
                FirstPropName = SubPath;
            }

            // 动态反射匹配：检查 Slot 是否拥有原生对应的属性
            bool bHasNativeProp = false;
            if (FoundWidget->Slot)
            {
                bHasNativeProp = FoundWidget->Slot->GetClass()->FindPropertyByName(FName(*FirstPropName)) != nullptr;
            }

            // [智能反射匹配成功]：直接交付原生反射引擎处理，跳过下面的降级转换
            if (bHasNativeProp)
            {
                continue;
            }

            // [智能反射匹配失败]：安全降级，进入 Canvas 别名处理层判断
            if (Key.Equals(TEXT("Slot.Position"), ESearchCase::IgnoreCase))
            {
                TSharedPtr<FJsonValue> Val = NormalizedProperties->TryGetField(Key);
                if (!Val.IsValid()) continue;
                if (Val->Type == EJson::Array && Val->AsArray().Num() >= 2) {
                    NormalizedProperties->SetField(TEXT("Slot.LayoutData.Offsets.Left"), Val->AsArray()[0]);
                    NormalizedProperties->SetField(TEXT("Slot.LayoutData.Offsets.Top"), Val->AsArray()[1]);
                    NormalizedProperties->RemoveField(Key);
                }
            }
            else if (Key.Equals(TEXT("Slot.Size"), ESearchCase::IgnoreCase))
            {
                TSharedPtr<FJsonValue> Val = NormalizedProperties->TryGetField(Key);
                if (!Val.IsValid()) continue;
                if (Val->Type == EJson::Array && Val->AsArray().Num() >= 2) {
                    NormalizedProperties->SetField(TEXT("Slot.LayoutData.Offsets.Right"), Val->AsArray()[0]);
                    NormalizedProperties->SetField(TEXT("Slot.LayoutData.Offsets.Bottom"), Val->AsArray()[1]);
                    NormalizedProperties->RemoveField(Key);
                }
            }
            else if (Key.Equals(TEXT("Slot.Anchors"), ESearchCase::IgnoreCase))
            {
                TSharedPtr<FJsonValue> Val = NormalizedProperties->TryGetField(Key);
                if (!Val.IsValid()) continue;
                NormalizedProperties->SetField(TEXT("Slot.LayoutData.Anchors"), Val);
                NormalizedProperties->RemoveField(Key);
            }
            else if (Key.Equals(TEXT("Slot.Alignment"), ESearchCase::IgnoreCase))
            {
                TSharedPtr<FJsonValue> Val = NormalizedProperties->TryGetField(Key);
                if (!Val.IsValid()) continue;
                NormalizedProperties->SetField(TEXT("Slot.LayoutData.Alignment"), Val);
                NormalizedProperties->RemoveField(Key);
            }
        }
    }

    // 3. Separate Slot properties from widget properties and build nested structures
    TSharedPtr<FJsonObject> SlotProperties = MakeShared<FJsonObject>();
    if (NormalizedProperties->HasField(TEXT("Slot")))
    {
        SlotProperties = NormalizedProperties->GetObjectField(TEXT("Slot"));
        NormalizedProperties->RemoveField(TEXT("Slot"));
    }

    // Re-scan for any dotted keys (including expanded aliases)
    RefreshCurrentKeys();
    for (const FString& FullKey : CurrentKeys)
    {
        if (FullKey.Contains(TEXT(".")))
        {
            TArray<FString> Parts;
            FullKey.ParseIntoArray(Parts, TEXT("."));
            
            TSharedPtr<FJsonObject> TargetObj = NormalizedProperties;
            if (Parts[0].Equals(TEXT("Slot"), ESearchCase::IgnoreCase))
            {
                TargetObj = SlotProperties;
                Parts.RemoveAt(0);
            }

            // Safe nested builder
            for (int32 i = 0; i < Parts.Num() - 1; ++i)
            {
                const TSharedPtr<FJsonObject>* ExistingObj = nullptr;
                if (!TargetObj->TryGetObjectField(Parts[i], ExistingObj))
                {
                    TSharedPtr<FJsonObject> NewSubObj = MakeShared<FJsonObject>();
                    TargetObj->SetObjectField(Parts[i], NewSubObj);
                    TargetObj = NewSubObj;
                }
                else
                {
                    TargetObj = ConstCastSharedPtr<FJsonObject>(*ExistingObj);
                }
            }
            TSharedPtr<FJsonValue> Val = NormalizedProperties->TryGetField(FullKey);
            if (!Val.IsValid()) continue;
            TargetObj->SetField(Parts.Last(), Val);
            NormalizedProperties->RemoveField(FullKey);
        }
    }

    if (SlotProperties->Values.Num() > 0)
    {
        // Log the collected Slot JSON
        FString SlotJsonString;
        TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&SlotJsonString);
        FJsonSerializer::Serialize(SlotProperties.ToSharedRef(), Writer);
        UE_LOG(LogUmgSet, Log, TEXT("SetWidgetProperties: Final Slot JSON for '%s': %s"), *WidgetName, *SlotJsonString);
    }

    WidgetBlueprint->Modify();
    FoundWidget->Modify();

    // REFINED STRATEGY: Intercept Brush.ResourceObject specifically as it's the #1 cause of failure.
    if (NormalizedProperties->HasField(TEXT("Brush")))
    {
        TSharedPtr<FJsonObject> BrushObj = NormalizedProperties->GetObjectField(TEXT("Brush"));
        if (BrushObj->HasField(TEXT("ResourceObject")))
        {
            FString Path = BrushObj->GetStringField(TEXT("ResourceObject"));
            UObject* MatAsset = LoadObject<UObject>(nullptr, *Path);
            if (MatAsset)
            {
                // We use reflection to set it directly to avoid converter issues
                if (FProperty* BrushProp = FoundWidget->GetClass()->FindPropertyByName(TEXT("Brush")))
                {
                    void* BrushPtr = BrushProp->ContainerPtrToValuePtr<void>(FoundWidget);
                    if (UScriptStruct* BrushStruct = CastField<FStructProperty>(BrushProp)->Struct)
                    {
                         if (FProperty* ResProp = BrushStruct->FindPropertyByName(TEXT("ResourceObject")))
                         {
                             CastField<FObjectProperty>(ResProp)->SetObjectPropertyValue(ResProp->ContainerPtrToValuePtr<void>(BrushPtr), MatAsset);
                             BrushObj->RemoveField(TEXT("ResourceObject")); // Done
                         }
                    }
                }
            }
        }
    }

    // 4. Apply widget properties (excluding Slot)
    if (NormalizedProperties->Values.Num() > 0)
    {
        for (const auto& Pair : NormalizedProperties->Values)
        {
            const FString PropertyName = UmgMcpJsonCompat::KeyToString(Pair.Key);
            FProperty* Prop = FoundWidget->GetClass()->FindPropertyByName(FName(*PropertyName));
            if (!Prop) continue;

            // SPECIAL CASE: Auto-resolve Object Pointers from paths
            if (FObjectProperty* ObjProp = CastField<FObjectProperty>(Prop))
            {
                if (Pair.Value->Type == EJson::String)
                {
                    FString ObjectPath = Pair.Value->AsString();
                    UObject* ResolvedObj = LoadObject<UObject>(nullptr, *ObjectPath);
                    if (ResolvedObj)
                    {
                        ObjProp->SetObjectPropertyValue(ObjProp->ContainerPtrToValuePtr<void>(FoundWidget), ResolvedObj);
                        continue;
                    }
                }
            }

            // Fallback: Use standard converter for this specific field
            TSharedPtr<FJsonObject> SinglePropJson = MakeShared<FJsonObject>();
            SinglePropJson->SetField(PropertyName, Pair.Value);
            FJsonObjectConverter::JsonObjectToUStruct(SinglePropJson.ToSharedRef(), FoundWidget->GetClass(), FoundWidget, 0, 0);
        }
    }
    
    // Apply Slot properties separately (CRITICAL: must apply to Slot object, not Widget object)
    if (SlotProperties.IsValid() && FoundWidget->Slot)
    {
        UE_LOG(LogUmgSet, Log, TEXT("SetWidgetProperties: Applying Slot properties to Slot object (class: %s)"), *FoundWidget->Slot->GetClass()->GetName());
        FoundWidget->Slot->Modify();

        // Standard converter is usually fine for Slots (mostly numeric/enums)
        FJsonObjectConverter::JsonObjectToUStruct(SlotProperties.ToSharedRef(), FoundWidget->Slot->GetClass(), FoundWidget->Slot, 0, 0);
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
    // 1. Try finding/loading as a fully qualified path (if provided)
    UClass* WidgetClass = nullptr;
    if (WidgetType.Contains(TEXT("/")))
    {
        WidgetClass = FindObject<UClass>(nullptr, *WidgetType);
        if (!WidgetClass)
        {
            WidgetClass = LoadObject<UClass>(nullptr, *WidgetType);
        }
    }

    // 1.5 Special handling for Blueprint Asset Paths (e.g. /Game/UI/MyWidget)
    // If we failed to find it as a direct class, and it looks like a content path, try appending _C
    // This allows users/AI to pass the Asset Path (from list_assets) directly without knowing about _C.
    if (!WidgetClass && WidgetType.StartsWith(TEXT("/Game")))
    {
        FString ClassPath = WidgetType;
        if (!ClassPath.EndsWith(TEXT("_C")))
        {
            ClassPath += TEXT("_C");
        }
        
        WidgetClass = FindObject<UClass>(nullptr, *ClassPath);
        if (!WidgetClass)
        {
            // Note: LoadObject might trigger a load of the package
            WidgetClass = LoadObject<UClass>(nullptr, *ClassPath);
        }
        
        if (WidgetClass)
        {
            UE_LOG(LogUmgSet, Log, TEXT("CreateWidget: Resolved Blueprint Asset '%s' to Class '%s'"), *WidgetType, *ClassPath);
        }
    }

    // 1. Check if widget already exists to prevent duplicates or recursive loops
    if (UWidget* ExistingWidget = WidgetBlueprint->WidgetTree->FindWidget(FName(*WidgetName)))
    {
        UE_LOG(LogUmgSet, Log, TEXT("CreateWidget: Widget '%s' already exists in '%s'. Skipping creation."), *WidgetName, *WidgetBlueprint->GetName());
        return WidgetName;
    }

    // 2. If not found or simple name, try native UMG paths
    if (!WidgetClass)
    {
        // Try /Script/UMG.WidgetType
        FString NativePath = FString::Printf(TEXT("/Script/UMG.%s"), *WidgetType);
        WidgetClass = FindObject<UClass>(nullptr, *NativePath);
        if (!WidgetClass)
        {
             WidgetClass = LoadObject<UClass>(nullptr, *NativePath);
        }
    }

    // 3. Try /Script/UMG.UWidgetType (Reflection prefix)
    if (!WidgetClass)
    {
        FString NativePathU = FString::Printf(TEXT("/Script/UMG.U%s"), *WidgetType);
        WidgetClass = FindObject<UClass>(nullptr, *NativePathU);
         if (!WidgetClass)
        {
             WidgetClass = LoadObject<UClass>(nullptr, *NativePathU);
        }
    }

    // 4. Last resort: Try LoadObject with the raw name (might cause 'Class None.X' warning but handles some edge cases)
    if (!WidgetClass && !WidgetType.Contains(TEXT("/")))
    {
        WidgetClass = FindObject<UClass>(nullptr, *WidgetType);
        if (!WidgetClass)
        {
            // Only call LoadObject if we really have to, as it might warn
            WidgetClass = LoadObject<UClass>(nullptr, *WidgetType);
        }
    }
    
    if (!WidgetClass)
    {
        UE_LOG(LogUmgSet, Error, TEXT("CreateWidget: Failed to find or load widget class '%s'."), *WidgetType);
        return FString();
    }

    // Check if this is a request to create root widget (no existing root)
    bool bCreatingRootWidget = false;
    UPanelWidget* ParentWidget = nullptr;

    // IMPLICIT PARENTING LOGIC
    FString ActualParentName = ParentName;
    if (ActualParentName.IsEmpty())
    {
        // 1. Try Active Widget Scope from Attention Subsystem
        if (GEditor)
        {
            if (UUmgAttentionSubsystem* AttentionSubsystem = GEditor->GetEditorSubsystem<UUmgAttentionSubsystem>())
            {
                FString ScopedWidgetName = AttentionSubsystem->GetTargetWidget();
                if (!ScopedWidgetName.IsEmpty())
                {
                    ActualParentName = ScopedWidgetName;
                    UE_LOG(LogUmgSet, Log, TEXT("CreateWidget: Implicit parent from Active Scope: '%s'"), *ActualParentName);
                }
            }
        }

        // 2. If still empty, try fallback to Root Widget
        if (ActualParentName.IsEmpty() && WidgetBlueprint->WidgetTree->RootWidget)
        {
            ActualParentName = WidgetBlueprint->WidgetTree->RootWidget->GetName();
             UE_LOG(LogUmgSet, Log, TEXT("CreateWidget: Implicit parent from Root Widget: '%s'"), *ActualParentName);
        }
    }

    if (!WidgetBlueprint->WidgetTree->RootWidget)
    {
        // If the tree is empty, ANY creation request must be for the root.
        // We are lenient here: if the widget type is a Panel (valid for root), we allow it regardless of what ParentName the AI sent.
        
        if (WidgetClass->IsChildOf(UPanelWidget::StaticClass()))
        {
            bCreatingRootWidget = true;
            UE_LOG(LogUmgSet, Log, TEXT("CreateWidget: No root widget exists. Auto-promoting '%s' to root widget."), *WidgetType);
        }
        else
        {
            UE_LOG(LogUmgSet, Error, TEXT("CreateWidget: Cannot create '%s' as root widget. Root widget must be a Panel type (e.g., VerticalBox, HorizontalBox, CanvasPanel)."), *WidgetType);
            return FString();
        }
    }
    
    if (!bCreatingRootWidget)
    {
        // Normal case: creating a child widget, need to find parent
        ParentWidget = Cast<UPanelWidget>(WidgetBlueprint->WidgetTree->FindWidget(FName(*ActualParentName)));
        if (!ParentWidget)
        {
            UE_LOG(LogUmgSet, Error, TEXT("CreateWidget: Failed to find ParentWidget with name '%s' in asset '%s'."), *ActualParentName, *WidgetBlueprint->GetPathName());
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
        UE_LOG(LogUmgSet, Log, TEXT("CreateWidget: Successfully created '%s' as child of '%s'."), *WidgetName, *ActualParentName);
    }

    EnsureSourceWidgetGuids(WidgetBlueprint);

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
        EnsureSourceWidgetGuids(WidgetBlueprint);
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBlueprint);
        return true;
    }

    return false;
}

bool UUmgSetSubsystem::ReorderWidgetTree(UWidgetBlueprint* WidgetBlueprint, const FString& RootName, const FString& TreeSpec, TArray<FString>& OutReorderedWidgets, TArray<FString>& OutWarnings, FString& OutError)
{
    OutReorderedWidgets.Reset();
    OutWarnings.Reset();
    OutError.Reset();

    if (!WidgetBlueprint || !WidgetBlueprint->WidgetTree)
    {
        OutError = TEXT("ReorderWidgetTree: Widget blueprint or widget tree is invalid.");
        return false;
    }

    if (TreeSpec.TrimStartAndEnd().IsEmpty())
    {
        OutError = TEXT("ReorderWidgetTree: Missing tree specification.");
        return false;
    }

    FWidgetOrderNode RootOrder;
    if (!BuildOrderRoot(TreeSpec, RootName, RootOrder, OutError))
    {
        return false;
    }

    FString EffectiveRootName = RootOrder.Name;
    if (EffectiveRootName.IsEmpty())
    {
        EffectiveRootName = RootName;
    }

    UPanelWidget* RootPanel = ResolvePanelByName(WidgetBlueprint, EffectiveRootName, OutError);
    if (!RootPanel)
    {
        return false;
    }

    WidgetBlueprint->Modify();
    ApplyOrderNode(RootPanel, RootOrder, OutReorderedWidgets, OutWarnings);

    if (OutReorderedWidgets.Num() > 0)
    {
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBlueprint);
    }

    return true;
}

TArray<FString> UUmgSetSubsystem::ReparentWidget(UWidgetBlueprint* WidgetBlueprint, const FString& WidgetName, const FString& NewParentWidgetJson)
{
    TArray<FString> AffectedWidgets;

    if (!WidgetBlueprint || !WidgetBlueprint->WidgetTree)
    {
        UE_LOG(LogUmgSet, Error, TEXT("ReparentWidget: Received null WidgetBlueprint or WidgetTree."));
        return AffectedWidgets;
    }

    UWidget* WidgetToReplace = WidgetBlueprint->WidgetTree->FindWidget(FName(*WidgetName));
    if (!WidgetToReplace)
    {
        UE_LOG(LogUmgSet, Error, TEXT("ReparentWidget: Failed to find widget to replace '%s'."), *WidgetName);
        return AffectedWidgets;
    }

    // 1. Parse JSON
    TSharedPtr<FJsonObject> JsonObj;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(NewParentWidgetJson);
    if (!FJsonSerializer::Deserialize(Reader, JsonObj) || !JsonObj.IsValid())
    {
        UE_LOG(LogUmgSet, Error, TEXT("ReparentWidget: Failed to parse NewParentWidgetJson."));
        return AffectedWidgets;
    }

    FString WidgetClassPath;
    if (!JsonObj->TryGetStringField(TEXT("widget_class"), WidgetClassPath) && !JsonObj->TryGetStringField(TEXT("widget_type"), WidgetClassPath))
    {
        UE_LOG(LogUmgSet, Error, TEXT("ReparentWidget: JSON is missing 'widget_class' or 'widget_type'."));
        return AffectedWidgets;
    }

    // 2. Resolve widget class
    UClass* WidgetClass = nullptr;
    if (WidgetClassPath.Contains(TEXT("/")))
    {
        WidgetClass = FindObject<UClass>(nullptr, *WidgetClassPath);
        if (!WidgetClass) WidgetClass = LoadObject<UClass>(nullptr, *WidgetClassPath);
    }
    
    if (!WidgetClass)
    {
        FString NativePath = FString::Printf(TEXT("/Script/UMG.%s"), *WidgetClassPath);
        WidgetClass = FindObject<UClass>(nullptr, *NativePath);
        if (!WidgetClass) WidgetClass = LoadObject<UClass>(nullptr, *NativePath);
    }

    if (!WidgetClass)
    {
        FString NativePathU = FString::Printf(TEXT("/Script/UMG.U%s"), *WidgetClassPath);
        WidgetClass = FindObject<UClass>(nullptr, *NativePathU);
        if (!WidgetClass) WidgetClass = LoadObject<UClass>(nullptr, *NativePathU);
    }

    if (!WidgetClass)
    {
        UE_LOG(LogUmgSet, Error, TEXT("ReparentWidget: Failed to find or load class '%s'."), *WidgetClassPath);
        return AffectedWidgets;
    }

    // 3. Prevent child loss: block conversion from container to non-container ONLY when there are active children
    bool bOldIsPanel = WidgetToReplace->IsA(UPanelWidget::StaticClass());
    bool bNewIsPanel = WidgetClass->IsChildOf(UPanelWidget::StaticClass());

    if (bOldIsPanel && !bNewIsPanel)
    {
        UPanelWidget* OldPanel = Cast<UPanelWidget>(WidgetToReplace);
        if (OldPanel && OldPanel->GetChildrenCount() > 0)
        {
            UE_LOG(LogUmgSet, Warning, TEXT("ReparentWidget: Cannot convert container '%s' to non-container '%s' because it has %d active children. Operation aborted to prevent child loss. Please delete children using delete_widget first if this is intentional."), *WidgetName, *WidgetClass->GetName(), OldPanel->GetChildrenCount());
            return AffectedWidgets;
        }
    }

    // 4. Generate name for the new widget (use temp name during replacement to avoid name conflict)
    FString CustomName;
    JsonObj->TryGetStringField(TEXT("widget_name"), CustomName);
    FString TargetName = CustomName;
    if (TargetName.IsEmpty() || TargetName == WidgetName)
    {
        FString CleanClassName = WidgetClass->GetName();
        if (CleanClassName.StartsWith(TEXT("U")))
        {
            CleanClassName.RemoveAt(0);
        }
        TargetName = FString::Printf(TEXT("%s_TempConvert"), *CleanClassName);
    }

    // 5. Construct new widget
    WidgetBlueprint->Modify();
    UWidget* NewWidget = WidgetBlueprint->WidgetTree->ConstructWidget<UWidget>(WidgetClass, FName(*TargetName));
    if (!NewWidget)
    {
        UE_LOG(LogUmgSet, Error, TEXT("ReparentWidget: Failed to construct new widget."));
        return AffectedWidgets;
    }

    // 6. Inherit properties (Union merge)
    TSharedPtr<FJsonObject> OldPropsJson = UUmgFileTransformation::ExportWidgetToJson(WidgetToReplace);
    if (OldPropsJson.IsValid() && OldPropsJson->HasField(TEXT("properties")))
    {
        TSharedPtr<FJsonObject> OldProps = OldPropsJson->GetObjectField(TEXT("properties"));
        // Remove Slot from widget properties since slot properties will be inherited separately
        OldProps->RemoveField(TEXT("Slot"));
        FJsonObjectConverter::JsonObjectToUStruct(OldProps.ToSharedRef(), NewWidget->GetClass(), NewWidget, 0, 0);
    }

    // Apply any new properties specified in the JSON parameter
    const TSharedPtr<FJsonObject>* PropertiesJsonObjPtr;
    if (JsonObj->TryGetObjectField(TEXT("properties"), PropertiesJsonObjPtr))
    {
        TSharedPtr<FJsonObject> TargetProps = *PropertiesJsonObjPtr;
        TargetProps->RemoveField(TEXT("Slot"));
        FJsonObjectConverter::JsonObjectToUStruct(TargetProps.ToSharedRef(), NewWidget->GetClass(), NewWidget, 0, 0);
    }

    // 7. Setup parent relation & inherit slot layout properties
    UPanelWidget* OldParent = Cast<UPanelWidget>(WidgetToReplace->GetParent());
    UPanelSlot* OldSlot = WidgetToReplace->Slot;

    if (OldParent)
    {
        OldParent->Modify();
        int32 ChildIndex = OldParent->GetChildIndex(WidgetToReplace);
        
        // Temporarily detach old widget
        OldParent->RemoveChild(WidgetToReplace);
        
        // Insert new widget at the same index
        UPanelSlot* NewSlot = nullptr;
        if (ChildIndex != INDEX_NONE)
        {
            NewSlot = OldParent->InsertChildAt(ChildIndex, NewWidget);
        }
        else
        {
            NewSlot = OldParent->AddChild(NewWidget);
        }

        // Inherit slot settings if slot types are identical
        if (OldSlot && NewSlot && OldSlot->GetClass() == NewSlot->GetClass())
        {
            TSharedRef<FJsonObject> TempSlotJson = MakeShared<FJsonObject>();
            if (FJsonObjectConverter::UStructToJsonObject(OldSlot->GetClass(), OldSlot, TempSlotJson, 0, 0))
            {
                NewSlot->Modify();
                FJsonObjectConverter::JsonObjectToUStruct(TempSlotJson, NewSlot->GetClass(), NewSlot, 0, 0);
            }
        }
    }
    else
    {
        // Root widget swap case
        if (WidgetBlueprint->WidgetTree->RootWidget == WidgetToReplace)
        {
            WidgetBlueprint->WidgetTree->RootWidget = NewWidget;
        }
    }

    // 8. Transfer children if both are containers
    if (bOldIsPanel && bNewIsPanel)
    {
        UPanelWidget* OldPanel = Cast<UPanelWidget>(WidgetToReplace);
        UPanelWidget* NewPanel = Cast<UPanelWidget>(NewWidget);
        if (OldPanel && NewPanel)
        {
            OldPanel->Modify();
            NewPanel->Modify();
            
            TArray<UWidget*> ChildrenToMove;
            for (int32 i = 0; i < OldPanel->GetChildrenCount(); ++i)
            {
                if (UWidget* Child = OldPanel->GetChildAt(i))
                {
                    ChildrenToMove.Add(Child);
                }
            }
            
            OldPanel->ClearChildren();
            for (UWidget* Child : ChildrenToMove)
            {
                NewPanel->AddChild(Child);
                AffectedWidgets.Add(Child->GetName()); // Children slots are rebuilt
            }
        }
    }

    // 9. Destroy old widget
    WidgetBlueprint->WidgetTree->RemoveWidget(WidgetToReplace);

    // 10. Revert name (rename the temporary name back to original widget name if no new custom name was specified)
    FString FinalName = CustomName;
    if (FinalName.IsEmpty() || FinalName == WidgetName)
    {
        NewWidget->Rename(*WidgetName, nullptr, REN_DontCreateRedirectors | REN_ForceNoResetLoaders);
        FinalName = WidgetName;
    }

    EnsureSourceWidgetGuids(WidgetBlueprint);

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBlueprint);
    AffectedWidgets.Add(FinalName); // The converted widget itself is affected

    return AffectedWidgets;
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
