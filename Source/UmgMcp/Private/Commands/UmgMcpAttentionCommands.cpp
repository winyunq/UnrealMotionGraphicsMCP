#include "Commands/UmgMcpAttentionCommands.h"
#include "Editor.h"
#include "UmgAttentionSubsystem.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "WidgetBlueprint.h" // Added based on UmgAttentionSubsystem.cpp

TSharedPtr<FJsonObject> FUmgMcpAttentionCommands::HandleCommand(const FString& Command, const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Response = MakeShareable(new FJsonObject);
    Response->SetStringField(TEXT("status"), TEXT("error")); // Default to error

    // Get UmgAttentionSubsystem
    UUmgAttentionSubsystem* AttentionSubsystem = nullptr;
    if (GEditor)
    {
        AttentionSubsystem = GEditor->GetEditorSubsystem<UUmgAttentionSubsystem>();
    }

    if (!AttentionSubsystem)
    {
        Response->SetStringField(TEXT("message"), TEXT("UmgAttentionSubsystem not available."));
        return Response;
    }

	if (Command == TEXT("get_last_edited_umg_asset"))
	{
        FString AssetPath = AttentionSubsystem->GetLastEditedUMGAsset();
        Response->SetStringField(TEXT("status"), TEXT("success"));
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
        Response->SetStringField(TEXT("status"), TEXT("success"));
        Response->SetArrayField(TEXT("assets"), JsonAssets);
	}
	else
	{
		Response->SetStringField(TEXT("message"), TEXT("Unknown attention command"));
	}

	return Response;
}