// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#include "Material/UmgMcpMaterialSubsystem.h"
#include "Bridge/UmgMcpJsonCompat.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Factories/MaterialFactoryNew.h"
#include "MaterialDomain.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionCustom.h"
#include "MaterialEditingLibrary.h" // Added for proper refresh
#include "MaterialShared.h"
#include "Materials/MaterialExpressionComponentMask.h" 
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "MaterialEditingLibrary.h"
// Graph Editing
#include "MaterialGraph/MaterialGraph.h" 
#include "MaterialGraph/MaterialGraphNode.h"
#include "MaterialGraph/MaterialGraphNode_Root.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Editor.h" 
#include "FileHelpers.h"
#if WITH_EDITOR
#endif
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
// Material Editor
#include "IMaterialEditor.h"

#include "UObject/UObjectGlobals.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Editor.h" 
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"
#include "JsonObjectConverter.h"
#include "Misc/PackageName.h"

namespace
{
bool ResolveMaterialAssetPath(const FString& InAssetPath, FString& OutPackageName, FString& OutObjectPath, FString& OutAssetName, FString& OutError)
{
    FString CleanPath = InAssetPath.TrimStartAndEnd();
    CleanPath.ReplaceInline(TEXT("\\"), TEXT("/"));

    if (CleanPath.EndsWith(FPackageName::GetAssetPackageExtension()))
    {
        CleanPath.LeftChopInline(FPackageName::GetAssetPackageExtension().Len());
    }

    if (CleanPath.IsEmpty() || !CleanPath.StartsWith(TEXT("/")))
    {
        OutError = FString::Printf(TEXT("错误: 无效的资产路径: %s"), *InAssetPath);
        return false;
    }

    OutPackageName = FPackageName::ObjectPathToPackageName(CleanPath);
    if (OutPackageName.IsEmpty())
    {
        OutPackageName = CleanPath;
    }

    FText FailureReason;
    if (!FPackageName::IsValidLongPackageName(OutPackageName, true, &FailureReason))
    {
        OutError = FString::Printf(TEXT("错误: 无效的材质包路径: %s (%s)"), *OutPackageName, *FailureReason.ToString());
        return false;
    }

    OutAssetName = FPackageName::GetLongPackageAssetName(OutPackageName);
    if (OutAssetName.IsEmpty())
    {
        OutError = FString::Printf(TEXT("错误: 无效的材质包路径，缺少资产名: %s"), *OutPackageName);
        return false;
    }

    OutObjectPath = CleanPath.Contains(TEXT("."))
        ? CleanPath
        : FString::Printf(TEXT("%s.%s"), *OutPackageName, *OutAssetName);
    return true;
}
}

void UUmgMcpMaterialSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    UE_LOG(LogTemp, Warning, TEXT("[MaterialSubsystem] Initialized."));
}

void UUmgMcpMaterialSubsystem::Deinitialize()
{
    Super::Deinitialize();
}

FString UUmgMcpMaterialSubsystem::SetTargetMaterial(const FString& AssetPath, bool bCreateIfNotFound)
{
    // UE_LOG(LogTemp, Warning, TEXT("[SetTargetMaterial] Called with path: %s"), *AssetPath);
    
    UMaterial* TargetMat = nullptr;
    FString PackageName;
    FString ObjectPath;
    FString AssetName;
    FString PathError;
    if (!ResolveMaterialAssetPath(AssetPath, PackageName, ObjectPath, AssetName, PathError))
    {
        return PathError;
    }
    
    // 1. Try to find in open Editor FIRST (UMG pattern)
    if (GEditor)
    {
        if (UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>())
        {
            FSoftObjectPath SoftObjectPath(ObjectPath);
            UObject* AssetObj = SoftObjectPath.ResolveObject();  // UMG uses ResolveObject, not TryLoad
            
            if (AssetObj)
            {
                IAssetEditorInstance* Editor = AssetEditorSubsystem->FindEditorForAsset(AssetObj, false);
                if (Editor)
                {
                    TargetMat = Cast<UMaterial>(AssetObj);
                    if (TargetMat)
                    {
                        TargetMaterial = TargetMat;
                        // UE_LOG(LogTemp, Warning, TEXT("[SetTargetMaterial] SUCCESS: Found open editor, using live instance (has Graph)"));
                        return FString::Printf(TEXT("设置目标材质成功: %s (编辑器实例)"), *ObjectPath);
                    }
                }
            }
        }
    }
    
    // 2. Fallback to LoadObject if not in editor (UMG pattern)
    if (!TargetMat)
    {
        TargetMat = LoadObject<UMaterial>(nullptr, *ObjectPath, nullptr, LOAD_NoWarn);
    }
    
    if (TargetMat)
    {
        TargetMaterial = TargetMat;
        // UE_LOG(LogTemp, Warning, TEXT("[SetTargetMaterial] SUCCESS: Loaded from disk"));
        return FString::Printf(TEXT("设置目标材质成功: %s"), *ObjectPath);
    }
    

    // 2. Create if Not Found and Allowed
    if (bCreateIfNotFound)
    {
        UPackage* Package = CreatePackage(*PackageName);
        UMaterialFactoryNew* Factory = NewObject<UMaterialFactoryNew>();
        
        UMaterial* NewMat = Cast<UMaterial>(Factory->FactoryCreateNew(
            UMaterial::StaticClass(),
            Package,
            *AssetName,
            RF_Public | RF_Standalone,
            nullptr,
            GWarn
        ));

        if (NewMat)
        {
            // Set Default Config for UMG
            NewMat->MaterialDomain = EMaterialDomain::MD_UI;
            NewMat->BlendMode = BLEND_Translucent;
            
            FAssetRegistryModule::AssetCreated(NewMat);
            NewMat->MarkPackageDirty();
            TargetMaterial = NewMat;
            return FString::Printf(TEXT("创建并设置目标材质: %s"), *ObjectPath);
        }
    }

    return FString::Printf(TEXT("错误: 找不到材质且未允许创建: %s"), *ObjectPath);
}

UMaterial* UUmgMcpMaterialSubsystem::GetTargetMaterial() const
{
    // 仅仅返回缓存，不做任何猜测逻辑，确保“操作对象”就是“缓存对象”
    if (TargetMaterial.IsValid())
    {
        return TargetMaterial.Get();
    }
    
    // 如果真的没设置，兜底捞一个正在编辑的
    if (GEditor)
    {
        if (UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>())
        {
            TArray<UObject*> EditedAssets = AssetEditorSubsystem->GetAllEditedAssets();
            for (UObject* Asset : EditedAssets)
            {
                if (UMaterial* Mat = Cast<UMaterial>(Asset))
                {
                    const_cast<UUmgMcpMaterialSubsystem*>(this)->TargetMaterial = Mat;
                    return Mat;
                }
            }
        }
    }
    return nullptr;
}

