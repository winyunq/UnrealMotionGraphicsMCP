// Copyright (c) 2025-2026 Winyunq. All rights reserved.
// UmgMcpFileTransformationCommands.cpp
#include "FileManage/UmgMcpFileTransformationCommands.h"
#include "FileManage/UmgFileTransformation.h" // Our utility class
#include "Serialization/JsonSerializer.h" // For FJsonSerializer
#include "Serialization/JsonWriter.h" // For FJsonWriter

TSharedPtr<FJsonObject> FUmgMcpFileTransformationCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> ResultJson = MakeShareable(new FJsonObject);

    if (CommandType == TEXT("export_umg_to_json"))
    {
        FString AssetPath;
        if (Params->TryGetStringField(TEXT("asset_path"), AssetPath))
        {
            FString TargetWidgetName;
            Params->TryGetStringField(TEXT("widget_name"), TargetWidgetName); // Optional

            FString JsonOutput = UUmgFileTransformation::ExportUmgAssetToJsonString(AssetPath, TargetWidgetName);
            if (!JsonOutput.IsEmpty())
            {
                ResultJson->SetStringField(TEXT("output"), JsonOutput);
                ResultJson->SetBoolField(TEXT("success"), true);
            }
            else
            {
                ResultJson->SetStringField(TEXT("error"), TEXT("Failed to export UMG asset to JSON."));
                ResultJson->SetBoolField(TEXT("success"), false);
            }
        }
        else
        {
            ResultJson->SetStringField(TEXT("error"), TEXT("Missing 'asset_path' parameter for export_umg_to_json."));
            ResultJson->SetBoolField(TEXT("success"), false);
        }
    }
    else if (CommandType == TEXT("apply_json_to_umg") || CommandType == TEXT("apply_layout"))
    {
        FString AssetPath;
        Params->TryGetStringField(TEXT("asset_path"), AssetPath); // Optional, can fall back to target asset

        FString JsonData;
        if (Params->TryGetStringField(TEXT("json_data"), JsonData) ||
            Params->TryGetStringField(TEXT("layout_json"), JsonData) ||
            Params->TryGetStringField(TEXT("layout_content"), JsonData))
        {
            FString TargetWidgetName;
            if (!Params->TryGetStringField(TEXT("widget_name"), TargetWidgetName))
            {
                Params->TryGetStringField(TEXT("target_widget_name"), TargetWidgetName);
            }

            bool bSuccess = UUmgFileTransformation::ApplyJsonStringToUmgAsset(AssetPath, JsonData, TargetWidgetName);
            if (bSuccess)
            {
                ResultJson->SetStringField(TEXT("asset_path"), AssetPath);
                if (!TargetWidgetName.IsEmpty())
                {
                    ResultJson->SetStringField(TEXT("target_widget"), TargetWidgetName);
                }
                ResultJson->SetBoolField(TEXT("success"), true);
            }
            else
            {
                ResultJson->SetStringField(TEXT("error"), TEXT("Failed to apply JSON data to UMG asset."));
                ResultJson->SetBoolField(TEXT("success"), false);
            }
        }
        else
        {
            ResultJson->SetStringField(TEXT("error"), TEXT("Missing 'json_data', 'layout_json', or 'layout_content' parameter for layout application."));
            ResultJson->SetBoolField(TEXT("success"), false);
        }
    }
    else
    {
        ResultJson->SetStringField(TEXT("error"), FString::Printf(TEXT("Unknown file transformation command: %s"), *CommandType));
        ResultJson->SetBoolField(TEXT("success"), false);
    }

    return ResultJson;
}