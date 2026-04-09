// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#include "Material/UmgMcpMaterialCommands.h"
#include "Material/UmgMcpMaterialSubsystem.h" // Required for Subsystem usage
#include "Editor.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionParameter.h"
#include "Materials/MaterialExpressionTextureSampleParameter.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "MaterialEditingLibrary.h"
#include "UObject/UnrealType.h"

namespace
{
    constexpr const TCHAR* DefaultBootstrapHlsl = TEXT("return float4(1,1,1,1);");
    constexpr const TCHAR* UnknownParamKind = TEXT("Unknown");
    constexpr const TCHAR* DefaultParamKind = TEXT("Scalar");
    constexpr const TCHAR* MasterHandleName = TEXT("Master");
    constexpr const TCHAR* FinalColorPinName = TEXT("FinalColor");
    constexpr const TCHAR* OpacityPinName = TEXT("Opacity");

    static bool IsErrorStatus(const FString& Status)
    {
        return Status.StartsWith(TEXT("Error")) || Status.StartsWith(TEXT("错误"));
    }

    static FString ResolveHandleFromExpression(const UMaterialExpression* Expr)
    {
        if (!Expr)
        {
            return TEXT("");
        }
        return Expr->Desc.IsEmpty() ? Expr->GetName() : Expr->Desc;
    }

    static FExpressionInput* FindMaterialInputByName(UMaterial* Mat, const FString& InputName)
    {
        if (!Mat)
        {
            return nullptr;
        }

        auto FindInObject = [&](UObject* Owner) -> FExpressionInput*
        {
            if (!Owner)
            {
                return nullptr;
            }

            for (TFieldIterator<FProperty> It(Owner->GetClass()); It; ++It)
            {
                FProperty* Prop = *It;
                const bool bNameMatch =
                    Prop->GetName().Equals(InputName, ESearchCase::IgnoreCase) ||
                    Prop->GetDisplayNameText().ToString().Equals(InputName, ESearchCase::IgnoreCase);

                if (!bNameMatch)
                {
                    continue;
                }

                if (FStructProperty* StructProp = CastField<FStructProperty>(Prop))
                {
                    if (StructProp->Struct &&
                        (StructProp->Struct->GetName().Equals(TEXT("ExpressionInput")) ||
                         StructProp->Struct->GetName().Contains(TEXT("MaterialInput"))))
                    {
                        return StructProp->ContainerPtrToValuePtr<FExpressionInput>(Owner);
                    }
                }
            }
            return nullptr;
        };

#if WITH_EDITOR
        if (Mat->GetEditorOnlyData())
        {
            if (FExpressionInput* Input = FindInObject((UObject*)Mat->GetEditorOnlyData()))
            {
                return Input;
            }
        }
#endif
        return FindInObject(Mat);
    }

    static FString DetectParamKind(const UMaterialExpression* Expr)
    {
        if (Cast<UMaterialExpressionScalarParameter>(Expr))
        {
            return DefaultParamKind;
        }
        if (Cast<UMaterialExpressionVectorParameter>(Expr))
        {
            return TEXT("Vector");
        }
        if (Cast<UMaterialExpressionTextureSampleParameter>(Expr))
        {
            return TEXT("Texture");
        }
        return UnknownParamKind;
    }

    static UMaterialExpressionCustom* FindSingleCustomNode(UMaterial* Mat)
    {
        if (!Mat)
        {
            return nullptr;
        }

        UMaterialExpressionCustom* Found = nullptr;
        for (UMaterialExpression* Expr : Mat->GetExpressions())
        {
            if (UMaterialExpressionCustom* Custom = Cast<UMaterialExpressionCustom>(Expr))
            {
                if (Found)
                {
                    return nullptr;
                }
                Found = Custom;
            }
        }
        return Found;
    }