FString UUmgMcpMaterialSubsystem::DefineVariable(const FString& ParamName, const FString& ParamType)
{
    UMaterial* Mat = GetTargetMaterial();
    if (!Mat) return TEXT("Error: No Target Material");

    // Check availability
    for (UMaterialExpression* Expr : Mat->GetExpressions())
    {
        UMaterialExpressionParameter* ParamExpr = Cast<UMaterialExpressionParameter>(Expr);
        if (ParamExpr && ParamExpr->ParameterName.ToString() == ParamName)
        {
            return Expr->GetName(); // Return existing handle
        }
        // Texture parameters inherit differently, check them
        UMaterialExpressionTextureSampleParameter* TexParam = Cast<UMaterialExpressionTextureSampleParameter>(Expr);
        if (TexParam && TexParam->ParameterName.ToString() == ParamName)
        {
             return Expr->GetName();
        }
    }

    // Create new using MaterialEditingLibrary
    UClass* NewClass = nullptr;
    if (ParamType.Equals(TEXT("Scalar"), ESearchCase::IgnoreCase))
    {
        NewClass = UMaterialExpressionScalarParameter::StaticClass();
    }
    else if (ParamType.Equals(TEXT("Vector"), ESearchCase::IgnoreCase))
    {
        NewClass = UMaterialExpressionVectorParameter::StaticClass();
    }
    else if (ParamType.Equals(TEXT("Texture"), ESearchCase::IgnoreCase))
    {
        NewClass = UMaterialExpressionTextureSampleParameter2D::StaticClass();
    }

    if (NewClass)
    {
        UMaterialExpression* NewExpr = UMaterialEditingLibrary::CreateMaterialExpression(Mat, NewClass);
        if (NewExpr)
        {
            UMaterialExpressionParameter* ParamExpr = Cast<UMaterialExpressionParameter>(NewExpr);
            if (ParamExpr)
            {
                ParamExpr->ParameterName = *ParamName;
            }
            
            // Texture Parameter special casting
            UMaterialExpressionTextureSampleParameter* TexParam = Cast<UMaterialExpressionTextureSampleParameter>(NewExpr);
            if (TexParam)
            {
                TexParam->ParameterName = *ParamName;
            }

            // Auto-arrange helper: place it somewhere?
            NewExpr->MaterialExpressionEditorX = -200;
            if (Mat->GetEditorOnlyData())
            {
                 // Try ExpressionCollection first
                 NewExpr->MaterialExpressionEditorY = Mat->GetEditorOnlyData()->ExpressionCollection.Expressions.Num() * 100;
            }
            
            Mat->MarkPackageDirty();
            return NewExpr->GetName();
        }
    }

    return TEXT("Error: Unknown Parameter Type or Creation Failed");
}

FString UUmgMcpMaterialSubsystem::AddNode(const FString& NodeClass, const FString& NodeName)
{
    UMaterial* Mat = GetTargetMaterial();
    if (!Mat) return TEXT("Error: No Target Material");

    // Find UClass
    UClass* ExprClass = nullptr;
    
    // Try explicit full path first (e.g. /Script/Engine.MaterialExpressionMask)
    ExprClass = FindObject<UClass>(nullptr, *NodeClass);
    if (!ExprClass)
    {
        // Try standard Engine path construction
        FString EnginePath = TEXT("/Script/Engine.MaterialExpression") + NodeClass;
        ExprClass = FindObject<UClass>(nullptr, *EnginePath);
    }

    if (!ExprClass)
    {
         // Last resort: LoadObject (if not in memory)
         FString EnginePath = TEXT("/Script/Engine.MaterialExpression") + NodeClass;
         ExprClass = LoadObject<UClass>(nullptr, *EnginePath);
    }
    
    if (!ExprClass || !ExprClass->IsChildOf(UMaterialExpression::StaticClass()))
    {
        return FString::Printf(TEXT("Error: Invalid Node Class %s"), *NodeClass);
    }

    UMaterialExpression* NewExpr = UMaterialEditingLibrary::CreateMaterialExpression(Mat, ExprClass);
    if (!NewExpr) return TEXT("Error: CreateMaterialExpression Failed");
    
    // Set Description/Name if provided to act as a tag
    if (!NodeName.IsEmpty())
    {
        NewExpr->Desc = NodeName;
    }
    
    // Stagger position to avoid stack overlap
    NewExpr->MaterialExpressionEditorX = -200;
    if (Mat->GetEditorOnlyData())
    {
        NewExpr->MaterialExpressionEditorY = Mat->GetEditorOnlyData()->ExpressionCollection.Expressions.Num() * 100;
    }

    Mat->MarkPackageDirty();
    ForceRefreshMaterialEditor();
    return NewExpr->GetName();
}

bool UUmgMcpMaterialSubsystem::DeleteNode(const FString& NodeHandle)
{
    UMaterial* Mat = GetTargetMaterial();
    if (!Mat) return false;

    UMaterialExpression* Expr = FindExpressionByHandle(NodeHandle);
    if (Expr)
    {
        if (Mat->GetEditorOnlyData())
        {
             Mat->GetEditorOnlyData()->ExpressionCollection.Expressions.Remove(Expr);
        }
        return true;
    }
    return false;
}

