// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#include "Widget/UmgMcpWidgetCommands.h"
#include "Bridge/UmgMcpJsonCompat.h"
#include "Bridge/UmgMcpCommonUtils.h"
#include "Widget/UmgGetSubsystem.h"
#include "Widget/UmgSetSubsystem.h"
#include "FileManage/UmgAttentionSubsystem.h"
#include "Editor.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
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
        FString ScopedWidgetName;
        if (UUmgAttentionSubsystem* AttentionSubsystem = GEditor->GetEditorSubsystem<UUmgAttentionSubsystem>())
        {
            ScopedWidgetName = AttentionSubsystem->GetTargetWidget();
        }

        bool bScopedWidgetFound = false;
        if (!ScopedWidgetName.IsEmpty() && TargetBlueprint && TargetBlueprint->WidgetTree)
        {
            bScopedWidgetFound = TargetBlueprint->WidgetTree->FindWidget(FName(*ScopedWidgetName)) != nullptr;
        }

        FString WidgetTreeString = GetSubsystem->GetWidgetTree(TargetBlueprint, ScopedWidgetName);
        if (!WidgetTreeString.IsEmpty())
        {
            Response->SetBoolField(TEXT("success"), true);
            Response->SetStringField(TEXT("widget_tree"), WidgetTreeString);
            if (!ScopedWidgetName.IsEmpty() && bScopedWidgetFound)
            {
                Response->SetStringField(TEXT("root_widget"), ScopedWidgetName);
                Response->SetStringField(TEXT("scope"), TEXT("target_widget"));
            }
            else
            {
                Response->SetStringField(TEXT("scope"), TEXT("root"));
                if (!ScopedWidgetName.IsEmpty())
                {
                    Response->SetStringField(TEXT("scope_warning"), FString::Printf(TEXT("Focused widget '%s' was not found; returned root tree."), *ScopedWidgetName));
                }
            }
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
                    Response->SetField(UmgMcpJsonCompat::KeyToString(Field.Key), Field.Value);
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
                    Response->SetField(UmgMcpJsonCompat::KeyToString(Field.Key), Field.Value);
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
    else if (Command == TEXT("get_layout_data"))
    {
        UUmgGetSubsystem* GetSubsystem = GEditor->GetEditorSubsystem<UUmgGetSubsystem>();
        int32 ResolutionWidth = 1920;
        int32 ResolutionHeight = 1080;
        if (Params.IsValid())
        {
            if (Params->HasField(TEXT("resolution_width")))
            {
                ResolutionWidth = Params->GetIntegerField(TEXT("resolution_width"));
            }
            if (Params->HasField(TEXT("resolution_height")))
            {
                ResolutionHeight = Params->GetIntegerField(TEXT("resolution_height"));
            }
        }

        FString LayoutJsonString = GetSubsystem->GetLayoutData(TargetBlueprint, ResolutionWidth, ResolutionHeight);
        TArray<TSharedPtr<FJsonValue>> LayoutArray;
        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(LayoutJsonString);
        if (FJsonSerializer::Deserialize(Reader, LayoutArray))
        {
            Response->SetBoolField(TEXT("success"), true);
            Response->SetArrayField(TEXT("layout_data"), LayoutArray);
            Response->SetNumberField(TEXT("resolution_width"), ResolutionWidth);
            Response->SetNumberField(TEXT("resolution_height"), ResolutionHeight);
        }
        else
        {
            Response->SetBoolField(TEXT("success"), false);
            Response->SetStringField(TEXT("error"), TEXT("Failed to get layout data or parse response."));
        }
    }
    
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
                 if (AttentionSubsystem->SetTargetWidget(WidgetName))
                 {
                     Response->SetBoolField(TEXT("success"), true);
                     Response->SetStringField(TEXT("widget_name"), WidgetName);
                 }
                 else
                 {
                     Response->SetBoolField(TEXT("success"), false);
                     Response->SetStringField(TEXT("error"), FString::Printf(TEXT("Widget '%s' was not found in the current target asset."), *WidgetName));
                 }
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
            bool bConfirmed = false;
            Params->TryGetBoolField(TEXT("confirm_delete"), bConfirmed);
            if (!bConfirmed)
            {
                Response->SetBoolField(TEXT("success"), false);
                Response->SetStringField(TEXT("error"), TEXT("Deletion hardened (Issue 15): set 'confirm_delete': true to delete a widget explicitly."));
                return Response;
            }

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
    else if (Command == TEXT("reorder_widget_tree"))
    {
        UUmgSetSubsystem* SetSubsystem = GEditor->GetEditorSubsystem<UUmgSetSubsystem>();
        FString RootName;
        if (!Params->TryGetStringField(TEXT("root"), RootName))
        {
            Params->TryGetStringField(TEXT("root_name"), RootName);
        }

        FString TreeSpec;
        TSharedPtr<FJsonValue> TreeValue = Params->TryGetField(TEXT("tree"));
        if (!TreeValue.IsValid())
        {
            Response->SetBoolField(TEXT("success"), false);
            Response->SetStringField(TEXT("error"), TEXT("Missing required parameter 'tree'."));
            return Response;
        }

        if (TreeValue->Type == EJson::String)
        {
            TreeSpec = TreeValue->AsString();
        }
        else
        {
            TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&TreeSpec);
            FJsonSerializer::Serialize(TreeValue.ToSharedRef(), TEXT(""), Writer);
            Writer->Close();
        }

        TArray<FString> ReorderedWidgets;
        TArray<FString> Warnings;
        FString Error;
        if (SetSubsystem->ReorderWidgetTree(TargetBlueprint, RootName, TreeSpec, ReorderedWidgets, Warnings, Error))
        {
            Response->SetBoolField(TEXT("success"), true);
            if (!RootName.IsEmpty())
            {
                Response->SetStringField(TEXT("root"), RootName);
            }

            TArray<TSharedPtr<FJsonValue>> ReorderedJson;
            for (const FString& Item : ReorderedWidgets)
            {
                ReorderedJson.Add(MakeShared<FJsonValueString>(Item));
            }
            Response->SetArrayField(TEXT("reordered_widgets"), ReorderedJson);
            Response->SetNumberField(TEXT("reordered_count"), ReorderedWidgets.Num());

            TArray<TSharedPtr<FJsonValue>> WarningJson;
            for (const FString& Warning : Warnings)
            {
                WarningJson.Add(MakeShared<FJsonValueString>(Warning));
            }
            Response->SetArrayField(TEXT("warnings"), WarningJson);
        }
        else
        {
            Response->SetBoolField(TEXT("success"), false);
            Response->SetStringField(TEXT("error"), Error.IsEmpty() ? TEXT("Failed to reorder widget tree.") : Error);
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



