#include "Commands/UmgMcpWidgetCommands.h"
#include "UmgGetSubsystem.h"
#include "UmgSetSubsystem.h"
#include "UmgAttentionSubsystem.h"
#include "Editor.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
#include "WidgetBlueprint.h"
#include "Misc/PackageName.h"

// Forward declaration of the new helper function
static UWidgetBlueprint* GetTargetWidgetBlueprint(const TSharedPtr<FJsonObject>& Params, FString& OutError);

TSharedPtr<FJsonObject> FUmgMcpWidgetCommands::HandleCommand(const FString& Command, const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Response = MakeShareable(new FJsonObject);
    Response->SetStringField(TEXT("status"), TEXT("error")); // Default to error

    if (!GEditor)
    {
        Response->SetStringField(TEXT("error"), TEXT("GEditor not available."));
        return Response;
    }

    FString ErrorMessage;
    UWidgetBlueprint* TargetBlueprint = GetTargetWidgetBlueprint(Params, ErrorMessage);

    if (!TargetBlueprint)
    {
        Response->SetStringField(TEXT("error"), ErrorMessage);
        return Response;
    }

    // --- GET/QUERY COMMANDS ---
    if (Command == TEXT("get_widget_tree"))
    {
        UUmgGetSubsystem* GetSubsystem = GEditor->GetEditorSubsystem<UUmgGetSubsystem>();
        FString WidgetTreeJsonString = GetSubsystem->GetWidgetTree(TargetBlueprint);
        if (!WidgetTreeJsonString.IsEmpty())
        {
            TSharedPtr<FJsonObject> WidgetTreeJson = MakeShareable(new FJsonObject());
            TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(WidgetTreeJsonString);
            if (FJsonSerializer::Deserialize(Reader, WidgetTreeJson) && WidgetTreeJson.IsValid())
            {
                Response->SetStringField(TEXT("status"), TEXT("success"));
                Response->SetObjectField(TEXT("data"), WidgetTreeJson);
            }
            else
            {
                Response->SetStringField(TEXT("error"), TEXT("Failed to parse widget tree JSON from subsystem."));
            }
        }
        else
        {
            Response->SetStringField(TEXT("error"), TEXT("GetWidgetTree from subsystem returned empty or invalid data. Check logs for details."));
        }
    }
    else if (Command == TEXT("query_widget_properties"))
    {
        UUmgGetSubsystem* GetSubsystem = GEditor->GetEditorSubsystem<UUmgGetSubsystem>();
        FString WidgetName;
        const TArray<TSharedPtr<FJsonValue>>* PropertiesJsonArray;
        if (Params->TryGetStringField(TEXT("widget_name"), WidgetName) && Params->TryGetArrayField(TEXT("properties"), PropertiesJsonArray))
        {
            TArray<FString> PropertiesToQuery;
            for (const TSharedPtr<FJsonValue>& JsonValue : *PropertiesJsonArray)
            {
                PropertiesToQuery.Add(JsonValue->AsString());
            }
            FString QueriedPropertiesString = GetSubsystem->QueryWidgetProperties(TargetBlueprint, WidgetName, PropertiesToQuery);
            TSharedPtr<FJsonObject> QueriedProperties;
            TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(QueriedPropertiesString);
            if (FJsonSerializer::Deserialize(Reader, QueriedProperties) && QueriedProperties.IsValid())
            {
                Response->SetStringField(TEXT("status"), TEXT("success"));
                Response->SetObjectField(TEXT("data"), QueriedProperties);
            }
            else
            {
                Response->SetStringField(TEXT("error"), TEXT("Failed to query widget properties or parse response."));
            }
        }
        else
        {
            Response->SetStringField(TEXT("error"), TEXT("Missing 'widget_name' or 'properties' parameter."));
        }
    }
    // ... other GET commands
    
    // --- SET/ACTION COMMANDS ---
    else if (Command == TEXT("create_widget"))
    {
        UUmgSetSubsystem* SetSubsystem = GEditor->GetEditorSubsystem<UUmgSetSubsystem>();
        FString ParentName, WidgetType, NewWidgetName;
        if (Params->TryGetStringField(TEXT("parent_name"), ParentName) &&
            Params->TryGetStringField(TEXT("widget_type"), WidgetType) &&
            Params->TryGetStringField(TEXT("new_widget_name"), NewWidgetName))
        {
            FString NewWidgetId = SetSubsystem->CreateWidget(TargetBlueprint, ParentName, WidgetType, NewWidgetName);
            if (!NewWidgetId.IsEmpty())
            {
                Response->SetStringField(TEXT("status"), TEXT("success"));
                Response->SetStringField(TEXT("new_widget_id"), NewWidgetId);
            }
            else
            {
                Response->SetStringField(TEXT("error"), TEXT("Failed to create widget. Check logs for details."));
            }
        }
        else
        {
            Response->SetStringField(TEXT("error"), TEXT("Missing parameters for create_widget (parent_name, widget_type, new_widget_name)."));
        }
    }
    else if (Command == TEXT("set_widget_properties"))
    {
        UUmgSetSubsystem* SetSubsystem = GEditor->GetEditorSubsystem<UUmgSetSubsystem>();
        FString WidgetName;
        const TSharedPtr<FJsonObject>* PropertiesJsonObject;
        if (Params->TryGetStringField(TEXT("widget_name"), WidgetName) && Params->TryGetObjectField(TEXT("properties"), PropertiesJsonObject))
        {
            FString PropertiesJsonString;
            TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&PropertiesJsonString);
            FJsonSerializer::Serialize(PropertiesJsonObject->ToSharedRef(), Writer.Get(), false);

            if (SetSubsystem->SetWidgetProperties(TargetBlueprint, WidgetName, PropertiesJsonString))
            {
                Response->SetStringField(TEXT("status"), TEXT("success"));
            }
            else
            {
                Response->SetStringField(TEXT("error"), TEXT("Failed to set widget properties. Check logs for details."));
            }
        }
        else
        {
            Response->SetStringField(TEXT("error"), TEXT("Missing 'widget_name' or 'properties' (as a JSON object) parameter."));
        }
    }
    // ... other SET commands

    else
    {
        Response->SetStringField(TEXT("error"), FString::Printf(TEXT("Unknown or not implemented widget command: %s"), *Command));
    }

    return Response;
}