// Helper: Find Property by Name (Case Insensitive)
FExpressionInput* FindInputProperty(UObject* Owner, const FString& PinName)
{
    if (!Owner) return nullptr;

    // List of objects to search for properties (Mat + EditorOnlyData)
    TArray<UObject*> Targets;
    
#if WITH_EDITOR
    if (UMaterial* Mat = Cast<UMaterial>(Owner))
    {
        if (Mat->GetEditorOnlyData())
        {
            Targets.Add((UObject*)Mat->GetEditorOnlyData());
        }
    }
#endif
    Targets.Add(Owner);
    
    // SMART MAPPING: Handle common aliases before searching
    FString SearchName = PinName.TrimStartAndEnd().Replace(TEXT(" "), TEXT(""));
    
    // Explicit user-requested mapping for Root node
    if (UMaterial* Mat = Cast<UMaterial>(Owner))
    {
        if (SearchName.Equals(TEXT("Output"), ESearchCase::IgnoreCase))
        {
            SearchName = (Mat->MaterialDomain == EMaterialDomain::MD_UI) ? TEXT("EmissiveColor") : TEXT("BaseColor");
        }
        else if (SearchName.Equals(TEXT("FinalColor"), ESearchCase::IgnoreCase) || SearchName.Equals(TEXT("最终颜色"), ESearchCase::IgnoreCase))
        {
            SearchName = TEXT("EmissiveColor");
        }
        else if (SearchName.Equals(TEXT("Opacity"), ESearchCase::IgnoreCase) || SearchName.Equals(TEXT("不透明度"), ESearchCase::IgnoreCase))
        {
            SearchName = TEXT("Opacity");
        }
        else if (SearchName.Equals(TEXT("OpacityMask"), ESearchCase::IgnoreCase) || SearchName.Equals(TEXT("不透明度蒙版"), ESearchCase::IgnoreCase))
        {
            SearchName = TEXT("OpacityMask");
        }
        else if (SearchName.Equals(TEXT("WorldPositionOffset"), ESearchCase::IgnoreCase))
        {
            SearchName = TEXT("WorldPositionOffset");
        }
    }

    for (UObject* Target : Targets)
    {
        for (TFieldIterator<FProperty> PropIt(Target->GetClass()); PropIt; ++PropIt)
        {
            FProperty* Prop = *PropIt;
            FString PropName = Prop->GetName();
            
            bool bMatch = PropName.Equals(SearchName, ESearchCase::IgnoreCase) ||
                          Prop->GetDisplayNameText().ToString().Equals(SearchName, ESearchCase::IgnoreCase);
            
            // Heuristic fallbacks
            if (!bMatch)
            {
                if (SearchName.Equals(TEXT("UV"), ESearchCase::IgnoreCase) && PropName.Equals(TEXT("Coordinates"), ESearchCase::IgnoreCase)) bMatch = true;
                if (SearchName.Equals(TEXT("Alpha"), ESearchCase::IgnoreCase) && PropName.Equals(TEXT("A"), ESearchCase::IgnoreCase)) bMatch = true;
            }

            if (bMatch)
            {
                FStructProperty* StructProp = CastField<FStructProperty>(Prop);
                if (StructProp && StructProp->Struct)
                {
                    FString StructName = StructProp->Struct->GetName();
                    // Precise match for material input types to avoid access violations
                    if (StructName.Equals(TEXT("ExpressionInput")) || 
                        StructName.Equals(TEXT("ScalarMaterialInput")) || 
                        StructName.Equals(TEXT("VectorMaterialInput")) || 
                        StructName.Equals(TEXT("ColorMaterialInput")) ||
                        StructName.Contains(TEXT("MaterialInput")))
                    {
                        return StructProp->ContainerPtrToValuePtr<FExpressionInput>(Target);
                    }
                }
            }
        }
    }
    return nullptr;
}

static int32 ResolveExpressionOutputIndex(UMaterialExpression* Expression, const FString& PinName)
{
    if (!Expression || PinName.IsEmpty() || PinName.Equals(TEXT("Output"), ESearchCase::IgnoreCase))
    {
        return 0;
    }

    if (UMaterialExpressionCustom* CustomNode = Cast<UMaterialExpressionCustom>(Expression))
    {
        for (int32 i = 0; i < CustomNode->AdditionalOutputs.Num(); ++i)
        {
            if (CustomNode->AdditionalOutputs[i].OutputName.ToString().Equals(PinName, ESearchCase::IgnoreCase))
            {
                return i + 1;
            }
        }
    }

    return 0;
}

bool UUmgMcpMaterialSubsystem::ConnectNodes(const FString& FromHandle, const FString& ToHandle)
{
    return ConnectPins(FromHandle, TEXT(""), ToHandle, TEXT("")); // Empty pins trigger Default logic
}

