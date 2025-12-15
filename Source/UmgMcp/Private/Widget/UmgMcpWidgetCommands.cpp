#include "Widget/UmgMcpWidgetCommands.h"
#include "Bridge/UmgMcpCommonUtils.h"
#include "Widget/UmgGetSubsystem.h"
#include "Widget/UmgSetSubsystem.h"
#include "FileManage/UmgAttentionSubsystem.h"
#include "Editor.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
#include "WidgetBlueprint.h"
#include "Misc/PackageName.h"



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
    UWidgetBlueprint* TargetBlueprint = nullptr;

    // Some commands don't require a target blueprint
    if (Command != TEXT("get_widget_schema"))
    {
        TargetBlueprint = FUmgMcpCommonUtils::GetTargetWidgetBlueprint(Params, ErrorMessage);

        if (!TargetBlueprint)
        {
            Response->SetStringField(TEXT("error"), ErrorMessage);
            return Response;
        }
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
    else if (Command == TEXT("get_widget_schema"))
    {
        UUmgGetSubsystem* GetSubsystem = GEditor->GetEditorSubsystem<UUmgGetSubsystem>();
        FString WidgetType;
        if (Params->TryGetStringField(TEXT("widget_type"), WidgetType))
        {
            FString SchemaJsonString = GetSubsystem->GetWidgetSchema(WidgetType);
            TSharedPtr<FJsonObject> SchemaJson;
            TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(SchemaJsonString);
            if (FJsonSerializer::Deserialize(Reader, SchemaJson) && SchemaJson.IsValid())
            {
                Response->SetStringField(TEXT("status"), TEXT("success"));
                Response->SetObjectField(TEXT("data"), SchemaJson);
            }
            else
            {
                Response->SetStringField(TEXT("error"), TEXT("Failed to get widget schema or parse response."));
            }
        }
        else
        {
            Response->SetStringField(TEXT("error"), TEXT("Missing 'widget_type' parameter."));
        }
    }
    // ... other GET commands
    
    // --- SET/ACTION COMMANDS ---
    else if (Command == TEXT("create_widget"))
    {
        UUmgSetSubsystem* SetSubsystem = GEditor->GetEditorSubsystem<UUmgSetSubsystem>();
        FString ParentName, WidgetType, NewWidgetName;
        if (Params->TryGetStringField(TEXT("widget_type"), WidgetType) &&
            Params->TryGetStringField(TEXT("new_widget_name"), NewWidgetName))
        {
            Params->TryGetStringField(TEXT("parent_name"), ParentName); // Optional
            
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
            Response->SetStringField(TEXT("error"), TEXT("Missing parameters for create_widget (widget_type, new_widget_name)."));
        }
    }
    // NEW: set_active_widget
    else if (Command == TEXT("set_active_widget"))
    {
         FString WidgetName;
         if (Params->TryGetStringField(TEXT("widget_name"), WidgetName))
         {
             if (GEditor)
             {
                 if (UUmgAttentionSubsystem* AttentionSubsystem = GEditor->GetEditorSubsystem<UUmgAttentionSubsystem>())
                 {
                     AttentionSubsystem->SetTargetWidget(WidgetName);
                     Response->SetStringField(TEXT("status"), TEXT("success"));
                     Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Active Widget Scope set to '%s'. Future create_widget calls will default to this parent."), *WidgetName));
                 }
                 else
                 {
                      Response->SetStringField(TEXT("error"), TEXT("Failed to get UmgAttentionSubsystem."));
                 }
             }
         }
         else
         {
             Response->SetStringField(TEXT("error"), TEXT("Missing 'widget_name' parameter."));
         }
    }
    else if (Command == TEXT("set_widget_properties"))
    {
        UUmgSetSubsystem* SetSubsystem = GEditor->GetEditorSubsystem<UUmgSetSubsystem>();
        FString WidgetName;
        const TSharedPtr<FJsonObject>* PropertiesPtr;
        if (Params->TryGetStringField(TEXT("widget_name"), WidgetName) && Params->TryGetObjectField(TEXT("properties"), PropertiesPtr))
        {
            TSharedPtr<FJsonObject> PropertiesJsonObject = *PropertiesPtr;
            FString PropertiesJsonString;
            TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&PropertiesJsonString);
            FJsonSerializer::Serialize(PropertiesJsonObject.ToSharedRef(), Writer);
            Writer->Close(); // Explicitly close to ensure flush

            UE_LOG(LogTemp, Log, TEXT("UmgMcpWidgetCommands: Serialized Properties JSON for '%s': %s"), *WidgetName, *PropertiesJsonString);

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
    else if (Command == TEXT("delete_widget"))
    {
        UUmgSetSubsystem* SetSubsystem = GEditor->GetEditorSubsystem<UUmgSetSubsystem>();
        FString WidgetName;
        if (Params->TryGetStringField(TEXT("widget_name"), WidgetName))
        {
            if (SetSubsystem->DeleteWidget(TargetBlueprint, WidgetName))
            {
                Response->SetStringField(TEXT("status"), TEXT("success"));
            }
            else
            {
                Response->SetStringField(TEXT("error"), TEXT("Failed to delete widget. Check logs for details."));
            }
        }
        else
        {
            Response->SetStringField(TEXT("error"), TEXT("Missing 'widget_name' parameter."));
        }
    }
    else if (Command == TEXT("reparent_widget"))
    {
        UUmgSetSubsystem* SetSubsystem = GEditor->GetEditorSubsystem<UUmgSetSubsystem>();
        FString WidgetName, NewParentName;
        if (Params->TryGetStringField(TEXT("widget_name"), WidgetName) && Params->TryGetStringField(TEXT("new_parent_name"), NewParentName))
        {
            if (SetSubsystem->ReparentWidget(TargetBlueprint, WidgetName, NewParentName))
            {
                Response->SetStringField(TEXT("status"), TEXT("success"));
            }
            else
            {
                Response->SetStringField(TEXT("error"), TEXT("Failed to reparent widget. Check logs for details."));
            }
        }
        else
        {
            Response->SetStringField(TEXT("error"), TEXT("Missing 'widget_name' or 'new_parent_name' parameter."));
        }
    }
    else if (Command == TEXT("save_asset"))
    {
        UUmgSetSubsystem* SetSubsystem = GEditor->GetEditorSubsystem<UUmgSetSubsystem>();
        if (SetSubsystem->SaveAsset(TargetBlueprint))
        {
            Response->SetStringField(TEXT("status"), TEXT("success"));
        }
        else
        {
            Response->SetStringField(TEXT("error"), TEXT("Failed to save asset. Check logs for details."));
        }
    }

    else
    {
        Response->SetStringField(TEXT("error"), FString::Printf(TEXT("Unknown or not implemented widget command: %s"), *Command));
    }

    return Response;
}



