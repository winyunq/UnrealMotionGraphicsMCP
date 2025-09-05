// UmgMcpFileTransformationCommands.cpp
#include "Commands/UmgMcpFileTransformationCommands.h"
#include "UmgFileTransformation.h" // Our utility class
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
            FString JsonOutput = UUmgFileTransformation::ExportUmgAssetToJsonString(AssetPath);
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
    else if (CommandType == TEXT("apply_json_to_umg"))
    {
        FString AssetPath;
        FString JsonData;
        if (Params->TryGetStringField(TEXT("asset_path"), AssetPath) && Params->TryGetStringField(TEXT("json_data"), JsonData))
        {
            bool bSuccess = UUmgFileTransformation::ApplyJsonStringToUmgAsset(AssetPath, JsonData);
            if (bSuccess)
            {
                ResultJson->SetStringField(TEXT("message"), TEXT("JSON data applied to UMG asset successfully."));
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
            ResultJson->SetStringField(TEXT("error"), TEXT("Missing 'asset_path' or 'json_data' parameter for apply_json_to_umg."));
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