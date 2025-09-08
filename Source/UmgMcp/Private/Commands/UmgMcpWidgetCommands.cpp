#include "Commands/UmgMcpWidgetCommands.h"
#include "UmgGetSubsystem.h"
#include "UmgSetSubsystem.h"
#include "UmgAttentionSubsystem.h"
#include "Editor.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"

TSharedPtr<FJsonObject> FUmgMcpWidgetCommands::HandleCommand(const FString& Command, const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Response = MakeShareable(new FJsonObject);
    Response->SetStringField(TEXT("status"), TEXT("error")); // Default to error

    if (!GEditor)
    {
        Response->SetStringField(TEXT("message"), TEXT("GEditor not available."));
        return Response;
    }

    // Helper lambda to set attention and get subsystems
    auto SetAttentionAndGetSubsystems = [&](const FString& PathParameterName, FString& OutWidgetName) -> TPair<UUmgAttentionSubsystem*, UObject*> 
    {
        UUmgAttentionSubsystem* AttentionSubsystem = GEditor->GetEditorSubsystem<UUmgAttentionSubsystem>();
        
        if (!AttentionSubsystem)
        {
            Response->SetStringField(TEXT("message"), TEXT("Could not find Attention Subsystem."));
            return {nullptr, nullptr};
        }

        FString PathValue;
        if (!Params || !Params->TryGetStringField(PathParameterName, PathValue) || PathValue.IsEmpty())
        {
            Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Missing or empty '%s' parameter."), *PathParameterName));
            return {nullptr, nullptr};
        }

        FString AssetPath = PathValue;
        if (PathParameterName == TEXT("widget_id"))
        {
            if (!PathValue.Split(TEXT(":"), &AssetPath, &OutWidgetName, ESearchCase::IgnoreCase, ESearchDir::FromEnd))
            {
                Response->SetStringField(TEXT("message"), TEXT("Invalid WidgetId format. Expected AssetPath:WidgetName."));
                return {nullptr, nullptr};
            }
        }

        AttentionSubsystem->SetTargetUmgAsset(AssetPath);
        
        // Determine which subsystem to return based on the command
        if (Command.StartsWith(TEXT("get_")) || Command.StartsWith(TEXT("check_")) || Command.StartsWith(TEXT("query_")))
        {
            return {AttentionSubsystem, GEditor->GetEditorSubsystem<UUmgGetSubsystem>()};
        }
        else
        {
            return {AttentionSubsystem, GEditor->GetEditorSubsystem<UUmgSetSubsystem>()};
        }
    };

    if (Command == TEXT("get_widget_tree"))
    {
        FString DummyWidgetName;
        auto Subsystems = SetAttentionAndGetSubsystems(TEXT("asset_path"), DummyWidgetName);
        if (Subsystems.Key && Subsystems.Value)
        {
            UUmgGetSubsystem* GetSubsystem = Cast<UUmgGetSubsystem>(Subsystems.Value);
            FString WidgetTreeJsonString = GetSubsystem->GetWidgetTree();
            if (!WidgetTreeJsonString.IsEmpty())
            {
                TSharedPtr<FJsonObject> WidgetTreeJson = MakeShareable(new FJsonObject());
                TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(WidgetTreeJsonString);
                if (FJsonSerializer::Deserialize(Reader, WidgetTreeJson) && WidgetTreeJson.IsValid())
                {
                    Response->SetStringField(TEXT("status"), TEXT("success"));
                    Response->SetObjectField(TEXT("widget_tree"), WidgetTreeJson);
                }
                else
                {
                    Response->SetStringField(TEXT("message"), TEXT("Failed to parse widget tree JSON from subsystem."));
                }
            }
            else
            {
                Response->SetStringField(TEXT("message"), TEXT("GetWidgetTree from subsystem returned empty string. Check logs for details."));
            }
        }
    }
    else if (Command == TEXT("query_widget_properties"))
    {
        FString WidgetName;
        auto Subsystems = SetAttentionAndGetSubsystems(TEXT("widget_id"), WidgetName);
        if (Subsystems.Key && Subsystems.Value)
        {
            UUmgGetSubsystem* GetSubsystem = Cast<UUmgGetSubsystem>(Subsystems.Value);
            const TArray<TSharedPtr<FJsonValue>>* PropertiesJsonArray;
            if (Params->TryGetArrayField(TEXT("properties"), PropertiesJsonArray))
            {
                TArray<FString> PropertiesToQuery;
                for (const TSharedPtr<FJsonValue>& JsonValue : *PropertiesJsonArray)
                {
                    PropertiesToQuery.Add(JsonValue->AsString());
                }

                FString QueriedPropertiesString = GetSubsystem->QueryWidgetProperties(WidgetName, PropertiesToQuery);
                TSharedPtr<FJsonObject> QueriedProperties;
                TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(QueriedPropertiesString);
                if (FJsonSerializer::Deserialize(Reader, QueriedProperties) && QueriedProperties.IsValid())
                {
                    Response->SetStringField(TEXT("status"), TEXT("success"));
                    Response->SetObjectField(TEXT("properties"), QueriedProperties.ToSharedRef());
                }
                else
                {
                    Response->SetStringField(TEXT("message"), TEXT("Failed to query widget properties."));
                }
            }
            else
            {
                Response->SetStringField(TEXT("message"), TEXT("Missing 'properties' parameter."));
            }
        }
    }
    else if (Command == TEXT("get_layout_data"))
    {
        FString DummyWidgetName;
        auto Subsystems = SetAttentionAndGetSubsystems(TEXT("asset_path"), DummyWidgetName);
        if (Subsystems.Key && Subsystems.Value)
        {
            UUmgGetSubsystem* GetSubsystem = Cast<UUmgGetSubsystem>(Subsystems.Value);
            int32 ResolutionWidth = 1920;
            int32 ResolutionHeight = 1080;
            if (Params->HasField(TEXT("resolution")))
            {
                const TSharedPtr<FJsonObject>* ResolutionObject;
                if (Params->TryGetObjectField(TEXT("resolution"), ResolutionObject))
                {
                    (*ResolutionObject)->TryGetNumberField(TEXT("width"), ResolutionWidth);
                    (*ResolutionObject)->TryGetNumberField(TEXT("height"), ResolutionHeight);
                }
            }

            FString LayoutDataString = GetSubsystem->GetLayoutData(ResolutionWidth, ResolutionHeight);
            TArray<TSharedPtr<FJsonValue>> LayoutDataArray;
            TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(LayoutDataString);
            if (FJsonSerializer::Deserialize(Reader, LayoutDataArray))
            {
                Response->SetStringField(TEXT("status"), TEXT("success"));
                Response->SetArrayField(TEXT("layout_data"), LayoutDataArray);
            }
            else
            {
                Response->SetStringField(TEXT("message"), TEXT("Failed to get layout data."));
            }
        }
    }
    else if (Command == TEXT("check_widget_overlap"))
    {
        FString DummyWidgetName;
        auto Subsystems = SetAttentionAndGetSubsystems(TEXT("asset_path"), DummyWidgetName);
        if (Subsystems.Key && Subsystems.Value)
        {
            UUmgGetSubsystem* GetSubsystem = Cast<UUmgGetSubsystem>(Subsystems.Value);
            const TArray<TSharedPtr<FJsonValue>>* WidgetIdsJsonArray = nullptr;
            TArray<FString> WidgetIds;
            if (Params->TryGetArrayField(TEXT("widget_ids"), WidgetIdsJsonArray))
            {
                for (const TSharedPtr<FJsonValue>& JsonValue : *WidgetIdsJsonArray)
                {
                    WidgetIds.Add(JsonValue->AsString());
                }
            }

            bool bOverlapExists = GetSubsystem->CheckWidgetOverlap(WidgetIds);
            Response->SetStringField(TEXT("status"), TEXT("success"));
            Response->SetBoolField(TEXT("overlap_exists"), bOverlapExists);
        }
    }
    else if (Command == TEXT("create_widget"))
    {
        FString DummyWidgetName;
        auto Subsystems = SetAttentionAndGetSubsystems(TEXT("asset_path"), DummyWidgetName);
        if (Subsystems.Key && Subsystems.Value)
        {
            UUmgSetSubsystem* SetSubsystem = Cast<UUmgSetSubsystem>(Subsystems.Value);
            FString ParentId, WidgetType, WidgetName;
            if (Params->TryGetStringField(TEXT("parent_id"), ParentId) &&
                Params->TryGetStringField(TEXT("widget_type"), WidgetType) &&
                Params->TryGetStringField(TEXT("widget_name"), WidgetName))
            {
                FString NewWidgetId = SetSubsystem->CreateWidget(ParentId, WidgetType, WidgetName);
                if (!NewWidgetId.IsEmpty())
                {
                    Response->SetStringField(TEXT("status"), TEXT("success"));
                    Response->SetStringField(TEXT("new_widget_id"), NewWidgetId);
                }
                else
                {
                    Response->SetStringField(TEXT("message"), TEXT("Failed to create widget."));
                }
            }
            else
            {
                Response->SetStringField(TEXT("message"), TEXT("Missing parameters for create_widget."));
            }
        }
    }
    else if (Command == TEXT("set_widget_properties"))
    {
        FString WidgetName;
        auto Subsystems = SetAttentionAndGetSubsystems(TEXT("widget_id"), WidgetName);
        if (Subsystems.Key && Subsystems.Value)
        {
            UUmgSetSubsystem* SetSubsystem = Cast<UUmgSetSubsystem>(Subsystems.Value);
            FString PropertiesJsonString;
            if (Params->TryGetStringField(TEXT("properties_json"), PropertiesJsonString))
            {
                if (SetSubsystem->SetWidgetProperties(WidgetName, PropertiesJsonString))
                {
                    Response->SetStringField(TEXT("status"), TEXT("success"));
                }
                else
                {
                    Response->SetStringField(TEXT("message"), TEXT("Failed to set widget properties."));
                }
            }
            else
            {
                Response->SetStringField(TEXT("message"), TEXT("Missing properties_json parameter."));
            }
        }
    }
    else if (Command == TEXT("delete_widget"))
    {
        FString WidgetName;
        auto Subsystems = SetAttentionAndGetSubsystems(TEXT("widget_id"), WidgetName);
        if (Subsystems.Key && Subsystems.Value)
        {
            UUmgSetSubsystem* SetSubsystem = Cast<UUmgSetSubsystem>(Subsystems.Value);
            if (SetSubsystem->DeleteWidget(WidgetName))
            {
                Response->SetStringField(TEXT("status"), TEXT("success"));
            }
            else
            {
                Response->SetStringField(TEXT("message"), TEXT("Failed to delete widget."));
            }
        }
    }
    else if (Command == TEXT("reparent_widget"))
    {
        FString WidgetName;
        auto Subsystems = SetAttentionAndGetSubsystems(TEXT("widget_id"), WidgetName);
        if (Subsystems.Key && Subsystems.Value)
        {
            UUmgSetSubsystem* SetSubsystem = Cast<UUmgSetSubsystem>(Subsystems.Value);
            FString NewParentId, NewParentName;
            if (Params->TryGetStringField(TEXT("new_parent_id"), NewParentId))
            {
                // We assume the new parent is in the same asset, so we only need its name.
                if (!NewParentId.Split(TEXT(":"), nullptr, &NewParentName, ESearchCase::IgnoreCase, ESearchDir::FromEnd))
                {
                    NewParentName = NewParentId; // Assume it's just the name if split fails
                }

                if (SetSubsystem->ReparentWidget(WidgetName, NewParentName))
                {
                    Response->SetStringField(TEXT("status"), TEXT("success"));
                }
                else
                {
                    Response->SetStringField(TEXT("message"), TEXT("Failed to reparent widget."));
                }
            }
            else
            {
                Response->SetStringField(TEXT("message"), TEXT("Missing new_parent_id parameter."));
            }
        }
    }

    return Response;
}