bool UUmgMcpMaterialSubsystem::ConnectPins(const FString& FromHandle, const FString& FromPin, const FString& ToHandle, const FString& ToPin)
{
    UMaterial* Mat = GetTargetMaterial();
    if (!Mat) return false;

    // Check if Target is Root Node
    bool bIsRootAlias = ToHandle.StartsWith(TEXT("Master")) || 
                        ToHandle.Equals(TEXT("Output"), ESearchCase::IgnoreCase) ||
                        ToHandle.Equals(TEXT("MaterialRoot")) || 
                        ToHandle.Equals(Mat->GetName());
    UE_LOG(LogTemp, Log, TEXT("[MaterialSubsystem] ConnectPins: From=%s, To=%s, bIsRootAlias=%d"), *FromHandle, *ToHandle, bIsRootAlias);

    if (bIsRootAlias)
    {
        if (Mat->MaterialGraph)
        {
            // === ROOT NODE: Use Graph-based connection (Preferred) ===
        // 1. Find Source GraphNode
        UMaterialGraphNode* SourceGraphNode = nullptr;
        for (UEdGraphNode* Node : Mat->MaterialGraph->Nodes)
        {
            UMaterialGraphNode* MatNode = Cast<UMaterialGraphNode>(Node);
            if (MatNode && MatNode->MaterialExpression)
            {
                if (MatNode->MaterialExpression->GetName().Equals(FromHandle, ESearchCase::IgnoreCase) ||
                    MatNode->MaterialExpression->Desc.Equals(FromHandle, ESearchCase::IgnoreCase))
                {
                    SourceGraphNode = MatNode;
                    break;
                }
            }
        }

        if (!SourceGraphNode)
        {
            // UE_LOG(LogTemp, Error, TEXT("[ConnectPins] Source node not found: %s"), *FromHandle);
            return false;
        }

        // 2. Find Root Node Object in Graph
        UEdGraphNode* RootNode = nullptr;
        for (UEdGraphNode* Node : Mat->MaterialGraph->Nodes)
        {
            if (Node->IsA(UMaterialGraphNode_Root::StaticClass()))
            {
                RootNode = Node;
                break;
            }
        }

        if (!RootNode)
        {
            UE_LOG(LogTemp, Error, TEXT("[MaterialSubsystem] ConnectPins FATAL: Root node object NOT found in Graph! Material might be corrupted or not open in Editor."));
            return false;
        }

        UE_LOG(LogTemp, Log, TEXT("[MaterialSubsystem] ConnectPins: Successfully accessed Root Node Object: %s"), *RootNode->GetName());

        UE_LOG(LogTemp, Log, TEXT("[MaterialSubsystem] ConnectPins: Found Root Node. Dumping all Input Pins:"));
        for (UEdGraphPin* Pin : RootNode->Pins)
        {
            if (Pin->Direction == EGPD_Input)
            {
                UE_LOG(LogTemp, Log, TEXT("  - Pin: '%s', Category: '%s', SubCategory: '%s'"), 
                    *Pin->PinName.ToString(), *Pin->PinType.PinCategory.ToString(), *Pin->PinType.PinSubCategory.ToString());
            }
        }

        // 3. Find Output Pin on Source
        UEdGraphPin* SourcePin = nullptr;
        int32 SourceOutputIndex = 0;
        if (FromPin.IsEmpty() || FromPin.Equals(TEXT("Output"), ESearchCase::IgnoreCase))
        {
            // If empty or named "Output", take the first output pin found
            int32 OutputIndex = 0;
            for (UEdGraphPin* Pin : SourceGraphNode->Pins)
            {
                if (Pin->Direction != EGPD_Output)
                {
                    continue;
                }
                if (Pin->Direction == EGPD_Output)
                {
                    SourcePin = Pin;
                    SourceOutputIndex = OutputIndex;
                    break;
                }
                ++OutputIndex;
            }
        }
        else
        {
            int32 OutputIndex = 0;
            for (UEdGraphPin* Pin : SourceGraphNode->Pins)
            {
                if (Pin->Direction != EGPD_Output)
                {
                    continue;
                }
                if (Pin->PinName.ToString().Equals(FromPin, ESearchCase::IgnoreCase))
                {
                    SourcePin = Pin;
                    SourceOutputIndex = OutputIndex;
                    break;
                }
                ++OutputIndex;
            }
        }

        if (!SourcePin)
        {
            // UE_LOG(LogTemp, Error, TEXT("[ConnectPins] Source output pin not found"));
            return false;
        }

        // 4. Resolve Target Pin on Root Node
        UEdGraphPin* TargetPin = nullptr;
        FString TargetPinName = ToPin;

        // SMART MAPPING: Map aliases to internal property names for Root Node
        FString CleanPinName = TargetPinName.TrimStartAndEnd().Replace(TEXT(" "), TEXT(""));
        if (CleanPinName.IsEmpty() || CleanPinName.Equals(TEXT("Output"), ESearchCase::IgnoreCase))
        {
            TargetPinName = (Mat->MaterialDomain == EMaterialDomain::MD_UI) ? TEXT("EmissiveColor") : TEXT("BaseColor");
        }
        else if (CleanPinName.Equals(TEXT("FinalColor"), ESearchCase::IgnoreCase) || CleanPinName.Equals(TEXT("最终颜色"), ESearchCase::IgnoreCase))
        {
            TargetPinName = TEXT("EmissiveColor");
        }
        else if (CleanPinName.Equals(TEXT("Opacity"), ESearchCase::IgnoreCase) || CleanPinName.Equals(TEXT("不透明度"), ESearchCase::IgnoreCase))
        {
            TargetPinName = TEXT("Opacity");
        }
        else if (CleanPinName.Equals(TEXT("OpacityMask"), ESearchCase::IgnoreCase) || CleanPinName.Equals(TEXT("不透明度蒙版"), ESearchCase::IgnoreCase))
        {
            TargetPinName = TEXT("OpacityMask");
        }
        else
        {
            TargetPinName = CleanPinName;
        }

        // --- STRATEGY 1: Use Stable Property Names from UMaterial (Language Independent) ---
        FProperty* MatProp = nullptr;
#if WITH_EDITOR
        if (Mat->GetEditorOnlyData())
        {
            MatProp = Mat->GetEditorOnlyData()->GetClass()->FindPropertyByName(*TargetPinName);
        }
#endif
        if (!MatProp)
        {
            MatProp = Mat->GetClass()->FindPropertyByName(*TargetPinName);
        }

        if (MatProp)
        {
            FString LocalizedDisplayName = MatProp->GetDisplayNameText().ToString();
            UE_LOG(LogTemp, Log, TEXT("[MaterialSubsystem] ConnectPins: Resolved Stable ID '%s' to Localized Name '%s'"), *TargetPinName, *LocalizedDisplayName);
            
            for (UEdGraphPin* Pin : RootNode->Pins)
            {
                if (Pin->Direction == EGPD_Input && 
                    (Pin->PinName.ToString().Equals(TargetPinName, ESearchCase::IgnoreCase) ||
                     Pin->PinName.ToString().Equals(LocalizedDisplayName, ESearchCase::IgnoreCase) ||
                     Pin->PinName.ToString().Replace(TEXT(" "), TEXT("")).Equals(TargetPinName, ESearchCase::IgnoreCase)))
                {
                    TargetPin = Pin;
                    break;
                }
            }
        }

        // --- STRATEGY 2: Heuristic/Localized fallback ---
        if (!TargetPin)
        {
            if (TargetPinName.Equals(TEXT("EmissiveColor"), ESearchCase::IgnoreCase) ||
                TargetPinName.Equals(TEXT("自发光"), ESearchCase::IgnoreCase) ||
                TargetPinName.Equals(TEXT("Final"), ESearchCase::IgnoreCase))
            {
                for (UEdGraphPin* Pin : RootNode->Pins)
                {
                    if (Pin->Direction == EGPD_Input && 
                         (Pin->PinName.ToString().Contains(TEXT("Final")) || 
                          Pin->PinName.ToString().Contains(TEXT("Emissive")) ||
                          Pin->PinName.ToString().Contains(TEXT("最终")) ||
                          Pin->PinName.ToString().Contains(TEXT("自发光"))))
                    {
                        TargetPin = Pin;
                        break;
                    }
                }
            }
            else if (TargetPinName.Equals(TEXT("Opacity"), ESearchCase::IgnoreCase) ||
                     TargetPinName.Equals(TEXT("不透明度"), ESearchCase::IgnoreCase))
            {
                for (UEdGraphPin* Pin : RootNode->Pins)
                {
                    if (Pin->Direction == EGPD_Input && 
                        (Pin->PinName.ToString().Contains(TEXT("不透明")) ||
                         Pin->PinName.ToString().Contains(TEXT("Opacity"))))
                    {
                        TargetPin = Pin;
                        break;
                    }
                }
            }
            else if (TargetPinName.Equals(TEXT("BaseColor"), ESearchCase::IgnoreCase) ||
                     TargetPinName.Equals(TEXT("基础颜色"), ESearchCase::IgnoreCase))
            {
                 for (UEdGraphPin* Pin : RootNode->Pins)
                 {
                     if (Pin->Direction == EGPD_Input && 
                          (Pin->PinName.ToString().Contains(TEXT("Base")) || 
                           Pin->PinName.ToString().Contains(TEXT("基础"))))
                     {
                         TargetPin = Pin;
                         break;
                     }
                 }
            }
        }

        // --- STRATEGY 3: Direct Name Match fallback ---
        if (!TargetPin)
        {
            for (UEdGraphPin* Pin : RootNode->Pins)
            {
                if (Pin->Direction == EGPD_Input && Pin->PinName.ToString().Equals(TargetPinName, ESearchCase::IgnoreCase))
                {
                    TargetPin = Pin;
                    break;
                }
            }
        }

        if (!TargetPin)
        {
            UE_LOG(LogTemp, Error, TEXT("[MaterialSubsystem] ConnectPins ERROR: Could not match Root Input Pin for handle '%s' with name '%s'"), *ToHandle, *TargetPinName);
            return false;
        }

        UE_LOG(LogTemp, Log, TEXT("[MaterialSubsystem] ConnectPins: SUCCESS! Linking '%s' to Root Pin '%s'"), *FromHandle, *TargetPin->PinName.ToString());

        // 5. Make Graph Connection
        SourcePin->MakeLinkTo(TargetPin);

        // --- NEW: Sync Data Layer ---
        FExpressionInput* DataInput = FindInputProperty(Mat, TargetPinName);
        if (DataInput && SourceGraphNode->MaterialExpression)
        {
            DataInput->Expression = SourceGraphNode->MaterialExpression;
            DataInput->OutputIndex = SourceOutputIndex;
            UE_LOG(LogTemp, Log, TEXT("[MaterialSubsystem] ConnectPins: Data-layer synced for Root.%s"), *TargetPinName);
        }

        // 6. Force Refresh
        if (Mat->MaterialGraph) Mat->MaterialGraph->NotifyGraphChanged();
        Mat->Modify();
        Mat->PostEditChange();
        Mat->MarkPackageDirty();
        ForceRefreshMaterialEditor();
        return true;
        }

        // === ROOT NODE FALLBACK: Use Reflection (When Graph is null) ===
        UE_LOG(LogTemp, Warning, TEXT("[MaterialSubsystem] ConnectPins: Graph is null, using Reflection fallback for Master."));
        
        UMaterialExpression* FromExpr = FindExpressionByHandle(FromHandle);
        if (!FromExpr) return false;

        FExpressionInput* InputPtr = FindInputProperty(Mat, ToPin);
        if (InputPtr)
        {
            InputPtr->Expression = FromExpr;
            InputPtr->OutputIndex = ResolveExpressionOutputIndex(FromExpr, FromPin);
            
            Mat->PostEditChange();
            Mat->MarkPackageDirty();
            ForceRefreshMaterialEditor();
            return true;
        }

        return false;
    }
    else
    {
        // === NORMAL NODE: Use Reflection-based connection (Original Method) ===
        UMaterialExpression* FromNode = FindExpressionByHandle(FromHandle);
        if (!FromNode) 
        {
            // UE_LOG(LogTemp, Error, TEXT("[ConnectPins] Source not found: %s"), *FromHandle);
            return false;
        }

        UObject* TargetObject = FindExpressionByHandle(ToHandle);
        if (!TargetObject) 
        {
            // UE_LOG(LogTemp, Error, TEXT("[ConnectPins] Target not found: %s"), *ToHandle);
            return false;
        }

        // UE_LOG(LogTemp, Warning, TEXT("[ConnectPins] Reflection: From: %s, To: %s"), *FromHandle, *ToHandle);

        // Find Input Property
        FExpressionInput* InputPtr = nullptr;
        FString ReflTargetPinName = ToPin;

        if (ReflTargetPinName.IsEmpty())
        {
            const TArray<FString> TryPins = { TEXT("Input"), TEXT("Coordinates"), TEXT("UV"), TEXT("Alpha"), TEXT("A") };
            for (const FString& Try : TryPins)
            {
                InputPtr = FindInputProperty(TargetObject, Try);
                if (InputPtr) break;
            }
        }
        else
        {
            InputPtr = FindInputProperty(TargetObject, ReflTargetPinName);
        }

        // Special Case: Custom Node Inputs
        UMaterialExpressionCustom* CustomNode = Cast<UMaterialExpressionCustom>(TargetObject);
        if (CustomNode && !InputPtr)
        {
            for (int32 i = 0; i < CustomNode->Inputs.Num(); i++)
            {
                if (CustomNode->Inputs[i].InputName.ToString().Equals(ReflTargetPinName, ESearchCase::IgnoreCase))
                {
                    InputPtr = &(CustomNode->Inputs[i].Input);
                    break;
                }
            }
        }

        if (InputPtr)
        {
            InputPtr->Expression = FromNode;
            InputPtr->OutputIndex = ResolveExpressionOutputIndex(FromNode, FromPin);
            
            // UE_LOG(LogTemp, Warning, TEXT("[ConnectPins] Reflection: Connected %s -> %s"), *FromHandle, *ToHandle);
            
            Mat->Modify();
            Mat->PostEditChange();
            Mat->MarkPackageDirty();
            
            if (UMaterialExpression* ToNode = Cast<UMaterialExpression>(TargetObject))
            {
                ToNode->PostEditChange();
            }

            ForceRefreshMaterialEditor();
            return true;
        }

        // UE_LOG(LogTemp, Error, TEXT("[ConnectPins] Reflection: Failed to resolve input pin: %s"), *ToPin);
        return false;
    }
}

