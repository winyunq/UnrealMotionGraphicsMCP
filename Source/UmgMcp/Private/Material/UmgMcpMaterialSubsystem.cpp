#include "Material/UmgMcpMaterialSubsystem.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Factories/MaterialFactoryNew.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionCustom.h"
#include "MaterialEditingLibrary.h" // Added for proper refresh
#include "Materials/MaterialExpressionComponentMask.h" 
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "MaterialEditingLibrary.h"
// Graph Editing
#include "MaterialGraph/MaterialGraph.h" 
#include "MaterialGraph/MaterialGraphNode.h"
#include "MaterialGraph/MaterialGraphNode_Root.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
// Material Editor
#include "IMaterialEditor.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Editor.h" 

#include "UObject/UObjectGlobals.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Editor.h" 
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"
#include "JsonObjectConverter.h"

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
    
    // 1. Try to find in open Editor FIRST (UMG pattern)
    if (GEditor)
    {
        if (UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>())
        {
            FSoftObjectPath ObjectPath(AssetPath);
            UObject* AssetObj = ObjectPath.ResolveObject();  // UMG uses ResolveObject, not TryLoad
            
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
                        return FString::Printf(TEXT("设置目标材质成功: %s (编辑器实例)"), *AssetPath);
                    }
                }
            }
        }
    }
    
    // 2. Fallback to LoadObject if not in editor (UMG pattern)
    if (!TargetMat)
    {
        TargetMat = LoadObject<UMaterial>(nullptr, *AssetPath, nullptr, LOAD_NoWarn);
    }
    
    if (TargetMat)
    {
        TargetMaterial = TargetMat;
        // UE_LOG(LogTemp, Warning, TEXT("[SetTargetMaterial] SUCCESS: Loaded from disk"));
        return FString::Printf(TEXT("设置目标材质成功: %s"), *AssetPath);
    }
    

    // 2. Create if Not Found and Allowed
    if (bCreateIfNotFound)
    {
        FString PackageName = AssetPath;
        FString AssetName = FPackageName::GetShortName(PackageName);
        
        if (!FPackageName::IsValidObjectPath(PackageName))
        {
            return FString::Printf(TEXT("错误: 无效的资产路径: %s"), *AssetPath);
        }

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
            NewMat->MaterialDomain = MD_UI;
            NewMat->BlendMode = BLEND_Translucent;
            
            FAssetRegistryModule::AssetCreated(NewMat);
            NewMat->MarkPackageDirty();
            TargetMaterial = NewMat;
            return FString::Printf(TEXT("创建并设置目标材质: %s"), *AssetPath);
        }
    }

    return FString::Printf(TEXT("错误: 找不到材质且未允许创建: %s"), *AssetPath);
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

    for (TFieldIterator<FProperty> PropIt(Owner->GetClass()); PropIt; ++PropIt)
    {
        FProperty* Prop = *PropIt;
        
        // Handle precise match or common aliases
        bool bMatch = Prop->GetName().Equals(PinName, ESearchCase::IgnoreCase);
        
        if (!bMatch)
        {
            // Auto-Resolution Aliases
            if (PinName.Equals(TEXT("UV"), ESearchCase::IgnoreCase) && Prop->GetName().Equals(TEXT("Coordinates"), ESearchCase::IgnoreCase)) bMatch = true;
            if (PinName.Equals(TEXT("Alpha"), ESearchCase::IgnoreCase) && Prop->GetName().Equals(TEXT("A"), ESearchCase::IgnoreCase)) bMatch = true;
        }

        if (bMatch)
        {
            // Check if it is an FExpressionInput structured property
            FStructProperty* StructProp = CastField<FStructProperty>(Prop);
            if (StructProp)
            {
                // STRATEGY: "Sparse Matrix" / "Best Effort"
                // In the context of a Material Graph, if we find a StructProperty with a matching name 
                // (e.g. "BaseColor", "Opacity", "UV"), it is almost certainly a Material Input.
                // We skip strict type validation (IsChildOf FExpressionInput) to avoid false negatives 
                // due to reflection quirks or derived struct hierarchies.
                // We implicitly trust that accessing the first members (Expression, OutputIndex) is safe 
                // because all Material Inputs inherit from FExpressionInput.
                return StructProp->ContainerPtrToValuePtr<FExpressionInput>(Owner);
            }
        }
    }
    return nullptr;
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
    bool bIsRootAlias = ToHandle.StartsWith(TEXT("Master")) || ToHandle.Equals(TEXT("MaterialRoot")) || ToHandle.Equals(Mat->GetName());

    if (bIsRootAlias && Mat->MaterialGraph)
    {
        // === ROOT NODE: Use Graph-based connection ===
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
            // UE_LOG(LogTemp, Error, TEXT("[ConnectPins] Root node not found"));
            return false;
        }

        // 3. Find Output Pin on Source
        UEdGraphPin* SourcePin = nullptr;
        if (FromPin.IsEmpty())
        {
            for (UEdGraphPin* Pin : SourceGraphNode->Pins)
            {
                if (Pin->Direction == EGPD_Output)
                {
                    SourcePin = Pin;
                    break;
                }
            }
        }
        else
        {
            for (UEdGraphPin* Pin : SourceGraphNode->Pins)
            {
                if (Pin->Direction == EGPD_Output && Pin->PinName.ToString().Equals(FromPin, ESearchCase::IgnoreCase))
                {
                    SourcePin = Pin;
                    break;
                }
            }
        }

        if (!SourcePin)
        {
            // UE_LOG(LogTemp, Error, TEXT("[ConnectPins] Source output pin not found"));
            return false;
        }

        // 4. Find Root Input Pin (使用PinCategory，语言无关)
        UEdGraphPin* TargetPin = nullptr;
        FString TargetPinName = ToPin;

        // Define Pin matching by Category (language-independent)
        auto FindPinByCategory = [&](const FString& Category, const FString& SubCategory = TEXT("")) -> UEdGraphPin*
        {
            for (UEdGraphPin* Pin : RootNode->Pins)
            {
                if (Pin->Direction == EGPD_Input)
                {
                    bool bCategoryMatch = Pin->PinType.PinCategory.ToString().Equals(Category, ESearchCase::IgnoreCase);
                    bool bSubCategoryMatch = SubCategory.IsEmpty() || Pin->PinType.PinSubCategory.ToString().Equals(SubCategory, ESearchCase::IgnoreCase);
                    
                    if (bCategoryMatch && bSubCategoryMatch)
                    {
                        return Pin;
                    }
                }
            }
            return nullptr;
        };

        // Map user-friendly names to PinCategory searches
        if (TargetPinName.IsEmpty() || 
            TargetPinName.Equals(TEXT("FinalColor"), ESearchCase::IgnoreCase) ||
            TargetPinName.Equals(TEXT("EmissiveColor"), ESearchCase::IgnoreCase) ||
            TargetPinName.Equals(TEXT("最终颜色"), ESearchCase::IgnoreCase))
        {
            // For UI materials: Find EmissiveColor/FinalColor (materialinput + rgba)
            // Try to find a pin with "Final" or "Emissive" in the name first
            for (UEdGraphPin* Pin : RootNode->Pins)
            {
                if (Pin->Direction == EGPD_Input &&
                    Pin->PinType.PinCategory.ToString().Equals(TEXT("materialinput")) &&
                    (Pin->PinName.ToString().Contains(TEXT("最终")) || 
                     Pin->PinName.ToString().Contains(TEXT("Final")) ||
                     Pin->PinName.ToString().Contains(TEXT("Emissive"))))
                {
                    TargetPin = Pin;
                    break;
                }
            }
            
            // Fallback: Find first rgba materialinput (usually BaseColor, but acceptable)
            if (!TargetPin)
            {
                TargetPin = FindPinByCategory(TEXT("materialinput"), TEXT("rgba"));
            }
        }
        else if (TargetPinName.Equals(TEXT("Opacity"), ESearchCase::IgnoreCase) ||
                 TargetPinName.Equals(TEXT("不透明度"), ESearchCase::IgnoreCase))
        {
            // Find Opacity (materialinput + red, contains "Opacity" or "不透明")
            for (UEdGraphPin* Pin : RootNode->Pins)
            {
                if (Pin->Direction == EGPD_Input &&
                    Pin->PinType.PinCategory.ToString().Equals(TEXT("materialinput")) &&
                    Pin->PinType.PinSubCategory.ToString().Equals(TEXT("red")) &&
                    (Pin->PinName.ToString().Contains(TEXT("不透明")) ||
                     Pin->PinName.ToString().Contains(TEXT("Opacity"))))
                {
                    TargetPin = Pin;
                    break;
                }
            }
        }
        else
        {
            // Direct name match fallback
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
            // UE_LOG(LogTemp, Error, TEXT("[ConnectPins] Root input pin not found for: %s"), *TargetPinName);
            return false;
        }

        // 5. Make Graph Connection
        SourcePin->MakeLinkTo(TargetPin);

        // UE_LOG(LogTemp, Warning, TEXT("[ConnectPins] Graph: Connected %s.%s -> Root.%s"), 
        //    *FromHandle, *SourcePin->PinName.ToString(), *TargetPin->PinName.ToString());

        // 6. Force Graph Refresh
        if (Mat->MaterialGraph)
        {
            Mat->MaterialGraph->NotifyGraphChanged();
        }
        Mat->PostEditChange();
        Mat->MarkPackageDirty();
        ForceRefreshMaterialEditor();

        return true;
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
        FString TargetPinName = ToPin;

        if (TargetPinName.IsEmpty())
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
            InputPtr = FindInputProperty(TargetObject, TargetPinName);
        }

        // Special Case: Custom Node Inputs
        UMaterialExpressionCustom* CustomNode = Cast<UMaterialExpressionCustom>(TargetObject);
        if (CustomNode && !InputPtr)
        {
            for (int32 i = 0; i < CustomNode->Inputs.Num(); i++)
            {
                if (CustomNode->Inputs[i].InputName.ToString().Equals(TargetPinName, ESearchCase::IgnoreCase))
                {
                    InputPtr = &(CustomNode->Inputs[i].Input);
                    break;
                }
            }
        }

        if (InputPtr)
        {
            InputPtr->Expression = FromNode;
            InputPtr->OutputIndex = 0;
            
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
    UMaterialExpression* Expr = FindExpressionByHandle(NodeHandle);
    if (!Expr || !Properties) return false;

    // Use FJsonObjectConverter to apply properties
    // Note: FJsonObjectConverter handles basic types. Structs might be tricky.
    
    for (const auto& Elem : Properties->Values)
    {
        FString PropName = Elem.Key;
        TSharedPtr<FJsonValue> JsonVal = Elem.Value;
        
        FProperty* Prop = Expr->GetClass()->FindPropertyByName(*PropName);
        if (Prop)
        {
             // Simple types
             if (FFloatProperty* FloatProp = CastField<FFloatProperty>(Prop))
             {
                 FloatProp->SetPropertyValue_InContainer(Expr, JsonVal->AsNumber());
             }
             else if (FDoubleProperty* DblProp = CastField<FDoubleProperty>(Prop))
             {
                 DblProp->SetPropertyValue_InContainer(Expr, JsonVal->AsNumber());
             }
             else if (FIntProperty* IntProp = CastField<FIntProperty>(Prop))
             {
                 IntProp->SetPropertyValue_InContainer(Expr, (int)JsonVal->AsNumber());
             }
             // TODO: Vector, Texture, etc.
        }
    }
    
    Expr->PostEditChange();
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
        if (bSuccess && Mat->MaterialDomain == MD_UI)
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

FString UUmgMcpMaterialSubsystem::GetNodeInfo(const FString& NodeHandle)
{
    UMaterial* Mat = GetTargetMaterial();
    if (!Mat) return TEXT("{}");

    // Ensure we have a valid Graph to look at
    if (!Mat->MaterialGraph)
    {
        return TEXT("{\"error\": \"Material Graph is null. Is Editor loaded?\"}");
    }

    UEdGraphNode* TargetNode = nullptr;

    // 1. Resolve Target Node in the Graph
    // Check for Root Alias
    bool bIsRootAlias = NodeHandle.StartsWith(TEXT("Master")) || NodeHandle.Equals(TEXT("MaterialRoot")) || NodeHandle.Equals(Mat->GetName());
    
    if (bIsRootAlias)
    {
        // Find Root Node
        for (UEdGraphNode* Node : Mat->MaterialGraph->Nodes)
        {
            if (Node->IsA(UMaterialGraphNode_Root::StaticClass()))
            {
                TargetNode = Node;
                break;
            }
        }
    }
    else
    {
        // Find Expression Node
        // Note: NodeHandle might be the Expression Name, but GraphNodes contain Expressions.
        // We need to match GraphNode -> MaterialExpression -> Name
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

    if (!TargetNode)
    {
        return FString::Printf(TEXT("{\"error\": \"Node not found in Graph: %s\"}"), *NodeHandle);
    }

    // 2. Dump Pins and Connections from Graph Node
    TSharedPtr<FJsonObject> RootObject = MakeShareable(new FJsonObject());
    TArray<TSharedPtr<FJsonValue>> PinsArray;
    TSharedPtr<FJsonObject> ConnectionsObject = MakeShareable(new FJsonObject());

    for (UEdGraphPin* Pin : TargetNode->Pins)
    {
        // Only interested in Inputs for "Pins" list usually, but let's dump all?
        // User wants to know what they can connect TO. So Inputs.
        if (Pin->Direction == EGPD_Input)
        {
             PinsArray.Add(MakeShareable(new FJsonValueString(Pin->PinName.ToString())));
             
             // Check Connections
             if (Pin->LinkedTo.Num() > 0)
             {
                 // It connects TO something.
                 // In Material Graph, Input Pin connects to an Output Pin of another Node.
                 UEdGraphPin* ConnectedPin = Pin->LinkedTo[0];
                 if (ConnectedPin && ConnectedPin->GetOwningNode())
                 {
                     UEdGraphNode* SourceNode = ConnectedPin->GetOwningNode();
                     FString SourceName = SourceNode->GetNodeTitle(ENodeTitleType::ListView).ToString();
                     
                     // Try to get the actual Expression Name for code usage
                     if (UMaterialGraphNode* SourceMatNode = Cast<UMaterialGraphNode>(SourceNode))
                     {
                         if (SourceMatNode->MaterialExpression)
                         {
                             SourceName = SourceMatNode->MaterialExpression->GetName();
                             if (!SourceMatNode->MaterialExpression->Desc.IsEmpty())
                             {
                                 SourceName = SourceMatNode->MaterialExpression->Desc;
                             }
                         }
                     }
                     
                     ConnectionsObject->SetStringField(Pin->PinName.ToString(), SourceName);
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
