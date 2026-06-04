// Copyright (c) 2025-2026 Winyunq. All rights reserved.
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

    if (!GEditor)
    {
        Response->SetBoolField(TEXT("success"), false);
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
            Response->SetBoolField(TEXT("success"), false);
            Response->SetStringField(TEXT("error"), ErrorMessage);
            return Response;
        }
    }

    // --- GET/QUERY COMMANDS ---
    if (Command == TEXT("get_widget_tree"))
    {
        UUmgGetSubsystem* GetSubsystem = GEditor->GetEditorSubsystem<UUmgGetSubsystem>();
        FString WidgetTreeString = GetSubsystem->GetWidgetTree(TargetBlueprint);
        if (!WidgetTreeString.IsEmpty())
        {
            Response->SetBoolField(TEXT("success"), true);
            Response->SetStringField(TEXT("widget_tree"), WidgetTreeString);
        }
        else
        {
            Response->SetBoolField(TEXT("success"), false);
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
                Response->SetBoolField(TEXT("success"), true);
                for (const auto& Field : QueriedProperties->Values)
                {
                    Response->SetField(Field.Key, Field.Value);
                }
            }
            else
            {
                Response->SetBoolField(TEXT("success"), false);
                Response->SetStringField(TEXT("error"), TEXT("Failed to query widget properties or parse response."));
            }
        }
        else
        {
            Response->SetBoolField(TEXT("success"), false);
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
                Response->SetBoolField(TEXT("success"), true);
                for (const auto& Field : SchemaJson->Values)
                {
                    Response->SetField(Field.Key, Field.Value);
                }
            }
            else
            {
                Response->SetBoolField(TEXT("success"), false);
                Response->SetStringField(TEXT("error"), TEXT("Failed to get widget schema or parse response."));
            }
        }
        else
        {
            Response->SetBoolField(TEXT("success"), false);
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
                Response->SetBoolField(TEXT("success"), true);
                Response->SetStringField(TEXT("new_widget_id"), NewWidgetId);
            }
            else
            {
                Response->SetBoolField(TEXT("success"), false);
                Response->SetStringField(TEXT("error"), TEXT("Failed to create widget. Check logs for details."));
            }
        }
        else
        {
            Response->SetBoolField(TEXT("success"), false);
            Response->SetStringField(TEXT("error"), TEXT("Missing parameters for create_widget (widget_type, new_widget_name)."));
        }
    }
    else if (Command == TEXT("set_active_widget"))
    {
         FString WidgetName;
         if (Params->TryGetStringField(TEXT("widget_name"), WidgetName))
         {
             if (UUmgAttentionSubsystem* AttentionSubsystem = GEditor->GetEditorSubsystem<UUmgAttentionSubsystem>())
             {
                 AttentionSubsystem->SetTargetWidget(WidgetName);
                 Response->SetBoolField(TEXT("success"), true);
                 Response->SetStringField(TEXT("widget_name"), WidgetName);
             }
             else
             {
                 Response->SetBoolField(TEXT("success"), false);
                 Response->SetStringField(TEXT("error"), TEXT("Failed to get UmgAttentionSubsystem."));
             }
         }
         else
         {
             Response->SetBoolField(TEXT("success"), false);
             Response->SetStringField(TEXT("error"), TEXT("Missing 'widget_name' parameter."));
         }
    }
    else if (Command == TEXT("set_widget_properties"))
    {
        UUmgSetSubsystem* SetSubsystem = GEditor->GetEditorSubsystem<UUmgSetSubsystem>();
        FString WidgetName;
        const TSharedPtr<FJsonObject>* PropertiesPtr;
        if (Params->TryGetObjectField(TEXT("properties"), PropertiesPtr))
        {
            if (!Params->TryGetStringField(TEXT("widget_name"), WidgetName) || WidgetName.IsEmpty())
            {
                if (UUmgAttentionSubsystem* AttentionSubsystem = GEditor->GetEditorSubsystem<UUmgAttentionSubsystem>())
                {
                    WidgetName = AttentionSubsystem->GetTargetWidget();
                }
            }

            if (WidgetName.IsEmpty())
            {
                Response->SetBoolField(TEXT("success"), false);
                Response->SetStringField(TEXT("error"), TEXT("Widget name was not specified and no active widget target was focused. Please specify 'widget_name' or focus a target."));
                return Response;
            }

            TSharedPtr<FJsonObject> PropertiesJsonObject = *PropertiesPtr;
            FString PropertiesJsonString;
            TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&PropertiesJsonString);
            FJsonSerializer::Serialize(PropertiesJsonObject.ToSharedRef(), Writer);
            Writer->Close(); // Explicitly close to ensure flush

            UE_LOG(LogTemp, Log, TEXT("UmgMcpWidgetCommands: Serialized Properties JSON for '%s': %s"), *WidgetName, *PropertiesJsonString);

            if (SetSubsystem->SetWidgetProperties(TargetBlueprint, WidgetName, PropertiesJsonString))
            {
                Response->SetBoolField(TEXT("success"), true);
                Response->SetStringField(TEXT("widget"), WidgetName);
                Response->SetObjectField(TEXT("properties_applied"), PropertiesJsonObject);
            }
            else
            {
                Response->SetBoolField(TEXT("success"), false);
                Response->SetStringField(TEXT("error"), TEXT("Failed to set widget properties. Check logs for details."));
            }
        }
        else
        {
            Response->SetBoolField(TEXT("success"), false);
            Response->SetStringField(TEXT("error"), TEXT("Missing 'properties' (as a JSON object) parameter."));
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
                Response->SetBoolField(TEXT("success"), true);
                Response->SetStringField(TEXT("deleted_widget"), WidgetName);
            }
            else
            {
                Response->SetBoolField(TEXT("success"), false);
                Response->SetStringField(TEXT("error"), TEXT("Failed to delete widget. Check logs for details."));
            }
        }
        else
        {
            Response->SetBoolField(TEXT("success"), false);
            Response->SetStringField(TEXT("error"), TEXT("Missing 'widget_name' parameter."));
        }
    }
    else if (Command == TEXT("reparent_widget"))
    {
        UUmgSetSubsystem* SetSubsystem = GEditor->GetEditorSubsystem<UUmgSetSubsystem>();
        FString WidgetName, NewParentWidgetJsonStr;

        // 1. widget_name is optional (fallback to focused target)
        if (!Params->TryGetStringField(TEXT("widget_name"), WidgetName) || WidgetName.IsEmpty())
        {
            if (UUmgAttentionSubsystem* AttentionSubsystem = GEditor->GetEditorSubsystem<UUmgAttentionSubsystem>())
            {
                WidgetName = AttentionSubsystem->GetTargetWidget();
            }
        }

        if (WidgetName.IsEmpty())
        {
            Response->SetBoolField(TEXT("success"), false);
            Response->SetStringField(TEXT("error"), TEXT("Widget name to reparent was not specified and no active widget target was focused."));
            return Response;
        }

        // 2. new_parent_widget (JSON object or string) is required
        const TSharedPtr<FJsonObject>* NewParentWidgetPtr = nullptr;
        if (Params->TryGetObjectField(TEXT("new_parent_widget"), NewParentWidgetPtr))
        {
            TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&NewParentWidgetJsonStr);
            FJsonSerializer::Serialize(NewParentWidgetPtr->ToSharedRef(), Writer);
            Writer->Close();
        }
        else if (Params->TryGetStringField(TEXT("new_parent_widget"), NewParentWidgetJsonStr))
        {
            // already a string
        }

        if (NewParentWidgetJsonStr.IsEmpty())
        {
            Response->SetBoolField(TEXT("success"), false);
            Response->SetStringField(TEXT("error"), TEXT("Missing required parameter 'new_parent_widget' (JSON container specification)."));
            return Response;
        }

        TArray<FString> Affected = SetSubsystem->ReparentWidget(TargetBlueprint, WidgetName, NewParentWidgetJsonStr);
        if (Affected.Num() > 0)
        {
            Response->SetBoolField(TEXT("success"), true);
            TArray<TSharedPtr<FJsonValue>> AffectedJsonArray;
            for (const FString& Name : Affected)
            {
                AffectedJsonArray.Add(MakeShared<FJsonValueString>(Name));
            }
            Response->SetArrayField(TEXT("affected_widgets"), AffectedJsonArray);
        }
        else
        {
            Response->SetBoolField(TEXT("success"), false);
            Response->SetStringField(TEXT("error"), TEXT("Failed to reparent/wrap widget. Check logs for details."));
        }
    }
    else if (Command == TEXT("move_widget"))
    {
        UUmgSetSubsystem* SetSubsystem = GEditor->GetEditorSubsystem<UUmgSetSubsystem>();
        FString TargetParentName, WidgetName;

        // 1. widget_name (dragged widget) is required
        if (!Params->TryGetStringField(TEXT("widget_name"), WidgetName) || WidgetName.IsEmpty())
        {
            Response->SetBoolField(TEXT("success"), false);
            Response->SetStringField(TEXT("error"), TEXT("Missing required parameter 'widget_name' (the widget to move)."));
            return Response;
        }

        // 2. target (parent to move to) is optional (fallback to focused target)
        if (!Params->TryGetStringField(TEXT("target"), TargetParentName) || TargetParentName.IsEmpty())
        {
            if (UUmgAttentionSubsystem* AttentionSubsystem = GEditor->GetEditorSubsystem<UUmgAttentionSubsystem>())
            {
                TargetParentName = AttentionSubsystem->GetTargetWidget();
            }
        }

        if (TargetParentName.IsEmpty())
        {
            Response->SetBoolField(TEXT("success"), false);
            Response->SetStringField(TEXT("error"), TEXT("Target parent name was not specified and no active widget target was focused."));
            return Response;
        }

        if (SetSubsystem->MoveWidget(TargetBlueprint, TargetParentName, WidgetName))
        {
            Response->SetBoolField(TEXT("success"), true);
            Response->SetStringField(TEXT("widget"), WidgetName);
            Response->SetStringField(TEXT("new_parent"), TargetParentName);
        }
        else
        {
            Response->SetBoolField(TEXT("success"), false);
            Response->SetStringField(TEXT("error"), TEXT("Failed to move widget. Check logs for details."));
        }
    }
    else if (Command == TEXT("save_asset"))
    {
        UUmgSetSubsystem* SetSubsystem = GEditor->GetEditorSubsystem<UUmgSetSubsystem>();
        if (SetSubsystem->SaveAsset(TargetBlueprint))
        {
            Response->SetBoolField(TEXT("success"), true);
            Response->SetStringField(TEXT("saved_asset"), TargetBlueprint->GetPathName());
        }
        else
        {
            Response->SetBoolField(TEXT("success"), false);
            Response->SetStringField(TEXT("error"), TEXT("Failed to save asset. Check logs for details."));
        }
    }

    else
    {
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), FString::Printf(TEXT("Unknown or not implemented widget command: %s"), *Command));
    }

    return Response;
}