/**
 * @brief Gets the target UWidgetBlueprint from either the 'asset_path' parameter or the UmgAttentionSubsystem.
 * This is the single source of truth for identifying which UMG asset to operate on.
 * @param Params The JSON parameters from the MCP request.
 * @param OutError A string to receive error messages.
 * @return A valid UWidgetBlueprint pointer, or nullptr if not found.
 */
static UWidgetBlueprint* GetTargetWidgetBlueprint(const TSharedPtr<FJsonObject>& Params, FString& OutError)
{
    FString AssetPath;
    // Priority 1: Check for 'asset_path' in the command parameters.
    if (Params && Params->TryGetStringField(TEXT("asset_path"), AssetPath) && !AssetPath.IsEmpty())
    {
        // Clean the asset path, removing the .uasset extension if present.
        if (AssetPath.EndsWith(FPackageName::GetAssetPackageExtension()))
        {
            AssetPath.LeftChopInline(FPackageName::GetAssetPackageExtension().Len());
        }

        UWidgetBlueprint* LoadedBlueprint = LoadObject<UWidgetBlueprint>(nullptr, *AssetPath);
        if (LoadedBlueprint)
        {
            return LoadedBlueprint;
        }
        else
        {
            OutError = FString::Printf(TEXT("Failed to load UMG asset from specified path: %s"), *AssetPath);
            return nullptr;
        }
    }

    // Priority 2: Fallback to the cached target in UmgAttentionSubsystem.
    if (GEditor)
    {
        UUmgAttentionSubsystem* AttentionSubsystem = GEditor->GetEditorSubsystem<UUmgAttentionSubsystem>();
        if (AttentionSubsystem)
        {
            UWidgetBlueprint* CachedBlueprint = AttentionSubsystem->GetCachedTargetWidgetBlueprint();
            if (CachedBlueprint)
            {
                return CachedBlueprint;
            }
        }
    }

    // If both attempts fail, return an error.
    OutError = TEXT("No UMG asset target specified. Please provide an 'asset_path' parameter or set a target using the attention subsystem.");
    return nullptr;
}