    static bool IsValidHlslTarget(UMaterial* Mat, UMaterialExpressionCustom*& OutCustomNode, FString& OutReason)
    {
        OutCustomNode = nullptr;
        if (!Mat)
        {
            OutReason = TEXT("No target material");
            return false;
        }

        if (Mat->MaterialDomain != MD_UI)
        {
            OutReason = TEXT("Target material is not a UI material (MaterialDomain != MD_UI)");
            return false;
        }

        UMaterialExpressionCustom* CustomNode = FindSingleCustomNode(Mat);
        if (!CustomNode)
        {
            OutReason = TEXT("Material must contain exactly one Custom HLSL node");
            return false;
        }

        FExpressionInput* FinalColorInput = FindMaterialInputByName(Mat, TEXT("EmissiveColor"));
        FExpressionInput* OpacityInput = FindMaterialInputByName(Mat, OpacityPinName);
        const bool bHasFinalColor = FinalColorInput && FinalColorInput->Expression != nullptr;
        const bool bHasOpacity = OpacityInput && OpacityInput->Expression != nullptr;

        if (!bHasFinalColor && !bHasOpacity)
        {
            OutReason = TEXT("UI output is not connected");
            return false;
        }

        OutCustomNode = CustomNode;
        OutReason = TEXT("ok");
        return true;
    }

    static bool ResetToSingleHlslTopology(UUmgMcpMaterialSubsystem* Subsystem, UMaterial* Mat, UMaterialExpressionCustom*& OutCustomNode, FString& OutError)
    {
        OutCustomNode = nullptr;
        if (!Subsystem || !Mat)
        {
            OutError = TEXT("No material target");
            return false;
        }

#if WITH_EDITOR
        if (Mat->GetEditorOnlyData())
        {
            Mat->GetEditorOnlyData()->ExpressionCollection.Expressions.Empty();
        }
#endif

        UMaterialExpressionCustom* CustomNode = Cast<UMaterialExpressionCustom>(
            UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionCustom::StaticClass()));
        UMaterialExpressionComponentMask* RgbMask = Cast<UMaterialExpressionComponentMask>(
            UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionComponentMask::StaticClass()));
        UMaterialExpressionComponentMask* AlphaMask = Cast<UMaterialExpressionComponentMask>(
            UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionComponentMask::StaticClass()));

        if (!CustomNode || !RgbMask || !AlphaMask)
        {
            OutError = TEXT("Failed to create required topology nodes");
            return false;
        }

        CustomNode->Desc = TEXT("HLSL_Core");
        if (CustomNode->Code.IsEmpty())
        {
            CustomNode->Code = DefaultBootstrapHlsl;
        }

        RgbMask->Desc = TEXT("HLSL_RGB");
        RgbMask->R = true;
        RgbMask->G = true;
        RgbMask->B = true;
        RgbMask->A = false;

        AlphaMask->Desc = TEXT("HLSL_A");
        AlphaMask->R = false;
        AlphaMask->G = false;
        AlphaMask->B = false;
        AlphaMask->A = true;

        const FString CustomHandle = CustomNode->GetName();
        const FString RgbHandle = RgbMask->GetName();
        const FString AlphaHandle = AlphaMask->GetName();

        const bool bLinkOk =
            Subsystem->ConnectPins(CustomHandle, TEXT(""), RgbHandle, TEXT("Input")) &&
            Subsystem->ConnectPins(CustomHandle, TEXT(""), AlphaHandle, TEXT("Input")) &&
            Subsystem->ConnectPins(RgbHandle, TEXT(""), MasterHandleName, FinalColorPinName) &&
            Subsystem->ConnectPins(AlphaHandle, TEXT(""), MasterHandleName, OpacityPinName);

        if (!bLinkOk)
        {
            OutError = TEXT("Failed to auto-wire HLSL output topology");
            return false;
        }

        OutCustomNode = CustomNode;
        OutError = TEXT("");
        return true;
    }

    static void BuildParameterSnapshot(UMaterialExpressionCustom* CustomNode, TArray<TSharedPtr<FJsonValue>>& OutParameters)
    {
        OutParameters.Empty();
        if (!CustomNode)
        {
            return;
        }

        for (const FCustomInput& Input : CustomNode->Inputs)
        {
            TSharedPtr<FJsonObject> ParamObj = MakeShareable(new FJsonObject);
            const FString Name = Input.InputName.ToString();
            ParamObj->SetStringField(TEXT("name"), Name);

            UMaterialExpression* SourceExpr = Input.Input.Expression;
            ParamObj->SetStringField(TEXT("kind"), DetectParamKind(SourceExpr));
            ParamObj->SetBoolField(TEXT("connected"), SourceExpr != nullptr);
            ParamObj->SetStringField(TEXT("source_handle"), ResolveHandleFromExpression(SourceExpr));
            ParamObj->SetStringField(TEXT("custom_input"), Name);
            OutParameters.Add(MakeShareable(new FJsonValueObject(ParamObj)));
        }
    }

    struct FHlslParamDraft
    {
        FString Name;
        FString Kind;
        bool bDelete = false;
    };

    static void ParseParameterDrafts(const TArray<TSharedPtr<FJsonValue>>& JsonParams, TArray<FHlslParamDraft>& OutDrafts)
    {
        OutDrafts.Empty();
        for (const TSharedPtr<FJsonValue>& JsonVal : JsonParams)
        {
            FHlslParamDraft Draft;
            Draft.Kind = DefaultParamKind;

            if (JsonVal->Type == EJson::String)
            {
                Draft.Name = JsonVal->AsString();
            }
            else if (JsonVal->Type == EJson::Object)
            {
                const TSharedPtr<FJsonObject> Obj = JsonVal->AsObject();
                if (!Obj.IsValid())
                {
                    continue;
                }
                Obj->TryGetStringField(TEXT("name"), Draft.Name);
                Obj->TryGetStringField(TEXT("kind"), Draft.Kind);
                bool bDeleteFlag = false;
                Obj->TryGetBoolField(TEXT("delete"), bDeleteFlag);
                FString Action;
                Obj->TryGetStringField(TEXT("action"), Action);
                Draft.bDelete = bDeleteFlag || Action.Equals(TEXT("delete"), ESearchCase::IgnoreCase);
            }

            if (!Draft.Name.IsEmpty())
            {
                OutDrafts.Add(Draft);
            }
        }
    }
}