bool UUmgMcpMaterialSubsystem::SetCustomNodeHLSL(const FString& NodeHandle, const FString& HlslCode, const TArray<FString>& InputNames)
{
    UMaterialExpressionCustom* CustomNode = Cast<UMaterialExpressionCustom>(FindExpressionByHandle(NodeHandle));
    if (!CustomNode) return false;

    // Unescape newlines
    FString Code = HlslCode.Replace(TEXT("\\n"), TEXT("\n"));
    CustomNode->Code = Code;
    
    // Rebuild Inputs
    CustomNode->Inputs.Empty();
    for (const FString& Name : InputNames)
    {
        FCustomInput NewInput;
        NewInput.InputName = *Name;
        CustomNode->Inputs.Add(NewInput);
    }
    
    // Force update
    CustomNode->PostEditChange();
    ForceRefreshMaterialEditor();
    
    // UE_LOG(LogTemp, Warning, TEXT("[SetCustomNodeHLSL] Node: %s, Set Code Len: %d, Inputs Count: %d"), *NodeHandle, Code.Len(), CustomNode->Inputs.Num());
    // for(const auto& Inp : CustomNode->Inputs)
    // {
    //      UE_LOG(LogTemp, Warning, TEXT("  - Input: %s"), *Inp.InputName.ToString());
    // }
    
    return true;
}

