#include "FileManage/UmgMcpAttentionCommands.h"
#include "Editor.h"
#include "FileManage/UmgAttentionSubsystem.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "WidgetBlueprint.h" // Added based on UmgAttentionSubsystem.cpp

TSharedPtr<FJsonObject> FUmgMcpAttentionCommands::HandleCommand(const FString& Command, const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Response = MakeShareable(new FJsonObject);
    Response->SetStringField(TEXT("status"), TEXT("error")); // Default to error

    UUmgAttentionSubsystem* AttentionSubsystem = GEditor ? GEditor->GetEditorSubsystem<UUmgAttentionSubsystem>() : nullptr;
    if (!AttentionSubsystem)
    {
        Response->SetStringField(TEXT("message"), TEXT("UmgAttentionSubsystem not available."));
        return Response;
    }

	// Create a data object for successful responses
	TSharedPtr<FJsonObject> Data = MakeShareable(new FJsonObject);

	if (Command == TEXT("get_target_umg_asset"))
	{
		// Per user's direction, this is now a simple, stable query.
		// The subsystem itself handles the logic of returning a locked target OR falling back to the last-opened asset.
        FString AssetPath = AttentionSubsystem->GetTargetUmgAsset();
        Data->SetStringField(TEXT("asset_path"), AssetPath);
        Response->SetStringField(TEXT("status"), TEXT("success"));
		Response->SetObjectField(TEXT("data"), Data);
	}
	else if (Command == TEXT("get_last_edited_umg_asset"))
	{
        FString AssetPath = AttentionSubsystem->GetLastEditedUMGAsset();
        Data->SetStringField(TEXT("asset_path"), AssetPath);
        Response->SetStringField(TEXT("status"), TEXT("success"));
		Response->SetObjectField(TEXT("data"), Data);
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
        Data->SetArrayField(TEXT("assets"), JsonAssets);
        Response->SetStringField(TEXT("status"), TEXT("success"));
		Response->SetObjectField(TEXT("data"), Data);
	}
    else if (Command == TEXT("set_target_umg_asset"))
    {
        if (Params && Params->HasField(TEXT("asset_path")))
        {
            FString AssetPath = Params->GetStringField(TEXT("asset_path"));
            AttentionSubsystem->SetTargetUmgAsset(AssetPath);
            Response->SetStringField(TEXT("status"), TEXT("success"));
        }
        else
        {
            Response->SetStringField(TEXT("message"), TEXT("Missing 'asset_path' parameter for set_target_umg_asset."));
        }
    }
	else
	{
		Response->SetStringField(TEXT("message"), TEXT("Unknown attention command"));
	}

	return Response;
}