FUmgMcpMaterialCommands::FUmgMcpMaterialCommands()
{
}

FUmgMcpMaterialCommands::~FUmgMcpMaterialCommands()
{
}

UUmgMcpMaterialSubsystem* FUmgMcpMaterialCommands::GetSubsystem() const
{
    if (GEditor)
    {
        return GEditor->GetEditorSubsystem<UUmgMcpMaterialSubsystem>();
    }
    return nullptr;
}

TSharedPtr<FJsonObject> FUmgMcpMaterialCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> ResultJson = MakeShareable(new FJsonObject);
    UUmgMcpMaterialSubsystem* Subsystem = GetSubsystem();

    if (!Subsystem)
    {
        ResultJson->SetStringField(TEXT("error"), TEXT("Material Subsystem not available"));
        ResultJson->SetBoolField(TEXT("success"), false);
        return ResultJson;
    }

    // --- P0: Context ---
    if (CommandType == TEXT("material_set_target"))
    {
        FString Path;
        if (Params->TryGetStringField(TEXT("path"), Path))
        {
            // Default to always trying to create/load (Context Anchor)
            FString Status = Subsystem->SetTargetMaterial(Path, true);
            
            if (IsErrorStatus(Status))
            {
                 ResultJson->SetStringField(TEXT("error"), Status);
                 ResultJson->SetBoolField(TEXT("success"), false);
            }
            else
            {
                 ResultJson->SetStringField(TEXT("target"), Path);
                 // "创建" appears only in the creation message "创建并设置目标材质: ..."
                 FString Action = Status.Contains(TEXT("创建")) ? TEXT("created") : TEXT("loaded");
                 ResultJson->SetStringField(TEXT("action"), Action);
                 ResultJson->SetBoolField(TEXT("success"), true);
            }
        }
        else
        {
            ResultJson->SetStringField(TEXT("error"), TEXT("Missing 'path' parameter"));
            ResultJson->SetBoolField(TEXT("success"), false);
        }
    }
    else if (CommandType == TEXT("hlsl_set_target"))
    {
        FString Path;
        bool bConfirmOverwrite = false;
        Params->TryGetBoolField(TEXT("confirm_overwrite"), bConfirmOverwrite);
        bool bCreateIfNotFoundValue = true;
        if (Params->HasField(TEXT("create_if_not_found")))
        {
            Params->TryGetBoolField(TEXT("create_if_not_found"), bCreateIfNotFoundValue);
        }
        const bool bCreateIfNotFound = bCreateIfNotFoundValue;

        if (!Params->TryGetStringField(TEXT("path"), Path))
        {
            ResultJson->SetStringField(TEXT("error"), TEXT("Missing 'path' parameter"));
            ResultJson->SetBoolField(TEXT("success"), false);
            return ResultJson;
        }

        FString LoadStatus = Subsystem->SetTargetMaterial(Path, false);
        UMaterial* Mat = nullptr;
        UMaterialExpressionCustom* CustomNode = nullptr;
        FString ValidationReason;

        if (!IsErrorStatus(LoadStatus))
        {
            Mat = Subsystem->GetTargetMaterial();
            if (IsValidHlslTarget(Mat, CustomNode, ValidationReason))
            {
                ResultJson->SetBoolField(TEXT("success"), true);
                ResultJson->SetStringField(TEXT("target"), Path);
                ResultJson->SetStringField(TEXT("action"), TEXT("loaded"));
                ResultJson->SetStringField(TEXT("custom_node_handle"), ResolveHandleFromExpression(CustomNode));
                ResultJson->SetStringField(TEXT("message"), TEXT("HLSL target ready"));
            }
            else if (!bConfirmOverwrite)
            {
                ResultJson->SetBoolField(TEXT("success"), false);
                ResultJson->SetBoolField(TEXT("requires_confirmation"), true);
                ResultJson->SetStringField(TEXT("target"), Path);
                ResultJson->SetStringField(TEXT("error"), FString::Printf(TEXT("Target does not match HLSL protocol (%s). Overwrite target file and continue?"), *ValidationReason));
            }
            else
            {
                FString BootstrapError;
                if (ResetToSingleHlslTopology(Subsystem, Mat, CustomNode, BootstrapError))
                {
                    ResultJson->SetBoolField(TEXT("success"), true);
                    ResultJson->SetStringField(TEXT("target"), Path);
                    ResultJson->SetStringField(TEXT("action"), TEXT("overwritten"));
                    ResultJson->SetStringField(TEXT("custom_node_handle"), ResolveHandleFromExpression(CustomNode));
                    ResultJson->SetStringField(TEXT("message"), TEXT("HLSL topology overwritten successfully"));
                }
                else
                {
                    ResultJson->SetBoolField(TEXT("success"), false);
                    ResultJson->SetStringField(TEXT("error"), BootstrapError);
                }
            }
        }
        else if (bCreateIfNotFound)
        {
            FString CreateStatus = Subsystem->SetTargetMaterial(Path, true);
            if (IsErrorStatus(CreateStatus))
            {
                ResultJson->SetBoolField(TEXT("success"), false);
                ResultJson->SetStringField(TEXT("error"), CreateStatus);
            }
            else
            {
                Mat = Subsystem->GetTargetMaterial();
                FString BootstrapError;
                if (ResetToSingleHlslTopology(Subsystem, Mat, CustomNode, BootstrapError))
                {
                    ResultJson->SetBoolField(TEXT("success"), true);
                    ResultJson->SetStringField(TEXT("target"), Path);
                    ResultJson->SetStringField(TEXT("action"), TEXT("created"));
                    ResultJson->SetStringField(TEXT("custom_node_handle"), ResolveHandleFromExpression(CustomNode));
                    ResultJson->SetStringField(TEXT("message"), TEXT("Created and initialized HLSL target"));
                }
                else
                {
                    ResultJson->SetBoolField(TEXT("success"), false);
                    ResultJson->SetStringField(TEXT("error"), BootstrapError);
                }
            }
        }
        else
        {
            ResultJson->SetBoolField(TEXT("success"), false);
            ResultJson->SetStringField(TEXT("error"), LoadStatus);
        }
    }
    else if (CommandType == TEXT("hlsl_get"))
    {
        UMaterial* Mat = Subsystem->GetTargetMaterial();
        UMaterialExpressionCustom* CustomNode = nullptr;
        FString ValidationReason;
        if (!IsValidHlslTarget(Mat, CustomNode, ValidationReason))
        {
            ResultJson->SetBoolField(TEXT("success"), false);
            ResultJson->SetStringField(TEXT("error"), FString::Printf(TEXT("Target is not HLSL-protocol ready: %s"), *ValidationReason));
            return ResultJson;
        }

        TArray<TSharedPtr<FJsonValue>> ParamsArray;
        BuildParameterSnapshot(CustomNode, ParamsArray);

        TSharedPtr<FJsonObject> OutputContract = MakeShareable(new FJsonObject);
        OutputContract->SetStringField(TEXT("return"), TEXT("float4"));
        OutputContract->SetStringField(TEXT("rgb_to"), FinalColorPinName);
        OutputContract->SetStringField(TEXT("a_to"), OpacityPinName);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("target"), Mat ? Mat->GetPathName() : TEXT(""));
        ResultJson->SetStringField(TEXT("custom_node_handle"), ResolveHandleFromExpression(CustomNode));
        ResultJson->SetStringField(TEXT("hlsl"), CustomNode ? CustomNode->Code : TEXT(""));
        ResultJson->SetArrayField(TEXT("parameters"), ParamsArray);
        ResultJson->SetObjectField(TEXT("output_contract"), OutputContract);
    }
    else if (CommandType == TEXT("hlsl_set"))
    {
        UMaterial* Mat = Subsystem->GetTargetMaterial();
        UMaterialExpressionCustom* CustomNode = nullptr;
        FString ValidationReason;
        if (!IsValidHlslTarget(Mat, CustomNode, ValidationReason))
        {
            ResultJson->SetBoolField(TEXT("success"), false);
            ResultJson->SetStringField(TEXT("error"), FString::Printf(TEXT("Target is not HLSL-protocol ready: %s. Call hlsl_set_target first."), *ValidationReason));
            return ResultJson;
        }

        FString NewHlsl;
        const bool bHasHlsl = Params->TryGetStringField(TEXT("hlsl"), NewHlsl);
        const TArray<TSharedPtr<FJsonValue>>* NewParametersJson = nullptr;
        const bool bHasParameters = Params->TryGetArrayField(TEXT("parameters"), NewParametersJson);

        if (!bHasHlsl && !bHasParameters)
        {
            ResultJson->SetBoolField(TEXT("success"), false);
            ResultJson->SetStringField(TEXT("error"), TEXT("Provide at least one of 'hlsl' or 'parameters'"));
            return ResultJson;
        }

        // Collect existing parameters from current custom input list
        TArray<FHlslParamDraft> WorkingParams;
        for (const FCustomInput& Input : CustomNode->Inputs)
        {
            FHlslParamDraft Draft;
            Draft.Name = Input.InputName.ToString();
            Draft.Kind = DetectParamKind(Input.Input.Expression);
            WorkingParams.Add(Draft);
        }

        if (bHasParameters)
        {
            TArray<FHlslParamDraft> Incoming;
            ParseParameterDrafts(*NewParametersJson, Incoming);

            bool bRequestedDeletion = false;
            for (const FHlslParamDraft& Item : Incoming)
            {
                if (Item.bDelete)
                {
                    bRequestedDeletion = true;
                    break;
                }
            }

            if (bRequestedDeletion)
            {
                ResultJson->SetBoolField(TEXT("success"), false);
                ResultJson->SetStringField(TEXT("error"), TEXT("Append-only mode: HLSL parameter deletion is disabled. Send additive or overwrite updates only."));
                return ResultJson;
            }

            for (const FHlslParamDraft& Item : Incoming)
            {
                int32 ExistingIndex = INDEX_NONE;
                for (int32 i = 0; i < WorkingParams.Num(); ++i)
                {
                    if (WorkingParams[i].Name.Equals(Item.Name, ESearchCase::IgnoreCase))
                    {
                        ExistingIndex = i;
                        break;
                    }
                }

                if (Item.bDelete)
                {
                    if (ExistingIndex != INDEX_NONE)
                    {
                        WorkingParams.RemoveAt(ExistingIndex);
                    }
                    continue;
                }

                if (ExistingIndex != INDEX_NONE)
                {
                    if (!Item.Kind.IsEmpty())
                    {
                        WorkingParams[ExistingIndex].Kind = Item.Kind;
                    }
                }
                else
                {
                    WorkingParams.Add(Item);
                }
            }
        }

        TArray<FString> FinalInputNames;
        FinalInputNames.Reserve(WorkingParams.Num());
        for (const FHlslParamDraft& Param : WorkingParams)
        {
            FinalInputNames.Add(Param.Name);
        }

        const FString EffectiveCode = bHasHlsl ? NewHlsl : CustomNode->Code;
        if (!Subsystem->SetCustomNodeHLSL(CustomNode->GetName(), EffectiveCode, FinalInputNames))
        {
            ResultJson->SetBoolField(TEXT("success"), false);
            ResultJson->SetStringField(TEXT("error"), TEXT("Failed to set HLSL code/inputs"));
            return ResultJson;
        }

        // Re-wire parameters to Custom inputs
        for (const FHlslParamDraft& Param : WorkingParams)
        {
            FString Kind = Param.Kind.IsEmpty() || Param.Kind.Equals(UnknownParamKind, ESearchCase::IgnoreCase) ? DefaultParamKind : Param.Kind;
            FString ParamHandle = Subsystem->DefineVariable(Param.Name, Kind);
            if (IsErrorStatus(ParamHandle))
            {
                ResultJson->SetBoolField(TEXT("success"), false);
                ResultJson->SetStringField(TEXT("error"), FString::Printf(TEXT("Failed to define parameter '%s': %s"), *Param.Name, *ParamHandle));
                return ResultJson;
            }
            Subsystem->ConnectPins(ParamHandle, TEXT(""), CustomNode->GetName(), Param.Name);
        }

        // Re-validate and expose resulting parameter list
        UMaterialExpressionCustom* NewCustomNode = FindSingleCustomNode(Subsystem->GetTargetMaterial());
        TArray<TSharedPtr<FJsonValue>> ParametersOut;
        BuildParameterSnapshot(NewCustomNode, ParametersOut);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetBoolField(TEXT("hlsl_updated"), bHasHlsl);
        ResultJson->SetBoolField(TEXT("parameters_updated"), bHasParameters);
        ResultJson->SetStringField(TEXT("custom_node_handle"), ResolveHandleFromExpression(NewCustomNode));
        ResultJson->SetArrayField(TEXT("parameters"), ParametersOut);
    }
    else if (CommandType == TEXT("hlsl_compile"))
    {
        const FString Status = Subsystem->CompileAsset();
        const bool bOk = !IsErrorStatus(Status);
        ResultJson->SetBoolField(TEXT("success"), bOk);
        ResultJson->SetStringField(TEXT("status"), bOk ? TEXT("ok") : TEXT("error"));
        ResultJson->SetStringField(TEXT("compile_message"), Status);
        if (!bOk)
        {
            ResultJson->SetStringField(TEXT("error"), Status);
        }
    }
    // --- P0: Context & Search ---
    
    // Command: material_get_pins
    // Params: { "handle": "Master" | "NodeHandle" }
    else if (CommandType == TEXT("material_get_pins"))
    {
        FString Handle;
        if (Params->TryGetStringField(TEXT("handle"), Handle))
        {
            FString InfoJsonStr = Subsystem->GetNodeInfo(Handle);
            
            // Embed the result. The ResultJson already has "success".
            // We parse InfoJsonStr and add it to ResultJson.
            TSharedPtr<FJsonObject> InfoObj;
            TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(InfoJsonStr);
            if (FJsonSerializer::Deserialize(Reader, InfoObj))
            {
                 // Relay Error if present
                 if (InfoObj->HasField(TEXT("error")))
                 {
                     ResultJson->SetStringField(TEXT("error"), InfoObj->GetStringField(TEXT("error")));
                     ResultJson->SetBoolField(TEXT("success"), false);
                 }
                 else
                 {
                     // Flatten: Add "pins", "connections" and "name" to the top level result
                     if (InfoObj->HasField(TEXT("pins"))) 
                         ResultJson->SetArrayField(TEXT("pins"), InfoObj->GetArrayField(TEXT("pins")));
                         
                     if (InfoObj->HasField(TEXT("connections")))
                         ResultJson->SetObjectField(TEXT("connections"), InfoObj->GetObjectField(TEXT("connections")));
                         
                     if (InfoObj->HasField(TEXT("name")))
                         ResultJson->SetStringField(TEXT("name"), InfoObj->GetStringField(TEXT("name")));
                     
                     ResultJson->SetBoolField(TEXT("success"), true);
                 }
            }
            else
            {
                ResultJson->SetStringField(TEXT("error"), TEXT("Failed to deserialize node info"));
                ResultJson->SetBoolField(TEXT("success"), false);
            }
        }
        else
        {
             ResultJson->SetStringField(TEXT("error"), TEXT("Missing 'handle' parameter"));
             ResultJson->SetBoolField(TEXT("success"), false);
        }
    }

    // --- P1: Input Definition ---
    else if (CommandType == TEXT("material_define_variable"))
    {
        FString Name, Type;
        if (Params->TryGetStringField(TEXT("name"), Name) && Params->TryGetStringField(TEXT("type"), Type))
        {
            FString Handle = Subsystem->DefineVariable(Name, Type);
            if (Handle.StartsWith(TEXT("Error")))
            {
                ResultJson->SetStringField(TEXT("error"), Handle);
                ResultJson->SetBoolField(TEXT("success"), false);
            }
            else
            {
                ResultJson->SetStringField(TEXT("handle"), Handle);
                ResultJson->SetBoolField(TEXT("success"), true);
            }
        }
        else
        {
            ResultJson->SetStringField(TEXT("error"), TEXT("Missing 'name' or 'type'"));
            ResultJson->SetBoolField(TEXT("success"), false);
        }
    }
    // --- P2: Node Management ---
    else if (CommandType == TEXT("material_add_node"))
    {
        FString Symbol, HandleName;
        Params->TryGetStringField(TEXT("symbol"), Symbol);
        Params->TryGetStringField(TEXT("handle"), HandleName); // Optional
        
        if (!Symbol.IsEmpty())
        {
            FString NewHandle = Subsystem->AddNode(Symbol, HandleName);
            if (NewHandle.StartsWith(TEXT("Error")))
            {
                ResultJson->SetStringField(TEXT("error"), NewHandle);
                ResultJson->SetBoolField(TEXT("success"), false);
            }
            else
            {
                ResultJson->SetStringField(TEXT("handle"), NewHandle);
                ResultJson->SetBoolField(TEXT("success"), true);
            }
        }
        else
        {
            ResultJson->SetStringField(TEXT("error"), TEXT("Missing 'symbol'"));
            ResultJson->SetBoolField(TEXT("success"), false);
        }
    }
    else if (CommandType == TEXT("material_delete"))
    {
        ResultJson->SetBoolField(TEXT("success"), false);
        ResultJson->SetStringField(TEXT("error"), TEXT("Append-only mode: material_delete is disabled. Use additive node updates instead."));
    }
    // --- P3: Connections ---
    else if (CommandType == TEXT("material_connect_nodes"))
    {
        FString From, To;
        if (Params->TryGetStringField(TEXT("from"), From) && Params->TryGetStringField(TEXT("to"), To))
        {
            bool bSuccess = Subsystem->ConnectNodes(From, To);
            ResultJson->SetBoolField(TEXT("success"), bSuccess);
            if (bSuccess)
            {
                ResultJson->SetStringField(TEXT("from"), From);
                ResultJson->SetStringField(TEXT("to"), To);
            }
            else
            {
                ResultJson->SetStringField(TEXT("error"), TEXT("Failed to connect nodes. Check Handles."));
            }
        }
        else
        {
             ResultJson->SetStringField(TEXT("error"), TEXT("Missing 'from' or 'to' parameters"));
             ResultJson->SetBoolField(TEXT("success"), false);
        }
    }
    else if (CommandType == TEXT("material_connect_pins"))
    {
        FString Source, Target, SourcePin, TargetPin;
        Params->TryGetStringField(TEXT("source"), Source);
        Params->TryGetStringField(TEXT("target"), Target);
        Params->TryGetStringField(TEXT("source_pin"), SourcePin); // Optional
        Params->TryGetStringField(TEXT("target_pin"), TargetPin);
        
        bool bSuccess = Subsystem->ConnectPins(Source, SourcePin, Target, TargetPin);
        ResultJson->SetBoolField(TEXT("success"), bSuccess);
        if (bSuccess)
        {
            ResultJson->SetStringField(TEXT("source"), Source);
            if (!SourcePin.IsEmpty()) ResultJson->SetStringField(TEXT("source_pin"), SourcePin);
            ResultJson->SetStringField(TEXT("target"), Target);
            ResultJson->SetStringField(TEXT("target_pin"), TargetPin);
        }
        else
        {
            ResultJson->SetStringField(TEXT("error"), TEXT("Failed to connect pins. Check Pin Names."));
        }
    }
    // --- P5: Detail Injection ---
    else if (CommandType == TEXT("material_set_hlsl_node_io"))
    {
        FString Handle, Code;
        const TArray<TSharedPtr<FJsonValue>>* InputsArray;
        
        if (Params->TryGetStringField(TEXT("handle"), Handle) && 
            Params->TryGetStringField(TEXT("code"), Code) &&
            Params->TryGetArrayField(TEXT("inputs"), InputsArray))
        {
            TArray<FString> Inputs;
            for (auto Val : *InputsArray)
            {
                Inputs.Add(Val->AsString());
            }
            
            bool bSuccess = Subsystem->SetCustomNodeHLSL(Handle, Code, Inputs);
            ResultJson->SetBoolField(TEXT("success"), bSuccess);
            if (bSuccess)
            {
                ResultJson->SetStringField(TEXT("handle"), Handle);
                ResultJson->SetNumberField(TEXT("code_length"), Code.Len());
                TArray<TSharedPtr<FJsonValue>> InputsJson;
                for (const FString& Input : Inputs) InputsJson.Add(MakeShareable(new FJsonValueString(Input)));
                ResultJson->SetArrayField(TEXT("inputs"), InputsJson);
            }
        }
        else
        {
             ResultJson->SetStringField(TEXT("error"), TEXT("Missing parameters for HLSL injection"));
             ResultJson->SetBoolField(TEXT("success"), false);
        }
    }
    else if (CommandType == TEXT("material_set_node_properties"))
    {
        FString Handle;
        const TSharedPtr<FJsonObject>* Props;
        
        if (Params->TryGetStringField(TEXT("handle"), Handle) && 
            Params->TryGetObjectField(TEXT("properties"), Props))
        {
             bool bSuccess = Subsystem->SetNodeProperties(Handle, *Props);
             ResultJson->SetBoolField(TEXT("success"), bSuccess);
             if (bSuccess)
             {
                 ResultJson->SetStringField(TEXT("handle"), Handle);
                 ResultJson->SetObjectField(TEXT("properties_set"), *Props);
             }
        }
        else
        {
             ResultJson->SetStringField(TEXT("error"), TEXT("Missing handle or properties"));
             ResultJson->SetBoolField(TEXT("success"), false);
        }
    }
    // --- P6: Output ---
    else if (CommandType == TEXT("material_set_output_node"))
    {
        FString Handle;
        if (Params->TryGetStringField(TEXT("handle"), Handle))
        {
             bool bSuccess = Subsystem->SetOutputNode(Handle);
             ResultJson->SetBoolField(TEXT("success"), bSuccess);
             if (!bSuccess) ResultJson->SetStringField(TEXT("error"), TEXT("Failed to set output node. Check Handle or Material Mode."));
        }
        else
        {
             ResultJson->SetStringField(TEXT("error"), TEXT("Missing 'handle'"));
             ResultJson->SetBoolField(TEXT("success"), false);
        }
    }
    // --- Lifecycle ---
    else if (CommandType == TEXT("material_compile_asset"))
    {
        FString Status = Subsystem->CompileAsset();
        bool bCompileSuccess = !Status.StartsWith(TEXT("Error"));
        ResultJson->SetStringField(TEXT("compile_status"), Status);
        ResultJson->SetBoolField(TEXT("success"), bCompileSuccess);
        if (!bCompileSuccess)
        {
            ResultJson->SetStringField(TEXT("error"), Status);
        }
    }
    else
    {
        ResultJson->SetStringField(TEXT("error"), FString::Printf(TEXT("Unknown Material Command: %s"), *CommandType));
        ResultJson->SetBoolField(TEXT("success"), false);
    }

    return ResultJson;
}
