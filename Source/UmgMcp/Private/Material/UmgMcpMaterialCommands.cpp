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
    constexpr const TCHAR* EmissiveColorPinName = TEXT("EmissiveColor");
    constexpr const TCHAR* BaseColorPinName = TEXT("BaseColor");
    constexpr const TCHAR* OpacityPinName = TEXT("Opacity");
    constexpr const TCHAR* OpacityMaskPinName = TEXT("OpacityMask");
    constexpr const TCHAR* HlslRgbDesc = TEXT("HLSL_RGB");
    constexpr const TCHAR* HlslAlphaDesc = TEXT("HLSL_A");

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

    static FString NormalizeMaterialToken(FString Value)
    {
        Value = Value.TrimStartAndEnd();
        Value.ReplaceInline(TEXT(" "), TEXT(""));
        Value.ReplaceInline(TEXT("-"), TEXT("_"));
        Value.ReplaceInline(TEXT("."), TEXT("_"));
        return Value.ToLower();
    }

    static bool ParseMaterialDomain(const FString& Input, EMaterialDomain& OutDomain)
    {
        const FString Key = NormalizeMaterialToken(Input);
        if (Key == TEXT("ui") || Key == TEXT("umg") || Key == TEXT("userinterface") || Key == TEXT("user_interface") || Key == TEXT("md_ui"))
        {
            OutDomain = EMaterialDomain::MD_UI;
            return true;
        }
        if (Key == TEXT("surface") || Key == TEXT("md_surface"))
        {
            OutDomain = EMaterialDomain::MD_Surface;
            return true;
        }
        if (Key == TEXT("postprocess") || Key == TEXT("post_process") || Key == TEXT("md_postprocess"))
        {
            OutDomain = EMaterialDomain::MD_PostProcess;
            return true;
        }
        if (Key == TEXT("decal") || Key == TEXT("deferreddecal") || Key == TEXT("deferred_decal") || Key == TEXT("md_deferreddecal"))
        {
            OutDomain = EMaterialDomain::MD_DeferredDecal;
            return true;
        }
        if (Key == TEXT("lightfunction") || Key == TEXT("light_function") || Key == TEXT("md_lightfunction"))
        {
            OutDomain = EMaterialDomain::MD_LightFunction;
            return true;
        }
        if (Key == TEXT("volume") || Key == TEXT("md_volume"))
        {
            OutDomain = EMaterialDomain::MD_Volume;
            return true;
        }
        return false;
    }

    static bool ParseBlendMode(const FString& Input, EBlendMode& OutBlendMode)
    {
        const FString Key = NormalizeMaterialToken(Input);
        if (Key == TEXT("opaque") || Key == TEXT("blend_opaque"))
        {
            OutBlendMode = BLEND_Opaque;
            return true;
        }
        if (Key == TEXT("masked") || Key == TEXT("blend_masked"))
        {
            OutBlendMode = BLEND_Masked;
            return true;
        }
        if (Key == TEXT("translucent") || Key == TEXT("blend_translucent"))
        {
            OutBlendMode = BLEND_Translucent;
            return true;
        }
        if (Key == TEXT("additive") || Key == TEXT("blend_additive"))
        {
            OutBlendMode = BLEND_Additive;
            return true;
        }
        if (Key == TEXT("modulate") || Key == TEXT("blend_modulate"))
        {
            OutBlendMode = BLEND_Modulate;
            return true;
        }
        if (Key == TEXT("alphacomposite") || Key == TEXT("alpha_composite") || Key == TEXT("premultipliedalpha") || Key == TEXT("blend_alphacomposite"))
        {
            OutBlendMode = BLEND_AlphaComposite;
            return true;
        }
        if (Key == TEXT("alphaholdout") || Key == TEXT("alpha_holdout") || Key == TEXT("blend_alphaholdout"))
        {
            OutBlendMode = BLEND_AlphaHoldout;
            return true;
        }
        return false;
    }

    static bool ParseShadingModel(const FString& Input, EMaterialShadingModel& OutShadingModel)
    {
        const FString Key = NormalizeMaterialToken(Input);
        if (Key == TEXT("unlit") || Key == TEXT("msm_unlit"))
        {
            OutShadingModel = MSM_Unlit;
            return true;
        }
        if (Key == TEXT("defaultlit") || Key == TEXT("default_lit") || Key == TEXT("lit") || Key == TEXT("msm_defaultlit"))
        {
            OutShadingModel = MSM_DefaultLit;
            return true;
        }
        if (Key == TEXT("subsurface") || Key == TEXT("msm_subsurface"))
        {
            OutShadingModel = MSM_Subsurface;
            return true;
        }
        if (Key == TEXT("preintegratedskin") || Key == TEXT("preintegrated_skin") || Key == TEXT("msm_preintegratedskin"))
        {
            OutShadingModel = MSM_PreintegratedSkin;
            return true;
        }
        if (Key == TEXT("clearcoat") || Key == TEXT("clear_coat") || Key == TEXT("msm_clearcoat"))
        {
            OutShadingModel = MSM_ClearCoat;
            return true;
        }
        if (Key == TEXT("subsurfaceprofile") || Key == TEXT("subsurface_profile") || Key == TEXT("msm_subsurfaceprofile"))
        {
            OutShadingModel = MSM_SubsurfaceProfile;
            return true;
        }
        if (Key == TEXT("twosidedfoliage") || Key == TEXT("two_sided_foliage") || Key == TEXT("msm_twosidedfoliage"))
        {
            OutShadingModel = MSM_TwoSidedFoliage;
            return true;
        }
        if (Key == TEXT("hair") || Key == TEXT("msm_hair"))
        {
            OutShadingModel = MSM_Hair;
            return true;
        }
        if (Key == TEXT("cloth") || Key == TEXT("msm_cloth"))
        {
            OutShadingModel = MSM_Cloth;
            return true;
        }
        if (Key == TEXT("eye") || Key == TEXT("msm_eye"))
        {
            OutShadingModel = MSM_Eye;
            return true;
        }
        if (Key == TEXT("singlelayerwater") || Key == TEXT("single_layer_water") || Key == TEXT("msm_singlelayerwater"))
        {
            OutShadingModel = MSM_SingleLayerWater;
            return true;
        }
        if (Key == TEXT("thintranslucent") || Key == TEXT("thin_translucent") || Key == TEXT("msm_thintranslucent"))
        {
            OutShadingModel = MSM_ThinTranslucent;
            return true;
        }
        return false;
    }

    static FString GetMaterialDomainName(EMaterialDomain Domain)
    {
        switch (Domain)
        {
        case EMaterialDomain::MD_Surface:
            return TEXT("surface");
        case EMaterialDomain::MD_DeferredDecal:
            return TEXT("deferred_decal");
        case EMaterialDomain::MD_LightFunction:
            return TEXT("light_function");
        case EMaterialDomain::MD_Volume:
            return TEXT("volume");
        case EMaterialDomain::MD_PostProcess:
            return TEXT("post_process");
        case EMaterialDomain::MD_UI:
            return TEXT("ui");
        default:
            return TEXT("unknown");
        }
    }

    static FString GetBlendModeName(EBlendMode BlendMode)
    {
        switch (BlendMode)
        {
        case BLEND_Opaque:
            return TEXT("opaque");
        case BLEND_Masked:
            return TEXT("masked");
        case BLEND_Translucent:
            return TEXT("translucent");
        case BLEND_Additive:
            return TEXT("additive");
        case BLEND_Modulate:
            return TEXT("modulate");
        case BLEND_AlphaComposite:
            return TEXT("alpha_composite");
        case BLEND_AlphaHoldout:
            return TEXT("alpha_holdout");
        default:
            return TEXT("unknown");
        }
    }

    static FString GetPrimaryShadingModelName(UMaterial* Mat)
    {
        if (!Mat)
        {
            return TEXT("");
        }

        const FMaterialShadingModelField Field = Mat->GetShadingModels();
        if (Field.HasShadingModel(MSM_Unlit))
        {
            return TEXT("unlit");
        }
        if (Field.HasShadingModel(MSM_DefaultLit))
        {
            return TEXT("default_lit");
        }
        if (Field.HasShadingModel(MSM_Subsurface))
        {
            return TEXT("subsurface");
        }
        if (Field.HasShadingModel(MSM_PreintegratedSkin))
        {
            return TEXT("preintegrated_skin");
        }
        if (Field.HasShadingModel(MSM_ClearCoat))
        {
            return TEXT("clear_coat");
        }
        if (Field.HasShadingModel(MSM_SubsurfaceProfile))
        {
            return TEXT("subsurface_profile");
        }
        if (Field.HasShadingModel(MSM_TwoSidedFoliage))
        {
            return TEXT("two_sided_foliage");
        }
        if (Field.HasShadingModel(MSM_Hair))
        {
            return TEXT("hair");
        }
        if (Field.HasShadingModel(MSM_Cloth))
        {
            return TEXT("cloth");
        }
        if (Field.HasShadingModel(MSM_Eye))
        {
            return TEXT("eye");
        }
        if (Field.HasShadingModel(MSM_SingleLayerWater))
        {
            return TEXT("single_layer_water");
        }
        if (Field.HasShadingModel(MSM_ThinTranslucent))
        {
            return TEXT("thin_translucent");
        }
        return TEXT("custom");
    }

    static FString ResolveColorOutputPin(UMaterial* Mat)
    {
        if (!Mat)
        {
            return BaseColorPinName;
        }

        const EMaterialDomain Domain = static_cast<EMaterialDomain>(Mat->MaterialDomain.GetValue());
        if (Domain == EMaterialDomain::MD_UI ||
            Domain == EMaterialDomain::MD_PostProcess ||
            Domain == EMaterialDomain::MD_LightFunction ||
            Mat->GetShadingModels().HasShadingModel(MSM_Unlit))
        {
            return EmissiveColorPinName;
        }
        return BaseColorPinName;
    }

    static FString ResolveAlphaOutputPin(UMaterial* Mat)
    {
        if (!Mat)
        {
            return TEXT("");
        }

        const EMaterialDomain Domain = static_cast<EMaterialDomain>(Mat->MaterialDomain.GetValue());
        const EBlendMode BlendMode = static_cast<EBlendMode>(Mat->BlendMode.GetValue());
        if (Domain == EMaterialDomain::MD_UI)
        {
            return OpacityPinName;
        }
        if (BlendMode == BLEND_Masked)
        {
            return OpacityMaskPinName;
        }
        if (BlendMode != BLEND_Opaque)
        {
            return OpacityPinName;
        }
        return TEXT("");
    }

    static FString ResolveMaterialOutputTarget(const FString& Input)
    {
        const FString Key = NormalizeMaterialToken(Input);
        if (Key == TEXT("output"))
        {
            return TEXT("Output");
        }
        if (Key == TEXT("basecolor") || Key == TEXT("base_color") || Key == TEXT("diffuse"))
        {
            return BaseColorPinName;
        }
        if (Key == TEXT("emissive") || Key == TEXT("emissivecolor") || Key == TEXT("finalcolor") || Key == TEXT("final_color"))
        {
            return EmissiveColorPinName;
        }
        if (Key == TEXT("opacity"))
        {
            return OpacityPinName;
        }
        if (Key == TEXT("opacitymask") || Key == TEXT("opacity_mask"))
        {
            return OpacityMaskPinName;
        }
        if (Key == TEXT("metallic"))
        {
            return TEXT("Metallic");
        }
        if (Key == TEXT("specular"))
        {
            return TEXT("Specular");
        }
        if (Key == TEXT("roughness"))
        {
            return TEXT("Roughness");
        }
        if (Key == TEXT("anisotropy"))
        {
            return TEXT("Anisotropy");
        }
        if (Key == TEXT("normal"))
        {
            return TEXT("Normal");
        }
        if (Key == TEXT("tangent"))
        {
            return TEXT("Tangent");
        }
        if (Key == TEXT("worldpositionoffset") || Key == TEXT("world_position_offset") || Key == TEXT("wpo"))
        {
            return TEXT("WorldPositionOffset");
        }
        if (Key == TEXT("subsurface") || Key == TEXT("subsurfacecolor") || Key == TEXT("subsurface_color"))
        {
            return TEXT("SubsurfaceColor");
        }
        if (Key == TEXT("ambientocclusion") || Key == TEXT("ambient_occlusion") || Key == TEXT("ao"))
        {
            return TEXT("AmbientOcclusion");
        }
        if (Key == TEXT("refraction"))
        {
            return TEXT("Refraction");
        }
        if (Key == TEXT("pixeldepthoffset") || Key == TEXT("pixel_depth_offset") || Key == TEXT("pdo"))
        {
            return TEXT("PixelDepthOffset");
        }
        if (Key == TEXT("customdata0") || Key == TEXT("custom_data_0"))
        {
            return TEXT("CustomData0");
        }
        if (Key == TEXT("customdata1") || Key == TEXT("custom_data_1"))
        {
            return TEXT("CustomData1");
        }
        if (Key == TEXT("customizeduv0") || Key == TEXT("customizeduvs0") || Key == TEXT("customized_uv_0") || Key == TEXT("customized_uvs_0"))
        {
            return TEXT("CustomizedUVs0");
        }
        if (Key == TEXT("customizeduv1") || Key == TEXT("customizeduvs1") || Key == TEXT("customized_uv_1") || Key == TEXT("customized_uvs_1"))
        {
            return TEXT("CustomizedUVs1");
        }
        if (Key == TEXT("customizeduv2") || Key == TEXT("customizeduvs2") || Key == TEXT("customized_uv_2") || Key == TEXT("customized_uvs_2"))
        {
            return TEXT("CustomizedUVs2");
        }
        if (Key == TEXT("customizeduv3") || Key == TEXT("customizeduvs3") || Key == TEXT("customized_uv_3") || Key == TEXT("customized_uvs_3"))
        {
            return TEXT("CustomizedUVs3");
        }
        if (Key == TEXT("customizeduv4") || Key == TEXT("customizeduvs4") || Key == TEXT("customized_uv_4") || Key == TEXT("customized_uvs_4"))
        {
            return TEXT("CustomizedUVs4");
        }
        if (Key == TEXT("customizeduv5") || Key == TEXT("customizeduvs5") || Key == TEXT("customized_uv_5") || Key == TEXT("customized_uvs_5"))
        {
            return TEXT("CustomizedUVs5");
        }
        if (Key == TEXT("customizeduv6") || Key == TEXT("customizeduvs6") || Key == TEXT("customized_uv_6") || Key == TEXT("customized_uvs_6"))
        {
            return TEXT("CustomizedUVs6");
        }
        if (Key == TEXT("customizeduv7") || Key == TEXT("customizeduvs7") || Key == TEXT("customized_uv_7") || Key == TEXT("customized_uvs_7"))
        {
            return TEXT("CustomizedUVs7");
        }
        if (Key == TEXT("surfacethickness") || Key == TEXT("surface_thickness"))
        {
            return TEXT("SurfaceThickness");
        }
        if (Key == TEXT("displacement"))
        {
            return TEXT("Displacement");
        }
        if (Key == TEXT("materialattributes") || Key == TEXT("material_attributes"))
        {
            return TEXT("MaterialAttributes");
        }
        return Input.TrimStartAndEnd().Replace(TEXT(" "), TEXT(""));
    }

    static bool IsFloat3MaterialOutput(const FString& Target)
    {
        return Target.Equals(BaseColorPinName, ESearchCase::IgnoreCase) ||
            Target.Equals(EmissiveColorPinName, ESearchCase::IgnoreCase) ||
            Target.Equals(TEXT("Normal"), ESearchCase::IgnoreCase) ||
            Target.Equals(TEXT("Tangent"), ESearchCase::IgnoreCase) ||
            Target.Equals(TEXT("WorldPositionOffset"), ESearchCase::IgnoreCase) ||
            Target.Equals(TEXT("SubsurfaceColor"), ESearchCase::IgnoreCase);
    }

    static bool IsFloat2MaterialOutput(const FString& Target)
    {
        return Target.StartsWith(TEXT("CustomizedUVs"), ESearchCase::IgnoreCase);
    }

    static ECustomMaterialOutputType DefaultCustomOutputTypeForTarget(const FString& Target)
    {
        if (Target.Equals(TEXT("MaterialAttributes"), ESearchCase::IgnoreCase))
        {
            return CMOT_MaterialAttributes;
        }
        if (IsFloat3MaterialOutput(Target))
        {
            return CMOT_Float3;
        }
        if (IsFloat2MaterialOutput(Target))
        {
            return CMOT_Float2;
        }
        return CMOT_Float1;
    }

    static bool ParseCustomOutputType(const FString& Input, ECustomMaterialOutputType& OutType)
    {
        const FString Key = NormalizeMaterialToken(Input);
        if (Key == TEXT("float") || Key == TEXT("float1") || Key == TEXT("scalar") || Key == TEXT("cmot_float1"))
        {
            OutType = CMOT_Float1;
            return true;
        }
        if (Key == TEXT("float2") || Key == TEXT("vector2") || Key == TEXT("cmot_float2"))
        {
            OutType = CMOT_Float2;
            return true;
        }
        if (Key == TEXT("float3") || Key == TEXT("vector3") || Key == TEXT("rgb") || Key == TEXT("cmot_float3"))
        {
            OutType = CMOT_Float3;
            return true;
        }
        if (Key == TEXT("float4") || Key == TEXT("vector4") || Key == TEXT("rgba") || Key == TEXT("cmot_float4"))
        {
            OutType = CMOT_Float4;
            return true;
        }
        if (Key == TEXT("materialattributes") || Key == TEXT("material_attributes") || Key == TEXT("cmot_materialattributes"))
        {
            OutType = CMOT_MaterialAttributes;
            return true;
        }
        return false;
    }

    static FString GetCustomOutputTypeName(ECustomMaterialOutputType Type)
    {
        switch (Type)
        {
        case CMOT_Float2:
            return TEXT("float2");
        case CMOT_Float3:
            return TEXT("float3");
        case CMOT_Float4:
            return TEXT("float4");
        case CMOT_MaterialAttributes:
            return TEXT("material_attributes");
        case CMOT_Float1:
        default:
            return TEXT("float1");
        }
    }

    static bool IsValidHlslIdentifier(const FString& Name)
    {
        if (Name.IsEmpty())
        {
            return false;
        }

        const TCHAR First = Name[0];
        if (!(FChar::IsAlpha(First) || First == TEXT('_')))
        {
            return false;
        }

        for (int32 i = 1; i < Name.Len(); ++i)
        {
            const TCHAR Ch = Name[i];
            if (!(FChar::IsAlnum(Ch) || Ch == TEXT('_')))
            {
                return false;
            }
        }
        return true;
    }

    static bool HasMaterialTypeOptions(const TSharedPtr<FJsonObject>& Params)
    {
        return Params.IsValid() &&
            (Params->HasField(TEXT("domain")) ||
             Params->HasField(TEXT("blend_mode")) ||
             Params->HasField(TEXT("shading_model")) ||
             Params->HasField(TEXT("two_sided")));
    }

    static bool ApplyMaterialTypeOptions(UMaterial* Mat, const TSharedPtr<FJsonObject>& Params, FString& OutError)
    {
        OutError = TEXT("");
        if (!Mat || !Params.IsValid())
        {
            OutError = TEXT("No target material");
            return false;
        }

        bool bSetDomain = false;
        bool bSetBlendMode = false;
        bool bSetShadingModel = false;
        bool bSetTwoSided = false;
        EMaterialDomain NewDomain = static_cast<EMaterialDomain>(Mat->MaterialDomain.GetValue());
        EBlendMode NewBlendMode = static_cast<EBlendMode>(Mat->BlendMode.GetValue());
        EMaterialShadingModel NewShadingModel = MSM_DefaultLit;
        bool bNewTwoSided = Mat->TwoSided != 0;

        FString DomainStr;
        if (Params->TryGetStringField(TEXT("domain"), DomainStr))
        {
            if (!ParseMaterialDomain(DomainStr, NewDomain))
            {
                OutError = FString::Printf(TEXT("Unsupported material domain: %s"), *DomainStr);
                return false;
            }
            bSetDomain = true;
        }

        FString BlendModeStr;
        if (Params->TryGetStringField(TEXT("blend_mode"), BlendModeStr))
        {
            if (!ParseBlendMode(BlendModeStr, NewBlendMode))
            {
                OutError = FString::Printf(TEXT("Unsupported blend mode: %s"), *BlendModeStr);
                return false;
            }
            bSetBlendMode = true;
        }

        FString ShadingModelStr;
        if (Params->TryGetStringField(TEXT("shading_model"), ShadingModelStr))
        {
            if (!ParseShadingModel(ShadingModelStr, NewShadingModel))
            {
                OutError = FString::Printf(TEXT("Unsupported shading model: %s"), *ShadingModelStr);
                return false;
            }
            bSetShadingModel = true;
        }

        if (Params->HasField(TEXT("two_sided")))
        {
            if (!Params->TryGetBoolField(TEXT("two_sided"), bNewTwoSided))
            {
                OutError = TEXT("'two_sided' must be a boolean");
                return false;
            }
            bSetTwoSided = true;
        }

        if (bSetDomain || bSetBlendMode || bSetShadingModel || bSetTwoSided)
        {
            Mat->Modify();
            Mat->PreEditChange(nullptr);
            if (bSetDomain)
            {
                Mat->MaterialDomain = NewDomain;
            }
            if (bSetBlendMode)
            {
                Mat->BlendMode = NewBlendMode;
            }
            if (bSetShadingModel)
            {
                Mat->SetShadingModel(NewShadingModel);
            }
            if (bSetTwoSided)
            {
                Mat->TwoSided = bNewTwoSided;
            }
            Mat->PostEditChange();
            Mat->MarkPackageDirty();
        }

        return true;
    }

    static bool IsMaterialInputConnected(UMaterial* Mat, const FString& InputName)
    {
        if (InputName.IsEmpty())
        {
            return false;
        }
        FExpressionInput* Input = FindMaterialInputByName(Mat, InputName);
        return Input && Input->Expression != nullptr;
    }

    static FString FindConnectedTargetForCustomOutput(UMaterial* Mat, UMaterialExpressionCustom* CustomNode, int32 OutputIndex)
    {
        if (!Mat || !CustomNode)
        {
            return TEXT("");
        }

        auto FindInObject = [&](UObject* Owner) -> FString
        {
            if (!Owner)
            {
                return TEXT("");
            }

            for (TFieldIterator<FProperty> It(Owner->GetClass()); It; ++It)
            {
                FProperty* Prop = *It;
                FStructProperty* StructProp = CastField<FStructProperty>(Prop);
                if (!StructProp || !StructProp->Struct)
                {
                    continue;
                }

                const FString StructName = StructProp->Struct->GetName();
                if (!StructName.Equals(TEXT("ExpressionInput")) && !StructName.Contains(TEXT("MaterialInput")))
                {
                    continue;
                }

                FExpressionInput* Input = StructProp->ContainerPtrToValuePtr<FExpressionInput>(Owner);
                if (Input && Input->Expression == CustomNode && Input->OutputIndex == OutputIndex)
                {
                    return Prop->GetName();
                }
            }
            return TEXT("");
        };

#if WITH_EDITOR
        if (Mat->GetEditorOnlyData())
        {
            const FString EditorOnlyResult = FindInObject((UObject*)Mat->GetEditorOnlyData());
            if (!EditorOnlyResult.IsEmpty())
            {
                return EditorOnlyResult;
            }
        }
#endif
        return FindInObject(Mat);
    }

    static void BuildHlslOutputSnapshot(UMaterial* Mat, UMaterialExpressionCustom* CustomNode, TArray<TSharedPtr<FJsonValue>>& OutOutputs)
    {
        OutOutputs.Empty();
        if (!CustomNode)
        {
            return;
        }

        for (int32 i = 0; i < CustomNode->AdditionalOutputs.Num(); ++i)
        {
            const FCustomOutput& Output = CustomNode->AdditionalOutputs[i];
            if (Output.OutputName.IsNone())
            {
                continue;
            }

            const int32 OutputIndex = i + 1;
            const FString Name = Output.OutputName.ToString();
            const FString ConnectedTarget = FindConnectedTargetForCustomOutput(Mat, CustomNode, OutputIndex);

            TSharedPtr<FJsonObject> OutputObj = MakeShareable(new FJsonObject);
            OutputObj->SetStringField(TEXT("name"), Name);
            OutputObj->SetStringField(TEXT("target"), ConnectedTarget.IsEmpty() ? ResolveMaterialOutputTarget(Name) : ConnectedTarget);
            OutputObj->SetStringField(TEXT("type"), GetCustomOutputTypeName(static_cast<ECustomMaterialOutputType>(Output.OutputType.GetValue())));
            OutputObj->SetBoolField(TEXT("connected"), !ConnectedTarget.IsEmpty());
            OutOutputs.Add(MakeShareable(new FJsonValueObject(OutputObj)));
        }
    }

    static TSharedPtr<FJsonObject> BuildOutputContract(UMaterial* Mat, UMaterialExpressionCustom* CustomNode = nullptr)
    {
        TSharedPtr<FJsonObject> OutputContract = MakeShareable(new FJsonObject);
        OutputContract->SetStringField(TEXT("return"), TEXT("float4"));
        if (Mat)
        {
            OutputContract->SetStringField(TEXT("domain"), GetMaterialDomainName(static_cast<EMaterialDomain>(Mat->MaterialDomain.GetValue())));
            OutputContract->SetStringField(TEXT("blend_mode"), GetBlendModeName(static_cast<EBlendMode>(Mat->BlendMode.GetValue())));
            OutputContract->SetStringField(TEXT("shading_model"), GetPrimaryShadingModelName(Mat));
        }
        OutputContract->SetStringField(TEXT("rgb_to"), ResolveColorOutputPin(Mat));

        const FString AlphaPin = ResolveAlphaOutputPin(Mat);
        if (!AlphaPin.IsEmpty())
        {
            OutputContract->SetStringField(TEXT("a_to"), AlphaPin);
        }

        TArray<TSharedPtr<FJsonValue>> AdditionalOutputs;
        BuildHlslOutputSnapshot(Mat, CustomNode, AdditionalOutputs);
        if (AdditionalOutputs.Num() > 0)
        {
            OutputContract->SetArrayField(TEXT("outputs"), AdditionalOutputs);
        }
        return OutputContract;
    }

    template <typename TExpression>
    static TExpression* FindExpressionByDesc(UMaterial* Mat, const TCHAR* Desc)
    {
        if (!Mat)
        {
            return nullptr;
        }

        for (UMaterialExpression* Expr : Mat->GetExpressions())
        {
            if (TExpression* Typed = Cast<TExpression>(Expr))
            {
                if (Typed->Desc.Equals(Desc, ESearchCase::IgnoreCase))
                {
                    return Typed;
                }
            }
        }
        return nullptr;
    }

    static bool RefreshHlslOutputWiring(UUmgMcpMaterialSubsystem* Subsystem, UMaterial* Mat, FString& OutError)
    {
        if (!Subsystem || !Mat)
        {
            OutError = TEXT("No material target");
            return false;
        }

        UMaterialExpressionComponentMask* RgbMask = FindExpressionByDesc<UMaterialExpressionComponentMask>(Mat, HlslRgbDesc);
        UMaterialExpressionComponentMask* AlphaMask = FindExpressionByDesc<UMaterialExpressionComponentMask>(Mat, HlslAlphaDesc);
        if (!RgbMask)
        {
            OutError = TEXT("HLSL RGB mask not found; call hlsl_set_target first to create topology");
            return false;
        }

        const FString ColorPin = ResolveColorOutputPin(Mat);
        bool bLinkOk = Subsystem->ConnectPins(RgbMask->GetName(), TEXT(""), MasterHandleName, ColorPin);

        const FString AlphaPin = ResolveAlphaOutputPin(Mat);
        if (!AlphaPin.IsEmpty() && AlphaMask)
        {
            bLinkOk = Subsystem->ConnectPins(AlphaMask->GetName(), TEXT(""), MasterHandleName, AlphaPin) && bLinkOk;
        }

        if (!bLinkOk)
        {
            OutError = FString::Printf(TEXT("Failed to refresh HLSL output wiring (%s%s%s)"),
                *ColorPin,
                AlphaPin.IsEmpty() ? TEXT("") : TEXT(", "),
                *AlphaPin);
            return false;
        }

        OutError = TEXT("");
        return true;
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

        UMaterialExpressionCustom* CustomNode = FindSingleCustomNode(Mat);
        if (!CustomNode)
        {
            OutReason = TEXT("Material must contain exactly one Custom HLSL node");
            return false;
        }

        if (CustomNode->OutputType != CMOT_Float4)
        {
            OutReason = TEXT("Custom HLSL node primary output must be float4");
            return false;
        }

        const FString ColorPin = ResolveColorOutputPin(Mat);
        const FString AlphaPin = ResolveAlphaOutputPin(Mat);
        const bool bHasColor = IsMaterialInputConnected(Mat, ColorPin);
        const bool bHasAlpha = IsMaterialInputConnected(Mat, AlphaPin);

        if (!bHasColor && !bHasAlpha)
        {
            OutReason = FString::Printf(TEXT("HLSL output is not connected to active material outputs (expected %s%s%s)"),
                *ColorPin,
                AlphaPin.IsEmpty() ? TEXT("") : TEXT(" / "),
                *AlphaPin);
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
        CustomNode->OutputType = CMOT_Float4;
        if (CustomNode->Code.IsEmpty())
        {
            CustomNode->Code = DefaultBootstrapHlsl;
        }

        RgbMask->Desc = HlslRgbDesc;
        RgbMask->R = true;
        RgbMask->G = true;
        RgbMask->B = true;
        RgbMask->A = false;

        AlphaMask->Desc = HlslAlphaDesc;
        AlphaMask->R = false;
        AlphaMask->G = false;
        AlphaMask->B = false;
        AlphaMask->A = true;

        const FString CustomHandle = CustomNode->GetName();
        const FString RgbHandle = RgbMask->GetName();
        const FString AlphaHandle = AlphaMask->GetName();

        const FString ColorPin = ResolveColorOutputPin(Mat);
        const FString AlphaPin = ResolveAlphaOutputPin(Mat);
        bool bLinkOk =
            Subsystem->ConnectPins(CustomHandle, TEXT(""), RgbHandle, TEXT("Input")) &&
            Subsystem->ConnectPins(CustomHandle, TEXT(""), AlphaHandle, TEXT("Input")) &&
            Subsystem->ConnectPins(RgbHandle, TEXT(""), MasterHandleName, ColorPin);

        if (!AlphaPin.IsEmpty())
        {
            bLinkOk = Subsystem->ConnectPins(AlphaHandle, TEXT(""), MasterHandleName, AlphaPin) && bLinkOk;
        }

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

    struct FHlslOutputDraft
    {
        FString Name;
        FString Target;
        ECustomMaterialOutputType Type = CMOT_Float1;
        bool bDelete = false;
    };

    static void ClearCustomAdditionalOutputConnections(UMaterial* Mat, UMaterialExpressionCustom* CustomNode)
    {
        if (!Mat || !CustomNode)
        {
            return;
        }

        auto ClearInObject = [&](UObject* Owner)
        {
            if (!Owner)
            {
                return;
            }

            for (TFieldIterator<FProperty> It(Owner->GetClass()); It; ++It)
            {
                FProperty* Prop = *It;
                FStructProperty* StructProp = CastField<FStructProperty>(Prop);
                if (!StructProp || !StructProp->Struct)
                {
                    continue;
                }

                const FString StructName = StructProp->Struct->GetName();
                if (!StructName.Equals(TEXT("ExpressionInput")) && !StructName.Contains(TEXT("MaterialInput")))
                {
                    continue;
                }

                FExpressionInput* Input = StructProp->ContainerPtrToValuePtr<FExpressionInput>(Owner);
                if (Input && Input->Expression == CustomNode && Input->OutputIndex > 0)
                {
                    Input->Expression = nullptr;
                    Input->OutputIndex = 0;
                }
            }
        };

#if WITH_EDITOR
        if (Mat->GetEditorOnlyData())
        {
            ClearInObject((UObject*)Mat->GetEditorOnlyData());
        }
#endif
        ClearInObject(Mat);
    }

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

    static void ParseOutputDrafts(const TArray<TSharedPtr<FJsonValue>>& JsonOutputs, TArray<FHlslOutputDraft>& OutDrafts, FString& OutError)
    {
        OutDrafts.Empty();
        OutError = TEXT("");

        for (const TSharedPtr<FJsonValue>& JsonVal : JsonOutputs)
        {
            FHlslOutputDraft Draft;
            bool bExplicitType = false;

            if (JsonVal->Type == EJson::String)
            {
                Draft.Target = ResolveMaterialOutputTarget(JsonVal->AsString());
                Draft.Name = Draft.Target.Equals(TEXT("Output"), ESearchCase::IgnoreCase) ? ResolveMaterialOutputTarget(JsonVal->AsString()) : Draft.Target;
            }
            else if (JsonVal->Type == EJson::Object)
            {
                const TSharedPtr<FJsonObject> Obj = JsonVal->AsObject();
                if (!Obj.IsValid())
                {
                    continue;
                }

                FString Target;
                Obj->TryGetStringField(TEXT("target"), Target);
                FString Name;
                Obj->TryGetStringField(TEXT("name"), Name);
                if (Target.IsEmpty())
                {
                    Target = Name;
                }

                Draft.Target = ResolveMaterialOutputTarget(Target);
                Draft.Name = Name.IsEmpty() ? Draft.Target : Name.TrimStartAndEnd().Replace(TEXT(" "), TEXT(""));

                FString TypeStr;
                if (Obj->TryGetStringField(TEXT("type"), TypeStr))
                {
                    if (!ParseCustomOutputType(TypeStr, Draft.Type))
                    {
                        OutError = FString::Printf(TEXT("Unsupported HLSL output type: %s"), *TypeStr);
                        return;
                    }
                    bExplicitType = true;
                }

                bool bDeleteFlag = false;
                Obj->TryGetBoolField(TEXT("delete"), bDeleteFlag);
                FString Action;
                Obj->TryGetStringField(TEXT("action"), Action);
                Draft.bDelete = bDeleteFlag || Action.Equals(TEXT("delete"), ESearchCase::IgnoreCase);
            }
            else
            {
                continue;
            }

            if (Draft.Target.IsEmpty())
            {
                OutError = TEXT("HLSL output target cannot be empty");
                return;
            }

            if (Draft.Name.IsEmpty())
            {
                Draft.Name = Draft.Target;
            }

            if (!IsValidHlslIdentifier(Draft.Name))
            {
                OutError = FString::Printf(TEXT("Invalid HLSL output name '%s'. Use a valid HLSL identifier."), *Draft.Name);
                return;
            }

            if (!bExplicitType)
            {
                Draft.Type = DefaultCustomOutputTypeForTarget(Draft.Target);
            }

            OutDrafts.Add(Draft);
        }
    }

    static bool RebuildHlslOutputs(UUmgMcpMaterialSubsystem* Subsystem, UMaterial* Mat, UMaterialExpressionCustom* CustomNode, const TArray<FHlslOutputDraft>& Outputs, FString& OutError)
    {
        if (!Subsystem || !Mat || !CustomNode)
        {
            OutError = TEXT("No HLSL material target");
            return false;
        }

        ClearCustomAdditionalOutputConnections(Mat, CustomNode);

        CustomNode->Modify();
        CustomNode->OutputType = CMOT_Float4;
        CustomNode->AdditionalOutputs.Empty();
        for (const FHlslOutputDraft& Output : Outputs)
        {
            FCustomOutput NewOutput;
            NewOutput.OutputName = *Output.Name;
            NewOutput.OutputType = Output.Type;
            CustomNode->AdditionalOutputs.Add(NewOutput);
        }

#if WITH_EDITOR
        CustomNode->RebuildOutputs();
#endif
        CustomNode->PostEditChange();
        Mat->PostEditChange();
        Mat->MarkPackageDirty();

        for (const FHlslOutputDraft& Output : Outputs)
        {
            if (!Subsystem->ConnectPins(CustomNode->GetName(), Output.Name, MasterHandleName, Output.Target))
            {
                OutError = FString::Printf(TEXT("Failed to connect HLSL output '%s' to material target '%s'"), *Output.Name, *Output.Target);
                return false;
            }
        }

        OutError = TEXT("");
        return true;
    }

    static bool ApplyHlslOutputs(UUmgMcpMaterialSubsystem* Subsystem, UMaterial* Mat, UMaterialExpressionCustom* CustomNode, const TArray<FHlslOutputDraft>& Incoming, FString& OutError)
    {
        if (!Subsystem || !Mat || !CustomNode)
        {
            OutError = TEXT("No HLSL material target");
            return false;
        }

        TArray<FHlslOutputDraft> WorkingOutputs;
        for (int32 i = 0; i < CustomNode->AdditionalOutputs.Num(); ++i)
        {
            const FCustomOutput& Existing = CustomNode->AdditionalOutputs[i];
            if (Existing.OutputName.IsNone())
            {
                continue;
            }

            FHlslOutputDraft Draft;
            Draft.Name = Existing.OutputName.ToString();
            Draft.Type = static_cast<ECustomMaterialOutputType>(Existing.OutputType.GetValue());
            const FString ConnectedTarget = FindConnectedTargetForCustomOutput(Mat, CustomNode, i + 1);
            Draft.Target = ConnectedTarget.IsEmpty() ? ResolveMaterialOutputTarget(Draft.Name) : ConnectedTarget;
            WorkingOutputs.Add(Draft);
        }

        for (const FHlslOutputDraft& Item : Incoming)
        {
            if (Item.bDelete)
            {
                OutError = TEXT("hlsl_set is union-only and cannot delete outputs. Use hlsl_delete_output with confirm_delete=true.");
                return false;
            }

            int32 ExistingIndex = INDEX_NONE;
            for (int32 i = 0; i < WorkingOutputs.Num(); ++i)
            {
                if (WorkingOutputs[i].Name.Equals(Item.Name, ESearchCase::IgnoreCase) ||
                    WorkingOutputs[i].Target.Equals(Item.Target, ESearchCase::IgnoreCase))
                {
                    ExistingIndex = i;
                    break;
                }
            }

            if (ExistingIndex != INDEX_NONE)
            {
                WorkingOutputs[ExistingIndex].Name = Item.Name;
                WorkingOutputs[ExistingIndex].Target = Item.Target;
                WorkingOutputs[ExistingIndex].Type = Item.Type;
            }
            else
            {
                WorkingOutputs.Add(Item);
            }
        }

        return RebuildHlslOutputs(Subsystem, Mat, CustomNode, WorkingOutputs, OutError);
    }

    static bool DeleteHlslOutputs(UUmgMcpMaterialSubsystem* Subsystem, UMaterial* Mat, UMaterialExpressionCustom* CustomNode, const TArray<FString>& NamesOrTargets, TArray<FString>& OutDeleted, FString& OutError)
    {
        OutDeleted.Empty();
        if (!Subsystem || !Mat || !CustomNode)
        {
            OutError = TEXT("No HLSL material target");
            return false;
        }

        TArray<FHlslOutputDraft> KeptOutputs;

        for (int32 i = 0; i < CustomNode->AdditionalOutputs.Num(); ++i)
        {
            const FCustomOutput& Existing = CustomNode->AdditionalOutputs[i];
            const FString Name = Existing.OutputName.ToString();
            const FString ConnectedTarget = FindConnectedTargetForCustomOutput(Mat, CustomNode, i + 1);
            const FString Target = ConnectedTarget.IsEmpty() ? ResolveMaterialOutputTarget(Name) : ConnectedTarget;

            const bool bDeleteThis = NamesOrTargets.ContainsByPredicate([&Name, &Target](const FString& Candidate)
            {
                const FString CanonicalCandidate = ResolveMaterialOutputTarget(Candidate);
                return Candidate.Equals(Name, ESearchCase::IgnoreCase) || CanonicalCandidate.Equals(Target, ESearchCase::IgnoreCase);
            });

            if (bDeleteThis)
            {
                OutDeleted.Add(Name);
            }
            else
            {
                FHlslOutputDraft Kept;
                Kept.Name = Name;
                Kept.Target = Target;
                Kept.Type = static_cast<ECustomMaterialOutputType>(Existing.OutputType.GetValue());
                KeptOutputs.Add(Kept);
            }
        }

        if (OutDeleted.Num() == 0)
        {
            OutError = TEXT("No matching HLSL output found.");
            return false;
        }

        return RebuildHlslOutputs(Subsystem, Mat, CustomNode, KeptOutputs, OutError);
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
    else if (CommandType == TEXT("material_modify_type"))
    {
        FString Path;
        if (Params->TryGetStringField(TEXT("path"), Path) && !Path.IsEmpty())
        {
            const FString Status = Subsystem->SetTargetMaterial(Path, true);
            if (IsErrorStatus(Status))
            {
                ResultJson->SetBoolField(TEXT("success"), false);
                ResultJson->SetStringField(TEXT("error"), Status);
                return ResultJson;
            }
        }

        UMaterial* Mat = Subsystem->GetTargetMaterial();
        if (!Mat)
        {
            ResultJson->SetBoolField(TEXT("success"), false);
            ResultJson->SetStringField(TEXT("error"), TEXT("No target material. Call material_set_target or hlsl_set_target first."));
            return ResultJson;
        }

        FString TypeError;
        if (!ApplyMaterialTypeOptions(Mat, Params, TypeError))
        {
            ResultJson->SetBoolField(TEXT("success"), false);
            ResultJson->SetStringField(TEXT("error"), TypeError);
            return ResultJson;
        }

        bool bRefreshHlslWiring = true;
        Params->TryGetBoolField(TEXT("refresh_hlsl_wiring"), bRefreshHlslWiring);
        FString WiringStatus = TEXT("skipped");
        FString WiringError;
        if (bRefreshHlslWiring && FindSingleCustomNode(Mat))
        {
            WiringStatus = RefreshHlslOutputWiring(Subsystem, Mat, WiringError) ? TEXT("refreshed") : TEXT("warning");
        }

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("target"), Mat->GetPathName());
        ResultJson->SetStringField(TEXT("domain"), GetMaterialDomainName(static_cast<EMaterialDomain>(Mat->MaterialDomain.GetValue())));
        ResultJson->SetStringField(TEXT("blend_mode"), GetBlendModeName(static_cast<EBlendMode>(Mat->BlendMode.GetValue())));
        ResultJson->SetStringField(TEXT("shading_model"), GetPrimaryShadingModelName(Mat));
        ResultJson->SetBoolField(TEXT("two_sided"), Mat->TwoSided != 0);
        ResultJson->SetStringField(TEXT("hlsl_wiring"), WiringStatus);
        if (!WiringError.IsEmpty())
        {
            ResultJson->SetStringField(TEXT("hlsl_wiring_warning"), WiringError);
        }
        ResultJson->SetObjectField(TEXT("output_contract"), BuildOutputContract(Mat, FindSingleCustomNode(Mat)));
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

        auto ApplyTypeOptionsIfRequested = [&]() -> bool
        {
            if (!HasMaterialTypeOptions(Params))
            {
                return true;
            }

            FString TypeError;
            if (!ApplyMaterialTypeOptions(Mat, Params, TypeError))
            {
                ResultJson->SetBoolField(TEXT("success"), false);
                ResultJson->SetStringField(TEXT("error"), TypeError);
                return false;
            }

            if (FindSingleCustomNode(Mat))
            {
                FString RefreshError;
                RefreshHlslOutputWiring(Subsystem, Mat, RefreshError);
            }
            return true;
        };

        if (!IsErrorStatus(LoadStatus))
        {
            Mat = Subsystem->GetTargetMaterial();
            if (!ApplyTypeOptionsIfRequested())
            {
                return ResultJson;
            }
            if (IsValidHlslTarget(Mat, CustomNode, ValidationReason))
            {
                ResultJson->SetBoolField(TEXT("success"), true);
                ResultJson->SetStringField(TEXT("target"), Path);
                ResultJson->SetStringField(TEXT("action"), TEXT("loaded"));
                ResultJson->SetStringField(TEXT("custom_node_handle"), ResolveHandleFromExpression(CustomNode));
                ResultJson->SetStringField(TEXT("message"), TEXT("HLSL target ready"));
                ResultJson->SetObjectField(TEXT("output_contract"), BuildOutputContract(Mat, CustomNode));
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
                if (!ApplyTypeOptionsIfRequested())
                {
                    return ResultJson;
                }
                if (ResetToSingleHlslTopology(Subsystem, Mat, CustomNode, BootstrapError))
                {
                    ResultJson->SetBoolField(TEXT("success"), true);
                    ResultJson->SetStringField(TEXT("target"), Path);
                    ResultJson->SetStringField(TEXT("action"), TEXT("overwritten"));
                    ResultJson->SetStringField(TEXT("custom_node_handle"), ResolveHandleFromExpression(CustomNode));
                    ResultJson->SetStringField(TEXT("message"), TEXT("HLSL topology overwritten successfully"));
                    ResultJson->SetObjectField(TEXT("output_contract"), BuildOutputContract(Mat, CustomNode));
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
                if (!ApplyTypeOptionsIfRequested())
                {
                    return ResultJson;
                }
                FString BootstrapError;
                if (ResetToSingleHlslTopology(Subsystem, Mat, CustomNode, BootstrapError))
                {
                    ResultJson->SetBoolField(TEXT("success"), true);
                    ResultJson->SetStringField(TEXT("target"), Path);
                    ResultJson->SetStringField(TEXT("action"), TEXT("created"));
                    ResultJson->SetStringField(TEXT("custom_node_handle"), ResolveHandleFromExpression(CustomNode));
                    ResultJson->SetStringField(TEXT("message"), TEXT("Created and initialized HLSL target"));
                    ResultJson->SetObjectField(TEXT("output_contract"), BuildOutputContract(Mat, CustomNode));
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

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("target"), Mat ? Mat->GetPathName() : TEXT(""));
        ResultJson->SetStringField(TEXT("custom_node_handle"), ResolveHandleFromExpression(CustomNode));
        ResultJson->SetStringField(TEXT("hlsl"), CustomNode ? CustomNode->Code : TEXT(""));
        ResultJson->SetArrayField(TEXT("parameters"), ParamsArray);
        ResultJson->SetObjectField(TEXT("output_contract"), BuildOutputContract(Mat, CustomNode));
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
        const TArray<TSharedPtr<FJsonValue>>* NewOutputsJson = nullptr;
        const bool bHasOutputs = Params->TryGetArrayField(TEXT("outputs"), NewOutputsJson);

        if (!bHasHlsl && !bHasParameters && !bHasOutputs)
        {
            ResultJson->SetBoolField(TEXT("success"), false);
            ResultJson->SetStringField(TEXT("error"), TEXT("Provide at least one of 'hlsl', 'parameters', or 'outputs'"));
            return ResultJson;
        }

        TArray<FHlslOutputDraft> IncomingOutputs;
        if (bHasOutputs)
        {
            FString OutputParseError;
            ParseOutputDrafts(*NewOutputsJson, IncomingOutputs, OutputParseError);
            if (!OutputParseError.IsEmpty())
            {
                ResultJson->SetBoolField(TEXT("success"), false);
                ResultJson->SetStringField(TEXT("error"), OutputParseError);
                return ResultJson;
            }
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

            for (const FHlslParamDraft& Item : Incoming)
            {
                if (Item.bDelete)
                {
                    ResultJson->SetBoolField(TEXT("success"), false);
                    ResultJson->SetStringField(TEXT("error"), TEXT("hlsl_set is union-only and cannot delete parameters. Use hlsl_delete_parameter with confirm_delete=true."));
                    return ResultJson;
                }
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

        if (bHasOutputs)
        {
            FString OutputApplyError;
            if (!ApplyHlslOutputs(Subsystem, Mat, CustomNode, IncomingOutputs, OutputApplyError))
            {
                ResultJson->SetBoolField(TEXT("success"), false);
                ResultJson->SetStringField(TEXT("error"), OutputApplyError);
                return ResultJson;
            }
        }

        // Re-validate and expose resulting parameter list
        UMaterialExpressionCustom* NewCustomNode = FindSingleCustomNode(Subsystem->GetTargetMaterial());
        TArray<TSharedPtr<FJsonValue>> ParametersOut;
        BuildParameterSnapshot(NewCustomNode, ParametersOut);
        TArray<TSharedPtr<FJsonValue>> OutputsOut;
        BuildHlslOutputSnapshot(Mat, NewCustomNode, OutputsOut);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetBoolField(TEXT("hlsl_updated"), bHasHlsl);
        ResultJson->SetBoolField(TEXT("parameters_updated"), bHasParameters);
        ResultJson->SetBoolField(TEXT("outputs_updated"), bHasOutputs);
        ResultJson->SetStringField(TEXT("custom_node_handle"), ResolveHandleFromExpression(NewCustomNode));
        ResultJson->SetArrayField(TEXT("parameters"), ParametersOut);
        ResultJson->SetArrayField(TEXT("outputs"), OutputsOut);
        ResultJson->SetObjectField(TEXT("output_contract"), BuildOutputContract(Mat, NewCustomNode));
    }
    else if (CommandType == TEXT("hlsl_delete"))
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

        bool bConfirmDelete = false;
        Params->TryGetBoolField(TEXT("confirm_delete"), bConfirmDelete);
        if (!bConfirmDelete)
        {
            ResultJson->SetBoolField(TEXT("success"), false);
            ResultJson->SetBoolField(TEXT("requires_confirmation"), true);
            ResultJson->SetStringField(TEXT("error"), TEXT("Deletion requires confirm_delete=true."));
            return ResultJson;
        }

        FString Kind;
        Params->TryGetStringField(TEXT("kind"), Kind);
        const FString KindKey = NormalizeMaterialToken(Kind);
        const bool bRestrictParameter = KindKey == TEXT("parameter") || KindKey == TEXT("param") || KindKey == TEXT("input");
        const bool bRestrictOutput = KindKey == TEXT("output");
        if (!KindKey.IsEmpty() && !bRestrictParameter && !bRestrictOutput)
        {
            ResultJson->SetBoolField(TEXT("success"), false);
            ResultJson->SetStringField(TEXT("error"), TEXT("'kind' must be 'parameter' or 'output' when provided."));
            return ResultJson;
        }

        TArray<FString> NamesToDelete;
        FString SingleName;
        if (Params->TryGetStringField(TEXT("name"), SingleName) && !SingleName.IsEmpty())
        {
            NamesToDelete.Add(SingleName);
        }

        const TArray<TSharedPtr<FJsonValue>>* NamesArray = nullptr;
        if (Params->TryGetArrayField(TEXT("names"), NamesArray))
        {
            for (const TSharedPtr<FJsonValue>& NameVal : *NamesArray)
            {
                if (NameVal.IsValid())
                {
                    FString Name = NameVal->AsString();
                    if (!Name.IsEmpty())
                    {
                        NamesToDelete.AddUnique(Name);
                    }
                }
            }
        }

        if (NamesToDelete.Num() == 0)
        {
            ResultJson->SetBoolField(TEXT("success"), false);
            ResultJson->SetStringField(TEXT("error"), TEXT("Provide 'name' or 'names' to delete."));
            return ResultJson;
        }

        TArray<FString> ParameterNamesToDelete;
        TArray<FString> OutputNamesToDelete;
        TArray<TSharedPtr<FJsonValue>> AmbiguousCandidates;

        for (const FString& RequestedName : NamesToDelete)
        {
            TArray<FString> ParameterMatches;
            TArray<FString> OutputMatches;

            if (!bRestrictOutput)
            {
                for (const FCustomInput& Input : CustomNode->Inputs)
                {
                    const FString InputName = Input.InputName.ToString();
                    if (InputName.Equals(RequestedName, ESearchCase::IgnoreCase))
                    {
                        ParameterMatches.Add(InputName);
                    }
                }
            }

            if (!bRestrictParameter)
            {
                const FString RequestedTarget = ResolveMaterialOutputTarget(RequestedName);
                for (int32 i = 0; i < CustomNode->AdditionalOutputs.Num(); ++i)
                {
                    const FCustomOutput& Existing = CustomNode->AdditionalOutputs[i];
                    const FString OutputName = Existing.OutputName.ToString();
                    const FString ConnectedTarget = FindConnectedTargetForCustomOutput(Mat, CustomNode, i + 1);
                    const FString OutputTarget = ConnectedTarget.IsEmpty() ? ResolveMaterialOutputTarget(OutputName) : ConnectedTarget;
                    if (OutputName.Equals(RequestedName, ESearchCase::IgnoreCase) ||
                        OutputTarget.Equals(RequestedTarget, ESearchCase::IgnoreCase))
                    {
                        OutputMatches.Add(OutputName);
                    }
                }
            }

            const int32 MatchCount = ParameterMatches.Num() + OutputMatches.Num();
            if (MatchCount == 0)
            {
                ResultJson->SetBoolField(TEXT("success"), false);
                ResultJson->SetStringField(TEXT("error"), FString::Printf(TEXT("No HLSL parameter or output found for '%s'."), *RequestedName));
                return ResultJson;
            }

            if (MatchCount > 1)
            {
                TSharedPtr<FJsonObject> CandidateObj = MakeShareable(new FJsonObject);
                CandidateObj->SetStringField(TEXT("name"), RequestedName);
                TArray<TSharedPtr<FJsonValue>> CandidateKinds;
                for (const FString& ParameterName : ParameterMatches)
                {
                    TSharedPtr<FJsonObject> Item = MakeShareable(new FJsonObject);
                    Item->SetStringField(TEXT("kind"), TEXT("parameter"));
                    Item->SetStringField(TEXT("name"), ParameterName);
                    CandidateKinds.Add(MakeShareable(new FJsonValueObject(Item)));
                }
                for (const FString& OutputName : OutputMatches)
                {
                    TSharedPtr<FJsonObject> Item = MakeShareable(new FJsonObject);
                    Item->SetStringField(TEXT("kind"), TEXT("output"));
                    Item->SetStringField(TEXT("name"), OutputName);
                    CandidateKinds.Add(MakeShareable(new FJsonValueObject(Item)));
                }
                CandidateObj->SetArrayField(TEXT("candidates"), CandidateKinds);
                AmbiguousCandidates.Add(MakeShareable(new FJsonValueObject(CandidateObj)));
                continue;
            }

            if (ParameterMatches.Num() == 1)
            {
                ParameterNamesToDelete.AddUnique(ParameterMatches[0]);
            }
            if (OutputMatches.Num() == 1)
            {
                OutputNamesToDelete.AddUnique(OutputMatches[0]);
            }
        }

        if (AmbiguousCandidates.Num() > 0)
        {
            ResultJson->SetBoolField(TEXT("success"), false);
            ResultJson->SetBoolField(TEXT("requires_kind"), true);
            ResultJson->SetStringField(TEXT("error"), TEXT("Delete target is ambiguous. Provide kind='parameter' or kind='output'."));
            ResultJson->SetArrayField(TEXT("ambiguous"), AmbiguousCandidates);
            return ResultJson;
        }

        TArray<FString> DeletedParameters;
        TArray<FString> ParameterNodeHandlesToDelete;
        TArray<FHlslParamDraft> ParametersToReconnect;
        if (ParameterNamesToDelete.Num() > 0)
        {
            TArray<FString> FinalInputNames;
            for (const FCustomInput& Input : CustomNode->Inputs)
            {
                const FString InputName = Input.InputName.ToString();
                UMaterialExpression* SourceExpr = Input.Input.Expression;
                const bool bDeleteThis = ParameterNamesToDelete.ContainsByPredicate([&InputName](const FString& Candidate)
                {
                    return Candidate.Equals(InputName, ESearchCase::IgnoreCase);
                });

                if (bDeleteThis)
                {
                    DeletedParameters.Add(InputName);
                    if (SourceExpr &&
                        (Cast<UMaterialExpressionScalarParameter>(SourceExpr) ||
                         Cast<UMaterialExpressionVectorParameter>(SourceExpr) ||
                         Cast<UMaterialExpressionTextureSampleParameter>(SourceExpr)))
                    {
                        ParameterNodeHandlesToDelete.AddUnique(ResolveHandleFromExpression(SourceExpr));
                    }
                }
                else
                {
                    FinalInputNames.Add(InputName);
                    FHlslParamDraft KeptParam;
                    KeptParam.Name = InputName;
                    KeptParam.Kind = DetectParamKind(SourceExpr);
                    ParametersToReconnect.Add(KeptParam);
                }
            }

            if (!Subsystem->SetCustomNodeHLSL(CustomNode->GetName(), CustomNode->Code, FinalInputNames))
            {
                ResultJson->SetBoolField(TEXT("success"), false);
                ResultJson->SetStringField(TEXT("error"), TEXT("Failed to remove HLSL parameter inputs."));
                return ResultJson;
            }

            for (const FString& Handle : ParameterNodeHandlesToDelete)
            {
                if (!Handle.IsEmpty())
                {
                    Subsystem->DeleteNode(Handle);
                }
            }

            for (const FHlslParamDraft& Param : ParametersToReconnect)
            {
                const FString ParamKind = Param.Kind.IsEmpty() || Param.Kind.Equals(UnknownParamKind, ESearchCase::IgnoreCase) ? DefaultParamKind : Param.Kind;
                const FString ParamHandle = Subsystem->DefineVariable(Param.Name, ParamKind);
                if (IsErrorStatus(ParamHandle))
                {
                    ResultJson->SetBoolField(TEXT("success"), false);
                    ResultJson->SetStringField(TEXT("error"), FString::Printf(TEXT("Failed to reconnect HLSL parameter '%s': %s"), *Param.Name, *ParamHandle));
                    return ResultJson;
                }
                if (!Subsystem->ConnectPins(ParamHandle, TEXT(""), CustomNode->GetName(), Param.Name))
                {
                    ResultJson->SetBoolField(TEXT("success"), false);
                    ResultJson->SetStringField(TEXT("error"), FString::Printf(TEXT("Failed to reconnect HLSL parameter '%s'."), *Param.Name));
                    return ResultJson;
                }
            }
            CustomNode = FindSingleCustomNode(Subsystem->GetTargetMaterial());
        }

        TArray<FString> DeletedOutputs;
        if (OutputNamesToDelete.Num() > 0)
        {
            FString DeleteOutputError;
            if (!DeleteHlslOutputs(Subsystem, Mat, CustomNode, OutputNamesToDelete, DeletedOutputs, DeleteOutputError))
            {
                ResultJson->SetBoolField(TEXT("success"), false);
                ResultJson->SetStringField(TEXT("error"), DeleteOutputError);
                return ResultJson;
            }
            CustomNode = FindSingleCustomNode(Subsystem->GetTargetMaterial());
        }

        TArray<TSharedPtr<FJsonValue>> DeletedJson;
        for (const FString& DeletedParameter : DeletedParameters)
        {
            TSharedPtr<FJsonObject> Item = MakeShareable(new FJsonObject);
            Item->SetStringField(TEXT("kind"), TEXT("parameter"));
            Item->SetStringField(TEXT("name"), DeletedParameter);
            DeletedJson.Add(MakeShareable(new FJsonValueObject(Item)));
        }
        for (const FString& DeletedOutput : DeletedOutputs)
        {
            TSharedPtr<FJsonObject> Item = MakeShareable(new FJsonObject);
            Item->SetStringField(TEXT("kind"), TEXT("output"));
            Item->SetStringField(TEXT("name"), DeletedOutput);
            DeletedJson.Add(MakeShareable(new FJsonValueObject(Item)));
        }

        TArray<TSharedPtr<FJsonValue>> ParametersOut;
        BuildParameterSnapshot(CustomNode, ParametersOut);
        TArray<TSharedPtr<FJsonValue>> OutputsOut;
        BuildHlslOutputSnapshot(Mat, CustomNode, OutputsOut);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("custom_node_handle"), ResolveHandleFromExpression(CustomNode));
        ResultJson->SetArrayField(TEXT("deleted"), DeletedJson);
        ResultJson->SetArrayField(TEXT("parameters"), ParametersOut);
        ResultJson->SetArrayField(TEXT("outputs"), OutputsOut);
        ResultJson->SetObjectField(TEXT("output_contract"), BuildOutputContract(Mat, CustomNode));
    }
    else if (CommandType == TEXT("hlsl_delete_parameter"))
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

        bool bConfirmDelete = false;
        Params->TryGetBoolField(TEXT("confirm_delete"), bConfirmDelete);

        TArray<FString> NamesToDelete;
        FString SingleName;
        if (Params->TryGetStringField(TEXT("name"), SingleName) && !SingleName.IsEmpty())
        {
            NamesToDelete.Add(SingleName);
        }

        const TArray<TSharedPtr<FJsonValue>>* NamesArray = nullptr;
        if (Params->TryGetArrayField(TEXT("names"), NamesArray))
        {
            for (const TSharedPtr<FJsonValue>& NameVal : *NamesArray)
            {
                if (NameVal.IsValid())
                {
                    FString Name = NameVal->AsString();
                    if (!Name.IsEmpty())
                    {
                        NamesToDelete.AddUnique(Name);
                    }
                }
            }
        }

        if (NamesToDelete.Num() == 0)
        {
            ResultJson->SetBoolField(TEXT("success"), false);
            ResultJson->SetStringField(TEXT("error"), TEXT("Provide 'name' or 'names' to delete."));
            return ResultJson;
        }

        if (!bConfirmDelete)
        {
            ResultJson->SetBoolField(TEXT("success"), false);
            ResultJson->SetBoolField(TEXT("requires_confirmation"), true);
            ResultJson->SetStringField(TEXT("error"), TEXT("Deletion requires confirm_delete=true."));
            return ResultJson;
        }

        TArray<FString> FinalInputNames;
        TArray<FString> DeletedNames;
        TArray<FString> ParameterNodeHandlesToDelete;
        TArray<FHlslParamDraft> ParametersToReconnect;
        for (const FCustomInput& Input : CustomNode->Inputs)
        {
            const FString InputName = Input.InputName.ToString();
            UMaterialExpression* SourceExpr = Input.Input.Expression;
            const bool bDeleteThis = NamesToDelete.ContainsByPredicate([&InputName](const FString& Candidate)
            {
                return Candidate.Equals(InputName, ESearchCase::IgnoreCase);
            });

            if (bDeleteThis)
            {
                DeletedNames.Add(InputName);
                if (SourceExpr &&
                    (Cast<UMaterialExpressionScalarParameter>(SourceExpr) ||
                     Cast<UMaterialExpressionVectorParameter>(SourceExpr) ||
                     Cast<UMaterialExpressionTextureSampleParameter>(SourceExpr)))
                {
                    ParameterNodeHandlesToDelete.AddUnique(ResolveHandleFromExpression(SourceExpr));
                }
            }
            else
            {
                FinalInputNames.Add(InputName);
                FHlslParamDraft KeptParam;
                KeptParam.Name = InputName;
                KeptParam.Kind = DetectParamKind(SourceExpr);
                ParametersToReconnect.Add(KeptParam);
            }
        }

        if (DeletedNames.Num() == 0)
        {
            ResultJson->SetBoolField(TEXT("success"), false);
            ResultJson->SetStringField(TEXT("error"), TEXT("No matching HLSL parameter found."));
            return ResultJson;
        }

        if (!Subsystem->SetCustomNodeHLSL(CustomNode->GetName(), CustomNode->Code, FinalInputNames))
        {
            ResultJson->SetBoolField(TEXT("success"), false);
            ResultJson->SetStringField(TEXT("error"), TEXT("Failed to remove HLSL parameter inputs."));
            return ResultJson;
        }

        for (const FString& Handle : ParameterNodeHandlesToDelete)
        {
            if (!Handle.IsEmpty())
            {
                Subsystem->DeleteNode(Handle);
            }
        }

        for (const FHlslParamDraft& Param : ParametersToReconnect)
        {
            const FString ParamKind = Param.Kind.IsEmpty() || Param.Kind.Equals(UnknownParamKind, ESearchCase::IgnoreCase) ? DefaultParamKind : Param.Kind;
            const FString ParamHandle = Subsystem->DefineVariable(Param.Name, ParamKind);
            if (IsErrorStatus(ParamHandle))
            {
                ResultJson->SetBoolField(TEXT("success"), false);
                ResultJson->SetStringField(TEXT("error"), FString::Printf(TEXT("Failed to reconnect HLSL parameter '%s': %s"), *Param.Name, *ParamHandle));
                return ResultJson;
            }
            if (!Subsystem->ConnectPins(ParamHandle, TEXT(""), CustomNode->GetName(), Param.Name))
            {
                ResultJson->SetBoolField(TEXT("success"), false);
                ResultJson->SetStringField(TEXT("error"), FString::Printf(TEXT("Failed to reconnect HLSL parameter '%s'."), *Param.Name));
                return ResultJson;
            }
        }

        UMaterialExpressionCustom* NewCustomNode = FindSingleCustomNode(Subsystem->GetTargetMaterial());
        TArray<TSharedPtr<FJsonValue>> ParametersOut;
        BuildParameterSnapshot(NewCustomNode, ParametersOut);

        TArray<TSharedPtr<FJsonValue>> DeletedJson;
        for (const FString& DeletedName : DeletedNames)
        {
            DeletedJson.Add(MakeShareable(new FJsonValueString(DeletedName)));
        }

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("custom_node_handle"), ResolveHandleFromExpression(NewCustomNode));
        ResultJson->SetArrayField(TEXT("deleted_parameters"), DeletedJson);
        ResultJson->SetArrayField(TEXT("parameters"), ParametersOut);
        ResultJson->SetObjectField(TEXT("output_contract"), BuildOutputContract(Mat, NewCustomNode));
    }
    else if (CommandType == TEXT("hlsl_delete_output"))
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

        bool bConfirmDelete = false;
        Params->TryGetBoolField(TEXT("confirm_delete"), bConfirmDelete);

        TArray<FString> NamesToDelete;
        FString SingleName;
        if (Params->TryGetStringField(TEXT("name"), SingleName) && !SingleName.IsEmpty())
        {
            NamesToDelete.Add(SingleName);
        }

        const TArray<TSharedPtr<FJsonValue>>* NamesArray = nullptr;
        if (Params->TryGetArrayField(TEXT("names"), NamesArray))
        {
            for (const TSharedPtr<FJsonValue>& NameVal : *NamesArray)
            {
                if (NameVal.IsValid())
                {
                    FString Name = NameVal->AsString();
                    if (!Name.IsEmpty())
                    {
                        NamesToDelete.AddUnique(Name);
                    }
                }
            }
        }

        if (NamesToDelete.Num() == 0)
        {
            ResultJson->SetBoolField(TEXT("success"), false);
            ResultJson->SetStringField(TEXT("error"), TEXT("Provide 'name' or 'names' to delete."));
            return ResultJson;
        }

        if (!bConfirmDelete)
        {
            ResultJson->SetBoolField(TEXT("success"), false);
            ResultJson->SetBoolField(TEXT("requires_confirmation"), true);
            ResultJson->SetStringField(TEXT("error"), TEXT("Deletion requires confirm_delete=true."));
            return ResultJson;
        }

        TArray<FString> DeletedNames;
        FString DeleteError;
        if (!DeleteHlslOutputs(Subsystem, Mat, CustomNode, NamesToDelete, DeletedNames, DeleteError))
        {
            ResultJson->SetBoolField(TEXT("success"), false);
            ResultJson->SetStringField(TEXT("error"), DeleteError);
            return ResultJson;
        }

        UMaterialExpressionCustom* NewCustomNode = FindSingleCustomNode(Subsystem->GetTargetMaterial());
        TArray<TSharedPtr<FJsonValue>> DeletedJson;
        for (const FString& DeletedName : DeletedNames)
        {
            DeletedJson.Add(MakeShareable(new FJsonValueString(DeletedName)));
        }

        TArray<TSharedPtr<FJsonValue>> OutputsOut;
        BuildHlslOutputSnapshot(Mat, NewCustomNode, OutputsOut);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("custom_node_handle"), ResolveHandleFromExpression(NewCustomNode));
        ResultJson->SetArrayField(TEXT("deleted_outputs"), DeletedJson);
        ResultJson->SetArrayField(TEXT("outputs"), OutputsOut);
        ResultJson->SetObjectField(TEXT("output_contract"), BuildOutputContract(Mat, NewCustomNode));
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
        FString Handle;
        if (Params->TryGetStringField(TEXT("handle"), Handle))
        {
            bool bSuccess = Subsystem->DeleteNode(Handle);
            ResultJson->SetBoolField(TEXT("success"), bSuccess);
            if (bSuccess)
            {
                ResultJson->SetStringField(TEXT("deleted_handle"), Handle);
            }
        }
        else
        {
             ResultJson->SetStringField(TEXT("error"), TEXT("Missing 'handle'"));
             ResultJson->SetBoolField(TEXT("success"), false);
        }
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