bool UUmgMcpMaterialSubsystem::SetNodeProperties(const FString& NodeHandle, const TSharedPtr<FJsonObject>& Properties)
{
    UMaterial* Mat = GetTargetMaterial();
    if (!Mat || !Properties) return false;

    UObject* TargetObject = nullptr;
    
    // Check if targeting the Material Asset itself
    bool bTargetRoot = NodeHandle.StartsWith(TEXT("Master")) || 
                      NodeHandle.Equals(TEXT("Output"), ESearchCase::IgnoreCase) ||
                      NodeHandle.Equals(TEXT("MaterialRoot")) || 
                      NodeHandle.Equals(Mat->GetName());

    if (bTargetRoot)
    {
        TargetObject = Mat;
    }
    else
    {
        TargetObject = FindExpressionByHandle(NodeHandle);
    }

    if (!TargetObject) return false;

    // Use Reflection to apply properties
    for (const auto& Elem : Properties->Values)
    {
        FString PropName = UmgMcpJsonCompat::KeyToString(Elem.Key);
        TSharedPtr<FJsonValue> JsonVal = Elem.Value;
        
        FProperty* Prop = TargetObject->GetClass()->FindPropertyByName(*PropName);
        if (Prop)
        {
             // 1. Numeric Types
             if (FFloatProperty* FloatProp = CastField<FFloatProperty>(Prop))
             {
                 FloatProp->SetPropertyValue_InContainer(TargetObject, JsonVal->AsNumber());
             }
             else if (FDoubleProperty* DblProp = CastField<FDoubleProperty>(Prop))
             {
                 DblProp->SetPropertyValue_InContainer(TargetObject, JsonVal->AsNumber());
             }
             else if (FIntProperty* IntProp = CastField<FIntProperty>(Prop))
             {
                 IntProp->SetPropertyValue_InContainer(TargetObject, (int32)JsonVal->AsNumber());
             }
             else if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Prop))
             {
                 BoolProp->SetPropertyValue_InContainer(TargetObject, JsonVal->AsBool());
             }
             // 2. Enum Types (Important for MaterialDomain, BlendMode)
             else if (FEnumProperty* EnumProp = CastField<FEnumProperty>(Prop))
             {
                 if (JsonVal->Type == EJson::String)
                 {
                     FString EnumString = JsonVal->AsString();
                     int64 EnumValue = EnumProp->GetEnum()->GetValueByNameString(EnumString);
                     if (EnumValue != INDEX_NONE)
                     {
                         EnumProp->GetUnderlyingProperty()->SetIntPropertyValue(EnumProp->ContainerPtrToValuePtr<void>(TargetObject), EnumValue);
                     }
                 }
                 else if (JsonVal->Type == EJson::Number)
                 {
                     EnumProp->GetUnderlyingProperty()->SetIntPropertyValue(EnumProp->ContainerPtrToValuePtr<void>(TargetObject), (int64)JsonVal->AsNumber());
                 }
             }
             else if (FByteProperty* ByteProp = CastField<FByteProperty>(Prop))
             {
                 if (ByteProp->Enum && JsonVal->Type == EJson::String)
                 {
                     int64 EnumValue = ByteProp->Enum->GetValueByNameString(*JsonVal->AsString(), EGetByNameFlags::None);
                     if (EnumValue != INDEX_NONE)
                     {
                         ByteProp->SetPropertyValue_InContainer(TargetObject, (uint8)EnumValue);
                     }
                 }
                 else if (JsonVal->Type == EJson::Number)
                 {
                     ByteProp->SetPropertyValue_InContainer(TargetObject, (uint8)JsonVal->AsNumber());
                 }
             }
             // 3. String/Name Types
             else if (FStrProperty* StrProp = CastField<FStrProperty>(Prop))
             {
                 StrProp->SetPropertyValue_InContainer(TargetObject, JsonVal->AsString());
             }
             else if (FNameProperty* NameProp = CastField<FNameProperty>(Prop))
             {
                 NameProp->SetPropertyValue_InContainer(TargetObject, FName(*JsonVal->AsString()));
             }
        }
    }
    
    TargetObject->PostEditChange();
    
    // If we changed Material properties, we might need a compile and a full refresh
    if (bTargetRoot)
    {
        Mat->MarkPackageDirty();
    }

    ForceRefreshMaterialEditor();
    return true;
}

