// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#include "FileManage/UmgMcpAttentionCommands.h"
#include "Editor.h"
#include "FileManage/UmgAttentionSubsystem.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "WidgetBlueprint.h" // Added based on UmgAttentionSubsystem.cpp
#include "AssetRegistry/AssetRegistryModule.h"

TSharedPtr<FJsonObject> FUmgMcpAttentionCommands::HandleCommand(const FString& Command, const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Response = MakeShareable(new FJsonObject);

    UUmgAttentionSubsystem* AttentionSubsystem = GEditor ? GEditor->GetEditorSubsystem<UUmgAttentionSubsystem>() : nullptr;
    if (!AttentionSubsystem)
    {
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), TEXT("UmgAttentionSubsystem not available."));
        return Response;
    }

	if (Command == TEXT("get_target_umg_asset"))
	{
        FString AssetPath = AttentionSubsystem->GetTargetUmgAsset();
        Response->SetBoolField(TEXT("success"), true);
        Response->SetStringField(TEXT("asset_path"), AssetPath);
	}
	else if (Command == TEXT("get_last_edited_umg_asset"))
	{
        FString AssetPath = AttentionSubsystem->GetLastEditedUMGAsset();
        Response->SetBoolField(TEXT("success"), true);
        Response->SetStringField(TEXT("asset_path"), AssetPath);
	}
	else if (Command == TEXT("get_recently_edited_umg_assets"))
	{
        int32 MaxCount = 5;
        if (Params && Params->HasField(TEXT("max_count")))
        {
            MaxCount = Params->GetIntegerField(TEXT("max_count"));
        }
        TArray<FString> RecentAssets = AttentionSubsystem->GetRecentlyEditedUMGAssets(MaxCount);
        
        TArray<TSharedPtr<FJsonValue>> JsonAssets;
        for (const FString& Asset : RecentAssets)
        {
            JsonAssets.Add(MakeShareable(new FJsonValueString(Asset)));
        }
        Response->SetBoolField(TEXT("success"), true);
        Response->SetArrayField(TEXT("assets"), JsonAssets);
	}
    else if (Command == TEXT("set_target_umg_asset"))
    {
        if (Params && Params->HasField(TEXT("asset_path")))
        {
            FString AssetPath = Params->GetStringField(TEXT("asset_path"));
            // Use AssetRegistry to check existence without loading the asset
            IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
            bool bAlreadyExists = AssetRegistry.GetAssetByObjectPath(FSoftObjectPath(AssetPath)).IsValid();
            bool bSuccess = AttentionSubsystem->SetTargetUmgAsset(AssetPath);
            if (bSuccess)
            {
                Response->SetBoolField(TEXT("success"), true);
                Response->SetStringField(TEXT("asset_path"), AssetPath);
                Response->SetStringField(TEXT("action"), bAlreadyExists ? TEXT("loaded") : TEXT("created"));
            }
            else
            {
                Response->SetBoolField(TEXT("success"), false);
                Response->SetStringField(TEXT("error"), FString::Printf(TEXT("Failed to set target. Asset '%s' not found or invalid."), *AssetPath));
            }
        }
        else
        {
            Response->SetBoolField(TEXT("success"), false);
            Response->SetStringField(TEXT("error"), TEXT("Missing 'asset_path' parameter for set_target_umg_asset."));
        }
    }
	else
	{
        Response->SetBoolField(TEXT("success"), false);
		Response->SetStringField(TEXT("error"), TEXT("Unknown attention command"));
	}

	return Response;
}