bool UUmgMcpMaterialSubsystem::SetOutputNode(const FString& NodeHandle)
{
    UMaterial* Mat = GetTargetMaterial();
    if (!Mat) return false;

    if (!Mat->MaterialGraph)
    {
        // UE_LOG(LogTemp, Error, TEXT("[SetOutputNode] MaterialGraph is null"));
        return false;
    }

    // 1. Find Source GraphNode
    UMaterialGraphNode* SourceGraphNode = nullptr;
    for (UEdGraphNode* Node : Mat->MaterialGraph->Nodes)
    {
        UMaterialGraphNode* MatNode = Cast<UMaterialGraphNode>(Node);
        if (MatNode && MatNode->MaterialExpression)
        {
            if (MatNode->MaterialExpression->GetName().Equals(NodeHandle, ESearchCase::IgnoreCase) ||
                MatNode->MaterialExpression->Desc.Equals(NodeHandle, ESearchCase::IgnoreCase))
            {
                SourceGraphNode = MatNode;
                break;
            }
        }
    }

    if (!SourceGraphNode)
    {
        // UE_LOG(LogTemp, Error, TEXT("[SetOutputNode] Source node not found: %s"), *NodeHandle);
        return false;
    }

    // 2. Find Root Node
    UEdGraphNode* RootNode = nullptr;
    for (UEdGraphNode* Node : Mat->MaterialGraph->Nodes)
    {
        if (Node->IsA(UMaterialGraphNode_Root::StaticClass()))
        {
            RootNode = Node;
            break;
        }
    }

    if (!RootNode)
    {
        // UE_LOG(LogTemp, Error, TEXT("[SetOutputNode] Root node not found in MaterialGraph"));
        return false;
    }

    // 3. Find Output Pin on Source (use first output)
    UEdGraphPin* SourcePin = nullptr;
    for (UEdGraphPin* Pin : SourceGraphNode->Pins)
    {
        if (Pin->Direction == EGPD_Output)
        {
            SourcePin = Pin;
            break;
        }
    }

    if (!SourcePin)
    {
        // UE_LOG(LogTemp, Error, TEXT("[SetOutputNode] No output pin found on source node"));
        return false;
    }

    // 4. Connect to appropriate Root pins based on Material Domain
    bool bSuccess = false;

    if (Mat->bUseMaterialAttributes)
    {
        // Connect to MaterialAttributes pin
        for (UEdGraphPin* Pin : RootNode->Pins)
        {
            if (Pin->Direction == EGPD_Input && Pin->PinName.ToString().Equals(TEXT("MaterialAttributes"), ESearchCase::IgnoreCase))
            {
                SourcePin->MakeLinkTo(Pin);
                bSuccess = true;
                // UE_LOG(LogTemp, Warning, TEXT("[SetOutputNode] Connected to MaterialAttributes"));
                break;
            }
        }
    }
    else
    {
        // Standard Mode: Connect to EmissiveColor (for UI) or BaseColor
        for (UEdGraphPin* Pin : RootNode->Pins)
        {
            if (Pin->Direction == EGPD_Input)
            {
                if (Pin->PinName.ToString().Equals(TEXT("EmissiveColor"), ESearchCase::IgnoreCase) ||
                    Pin->PinName.ToString().Equals(TEXT("BaseColor"), ESearchCase::IgnoreCase))
                {
                    SourcePin->MakeLinkTo(Pin);
                    bSuccess = true;
                    // UE_LOG(LogTemp, Warning, TEXT("[SetOutputNode] Connected to %s"), *Pin->PinName.ToString());
                    break;
                }
            }
        }

        // Also try to connect to Opacity if available (for translucent UI materials)
        if (bSuccess && Mat->MaterialDomain == EMaterialDomain::MD_UI)
        {
            for (UEdGraphPin* Pin : RootNode->Pins)
            {
                if (Pin->Direction == EGPD_Input && 
                    (Pin->PinName.ToString().Equals(TEXT("Opacity"), ESearchCase::IgnoreCase) ||
                     Pin->PinName.ToString().Equals(TEXT("OpacityMask"), ESearchCase::IgnoreCase)))
                {
                    SourcePin->MakeLinkTo(Pin);
                    // UE_LOG(LogTemp, Warning, TEXT("[SetOutputNode] Also connected to %s"), *Pin->PinName.ToString());
                    break;
                }
            }
        }
    }

    if (bSuccess)
    {
        Mat->PostEditChange();
        Mat->MarkPackageDirty();
        ForceRefreshMaterialEditor();
    }

    return bSuccess;
}

FString UUmgMcpMaterialSubsystem::CompileAsset()
{
    if (UMaterial* Mat = GetTargetMaterial())
    {
        Mat->PreEditChange(nullptr);
        Mat->PostEditChange();
        Mat->ForceRecompileForRendering();
        return TEXT("Compiled Successfully");
    }
    return TEXT("Error: No Target Material");
}

bool UUmgMcpMaterialSubsystem::SaveTargetMaterial()
{
    UMaterial* Mat = GetTargetMaterial();
    if (!Mat)
    {
        UE_LOG(LogTemp, Error, TEXT("SaveTargetMaterial: No target material."));
        return false;
    }

    UPackage* Package = Mat->GetOutermost();
    if (!Package)
    {
        UE_LOG(LogTemp, Error, TEXT("SaveTargetMaterial: Failed to get package for asset '%s'."), *Mat->GetPathName());
        return false;
    }

    TArray<UPackage*> PackagesToSave;
    PackagesToSave.Add(Package);

    FEditorFileUtils::EPromptReturnCode ReturnCode = FEditorFileUtils::PromptForCheckoutAndSave(PackagesToSave, false, false);
    if (ReturnCode == FEditorFileUtils::EPromptReturnCode::PR_Success)
    {
        UE_LOG(LogTemp, Log, TEXT("SaveTargetMaterial: Successfully saved asset '%s'."), *Mat->GetPathName());
        return true;
    }

    UE_LOG(LogTemp, Error, TEXT("SaveTargetMaterial: Failed to save asset '%s'."), *Mat->GetPathName());
    return false;
}

FString UUmgMcpMaterialSubsystem::GetNodeInfo(const FString& NodeHandle)
{
    UMaterial* Mat = GetTargetMaterial();
    if (!Mat) return TEXT("{}");

    // We no longer strictly require MaterialGraph here for bIsRootAlias fallback

    UEdGraphNode* TargetNode = nullptr;

    // 1. Resolve Target Node in the Graph
    // Check for Root Alias
    bool bIsRootAlias = NodeHandle.StartsWith(TEXT("Master")) || NodeHandle.Equals(TEXT("MaterialRoot")) || NodeHandle.Equals(Mat->GetName());
    
    if (Mat->MaterialGraph)
    {
        if (bIsRootAlias)
        {
            // STRATEGY: Directly iterate the Graph Nodes to find the Root instance
            for (UEdGraphNode* Node : Mat->MaterialGraph->Nodes)
            {
                if (Node->IsA(UMaterialGraphNode_Root::StaticClass()))
                {
                    TargetNode = Node;
                    break;
                }
            }
            
            if (TargetNode)
            {
                UE_LOG(LogTemp, Log, TEXT("[MaterialSubsystem] GetNodeInfo: Successfully located Root Node Object in Graph: %s"), *TargetNode->GetName());
            }
        }
        else
        {
            // Find Expression Node
            for (UEdGraphNode* Node : Mat->MaterialGraph->Nodes)
            {
                UMaterialGraphNode* MatNode = Cast<UMaterialGraphNode>(Node);
                if (MatNode && MatNode->MaterialExpression)
                {
                    if (MatNode->MaterialExpression->GetName().Equals(NodeHandle, ESearchCase::IgnoreCase) || 
                        MatNode->MaterialExpression->Desc.Equals(NodeHandle, ESearchCase::IgnoreCase))
                    {
                        TargetNode = Node;
                        break;
                    }
                }
            }
        }
    }

    if (!TargetNode)
    {
        if (bIsRootAlias)
        {
            UE_LOG(LogTemp, Warning, TEXT("[MaterialSubsystem] GetNodeInfo: GraphNode not found for Root, using Property Reflection fallback."));
        }
        else
        {
            return FString::Printf(TEXT("{\"error\": \"Node not found in Graph: %s\"}"), *NodeHandle);
        }
    }

    // 2. Dump Pins and Connections
    TSharedPtr<FJsonObject> RootObject = MakeShareable(new FJsonObject());
    TArray<TSharedPtr<FJsonValue>> PinsArray;
    TSharedPtr<FJsonObject> ConnectionsObject = MakeShareable(new FJsonObject());
    TSet<FString> UniquePins;

    if (TargetNode)
    {
        // === GRAPH-BASED INTROSPECTION ===
        for (UEdGraphPin* Pin : TargetNode->Pins)
        {
            if (Pin->Direction == EGPD_Input)
            {
                FString PinName = Pin->PinName.ToString();
                
                // Deduplicate pins (UE5 often lists properties twice in Graph nodes)
                if (UniquePins.Contains(PinName)) continue;
                UniquePins.Add(PinName);

                FString StableId = PinName;
                if (bIsRootAlias)
                {
                    // Map localized UI name back to Stable Property ID for Root Node
#if WITH_EDITOR
                    if (Mat->GetEditorOnlyData())
                    {
                        for (TFieldIterator<FProperty> PropIt(Mat->GetEditorOnlyData()->GetClass()); PropIt; ++PropIt)
                        {
                            if (PropIt->GetDisplayNameText().ToString().Equals(PinName, ESearchCase::IgnoreCase))
                            {
                                StableId = PropIt->GetName();
                                break;
                            }
                        }
                    }
#endif
                }

                TSharedPtr<FJsonObject> PinObj = MakeShareable(new FJsonObject());
                PinObj->SetStringField(TEXT("name"), PinName);
                PinObj->SetStringField(TEXT("id"), StableId);
                PinsArray.Add(MakeShareable(new FJsonValueObject(PinObj)));

                if (Pin->LinkedTo.Num() > 0)
                {
                    UEdGraphPin* ConnectedPin = Pin->LinkedTo[0];
                    if (ConnectedPin && ConnectedPin->GetOwningNode())
                    {
                        FString SourceHandle = ConnectedPin->GetOwningNode()->GetName();
                        if (UMaterialGraphNode* SourceMatNode = Cast<UMaterialGraphNode>(ConnectedPin->GetOwningNode()))
                        {
                            if (SourceMatNode->MaterialExpression)
                            {
                                SourceHandle = SourceMatNode->MaterialExpression->GetName();
                                if (!SourceMatNode->MaterialExpression->Desc.IsEmpty())
                                    SourceHandle = SourceMatNode->MaterialExpression->Desc;
                            }
                        }
                        ConnectionsObject->SetStringField(StableId, SourceHandle);
                    }
                }
            }
        }
    }
    else if (bIsRootAlias)
    {
        // === REFLECTION FALLBACK (When Graph is null or hidden) ===
        TArray<UObject*> SearchTargets;
#if WITH_EDITOR
        if (Mat->GetEditorOnlyData()) SearchTargets.Add((UObject*)Mat->GetEditorOnlyData());
#endif
        SearchTargets.Add(Mat);

        for (UObject* Target : SearchTargets)
        {
            for (TFieldIterator<FProperty> PropIt(Target->GetClass()); PropIt; ++PropIt)
            {
                FProperty* Prop = *PropIt;
                FString PropName = Prop->GetName();
                if (UniquePins.Contains(PropName)) continue;

                // Check if it's a Material Input (avoid broad "Input" match)
                FStructProperty* StructProp = CastField<FStructProperty>(Prop);
                if (StructProp && StructProp->Struct)
                {
                    FString StructName = StructProp->Struct->GetName();
                    if (StructName.Equals(TEXT("ExpressionInput")) || 
                        StructName.Contains(TEXT("MaterialInput")))
                    {
                        UniquePins.Add(PropName);
                        
                        TSharedPtr<FJsonObject> PinObj = MakeShareable(new FJsonObject());
                        PinObj->SetStringField(TEXT("name"), Prop->GetDisplayNameText().ToString());
                        PinObj->SetStringField(TEXT("id"), PropName);
                        PinsArray.Add(MakeShareable(new FJsonValueObject(PinObj)));

                        FExpressionInput* InputPtr = StructProp->ContainerPtrToValuePtr<FExpressionInput>(Target);
                        if (InputPtr && InputPtr->Expression)
                        {
                            FString SourceHandle = InputPtr->Expression->GetName();
                            if (!InputPtr->Expression->Desc.IsEmpty())
                                SourceHandle = InputPtr->Expression->Desc;
                            ConnectionsObject->SetStringField(PropName, SourceHandle);
                        }
                    }
                }
            }
        }
    }

    RootObject->SetArrayField(TEXT("pins"), PinsArray);
    RootObject->SetObjectField(TEXT("connections"), ConnectionsObject);

    FString OutputString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
    FJsonSerializer::Serialize(RootObject.ToSharedRef(), Writer);
    return OutputString;
}
    


UMaterialExpression* UUmgMcpMaterialSubsystem::FindExpressionByHandle(const FString& Handle)
{
    UMaterial* Mat = GetTargetMaterial();
    if (!Mat) return nullptr;

    for (UMaterialExpression* Expr : Mat->GetExpressions())
    {
        // Check Name
        if (Expr->GetName() == Handle) return Expr;
        
        // Check Desc
        if (Expr->Desc.Equals(Handle, ESearchCase::IgnoreCase)) return Expr;
    }
    
    return nullptr;
}

void UUmgMcpMaterialSubsystem::ForceRefreshMaterialEditor()
{
    UMaterial* Mat = GetTargetMaterial();
    if (!Mat) return;

    // 1. 数据一致性：仅标记改变，触发内部属性同步
    Mat->Modify();
    Mat->PostEditChange();
    Mat->MarkPackageDirty();

    // 2. 编辑器层通知：如果编辑器打开了，告知其数据已变，让属性面板等 UI 更新
    if (GEditor)
    {
        if (IAssetEditorInstance* Ed = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->FindEditorForAsset(Mat, false))
        {
            IMaterialEditor* MaterialEditor = (IMaterialEditor*)Ed;
            MaterialEditor->NotifyExternalMaterialChange();
            MaterialEditor->UpdateMaterialAfterGraphChange();
        }
    }

    // [DEFERRED COMPILATION] 移除 UMaterialEditingLibrary::RecompileMaterial(Mat);
    // 渲染刷新的重担交给独立的 material_compile 命令处理，以避免高频操作下的 FlushRenderingCommands 递归警告
}
