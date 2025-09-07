#include "UmgGetSubsystem.h"
#include "UmgSetSubsystem.h"
#include "Commands/UmgMcpWidgetCommands.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/UserWidget.h"
#include "Components/PanelWidget.h"
#include "WidgetBlueprint.h"
#include "UObject/UObjectGlobals.h"
#include "Serialization/JsonSerializer.h"

TSharedPtr<FJsonObject> FUmgMcpWidgetCommands::HandleCommand(const FString& Command, const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Response = MakeShareable(new FJsonObject);
    Response->SetStringField(TEXT("status"), TEXT("error"));

    if (Command == TEXT("get_widget_tree"))
    {
        FString AssetPath;
        if (Params && Params->TryGetStringField(TEXT("asset_path"), AssetPath))
        {
            UWidgetBlueprint* WidgetBlueprint = Cast<UWidgetBlueprint>(StaticLoadObject(UWidgetBlueprint::StaticClass(), nullptr, *AssetPath));
            if (WidgetBlueprint && WidgetBlueprint->WidgetTree)
            {
                TSharedPtr<FJsonObject> RootWidgetJson = BuildWidgetJson(WidgetBlueprint->WidgetTree->RootWidget);
                Response->SetStringField(TEXT("status"), TEXT("success"));
                Response->SetObjectField(TEXT("widget_tree"), RootWidgetJson);
        }
        else
        {
            Response->SetStringField(TEXT("message"), TEXT("Failed to load Widget Blueprint or WidgetTree is null."));
        }
    }
    else
    {
        Response->SetStringField(TEXT("message"), TEXT("Missing 'asset_path' parameter."));
    }
}
    else if (Command == TEXT("query_widget_properties"))
    {
        FString WidgetId;
        const TArray<TSharedPtr<FJsonValue>>* PropertiesJsonArray;
        if (Params && Params->TryGetStringField(TEXT("widget_id"), WidgetId) &&
            Params->TryGetArrayField(TEXT("properties"), PropertiesJsonArray))
        {
            TArray<FString> PropertiesToQuery;
            for (const TSharedPtr<FJsonValue>& JsonValue : *PropertiesJsonArray)
            {
                PropertiesToQuery.Add(JsonValue->AsString());
            }

            UUmgGetSubsystem* UmgGetSubsystem = GEditor->GetEditorSubsystem<UUmgGetSubsystem>();
            if (UmgGetSubsystem)
            {
                FString QueriedPropertiesString = UmgGetSubsystem->QueryWidgetProperties(WidgetId, PropertiesToQuery);
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
                Response->SetStringField(TEXT("message"), TEXT("UmgGetSubsystem not found."));
            }
        }
        else
        {
            Response->SetStringField(TEXT("message"), TEXT("Missing widget_id or properties parameter."));
        }
    }
    else if (Command == TEXT("get_layout_data"))
    {
        FString AssetPath;
        int32 ResolutionWidth = 1920;
        int32 ResolutionHeight = 1080;

        if (Params && Params->TryGetStringField(TEXT("asset_path"), AssetPath))
        {
            if (Params->HasField(TEXT("resolution")))
            {
                const TSharedPtr<FJsonObject>* ResolutionObject;
                if (Params->TryGetObjectField(TEXT("resolution"), ResolutionObject))
                {
                    (*ResolutionObject)->TryGetNumberField(TEXT("width"), ResolutionWidth);
                    (*ResolutionObject)->TryGetNumberField(TEXT("height"), ResolutionHeight);
                }
            }

            UUmgGetSubsystem* UmgGetSubsystem = GEditor->GetEditorSubsystem<UUmgGetSubsystem>();
            if (UmgGetSubsystem)
            {
                FString LayoutDataString = UmgGetSubsystem->GetLayoutData(AssetPath, ResolutionWidth, ResolutionHeight);
                TSharedPtr<FJsonObject> LayoutData;
                TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(LayoutDataString);
                if (FJsonSerializer::Deserialize(Reader, LayoutData) && LayoutData.IsValid())
                {
                    Response->SetStringField(TEXT("status"), TEXT("success"));
                    Response->SetObjectField(TEXT("layout_data"), LayoutData.ToSharedRef());
                }
                else
                {
                    Response->SetStringField(TEXT("message"), TEXT("Failed to get layout data."));
                }
            }
            else
            {
                Response->SetStringField(TEXT("message"), TEXT("UmgGetSubsystem not found."));
            }
        }
        else
        {
            Response->SetStringField(TEXT("message"), TEXT("Missing asset_path parameter."));
        }
    }
    else if (Command == TEXT("check_widget_overlap"))
    {
        FString AssetPath;
        const TArray<TSharedPtr<FJsonValue>>* WidgetIdsJsonArray = nullptr; // Optional
        TArray<FString> WidgetIds;

        if (Params && Params->TryGetStringField(TEXT("asset_path"), AssetPath))
        {
            if (Params->TryGetArrayField(TEXT("widget_ids"), WidgetIdsJsonArray))
            {
                for (const TSharedPtr<FJsonValue>& JsonValue : *WidgetIdsJsonArray)
                {
                    WidgetIds.Add(JsonValue->AsString());
                }
            }

            UUmgGetSubsystem* UmgGetSubsystem = GEditor->GetEditorSubsystem<UUmgGetSubsystem>();
            if (UmgGetSubsystem)
            {
                bool bOverlapExists = UmgGetSubsystem->CheckWidgetOverlap(AssetPath, WidgetIds);
                Response->SetStringField(TEXT("status"), TEXT("success"));
                Response->SetBoolField(TEXT("overlap_exists"), bOverlapExists);
            }
            else
            {
                Response->SetStringField(TEXT("message"), TEXT("UmgGetSubsystem not found."));
            }
        }
        else
        {
            Response->SetStringField(TEXT("message"), TEXT("Missing asset_path parameter."));
        }
    }
    else if (Command == TEXT("create_widget"))
        {
            FString AssetPath, ParentId, WidgetType, WidgetName;
            if (Params && Params->TryGetStringField(TEXT("asset_path"), AssetPath) &&
                Params->TryGetStringField(TEXT("parent_id"), ParentId) &&
                Params->TryGetStringField(TEXT("widget_type"), WidgetType) &&
                Params->TryGetStringField(TEXT("widget_name"), WidgetName))
            {
                UUmgSetSubsystem* UmgSetSubsystem = GEditor->GetEditorSubsystem<UUmgSetSubsystem>();
                if (UmgSetSubsystem && !UmgSetSubsystem->CreateWidget(AssetPath, ParentId, WidgetType, WidgetName).IsEmpty())
                {
                    Response->SetStringField(TEXT("status"), TEXT("success"));
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
    else if (Command == TEXT("set_widget_properties"))
    {
        FString WidgetId;
        FString PropertiesJsonString;
        bool bHasWidgetId = false;
        bool bHasPropertiesJson = false;

        if (Params)
        {
            bHasWidgetId = Params->TryGetStringField(TEXT("widget_id"), WidgetId);
            bHasPropertiesJson = Params->TryGetStringField(TEXT("properties_json"), PropertiesJsonString);
        }

        if (bHasWidgetId && bHasPropertiesJson)
        {
            UUmgSetSubsystem* UmgSetSubsystem = GEditor->GetEditorSubsystem<UUmgSetSubsystem>();
            if (UmgSetSubsystem && UmgSetSubsystem->SetWidgetProperties(WidgetId, PropertiesJsonString))
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
            Response->SetStringField(TEXT("message"), TEXT("Missing widget_id or properties_json parameter."));
        }
    }
    else if (Command == TEXT("delete_widget"))
    {
        FString WidgetId;
        if (Params && Params->TryGetStringField(TEXT("widget_id"), WidgetId))
        {
            UUmgSetSubsystem* UmgSetSubsystem = GEditor->GetEditorSubsystem<UUmgSetSubsystem>();
            if (UmgSetSubsystem && UmgSetSubsystem->DeleteWidget(WidgetId))
            {
                Response->SetStringField(TEXT("status"), TEXT("success"));
            }
            else
            {
                Response->SetStringField(TEXT("message"), TEXT("Failed to delete widget."));
            }
        }
        else
        {
            Response->SetStringField(TEXT("message"), TEXT("Missing widget_id parameter."));
        }
    }
    else if (Command == TEXT("reparent_widget"))
    {
        FString WidgetId, NewParentId;
        if (Params && Params->TryGetStringField(TEXT("widget_id"), WidgetId) &&
            Params->TryGetStringField(TEXT("new_parent_id"), NewParentId))
        {
            UUmgSetSubsystem* UmgSetSubsystem = GEditor->GetEditorSubsystem<UUmgSetSubsystem>();
            if (UmgSetSubsystem && UmgSetSubsystem->ReparentWidget(WidgetId, NewParentId))
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
            Response->SetStringField(TEXT("message"), TEXT("Missing widget_id or new_parent_id parameter."));
        }
    }
    return Response;
}

TSharedPtr<FJsonObject> FUmgMcpWidgetCommands::BuildWidgetJson(UWidget* CurrentWidget)
{
    TSharedPtr<FJsonObject> WidgetJson = MakeShareable(new FJsonObject);

    if (!CurrentWidget)
    {
        return WidgetJson; // Return empty object for null widget
    }

    WidgetJson->SetStringField(TEXT("WidgetId"), CurrentWidget->GetPathName()); // Using PathName for unique ID
    WidgetJson->SetStringField(TEXT("WidgetName"), CurrentWidget->GetName());
    WidgetJson->SetStringField(TEXT("WidgetType"), CurrentWidget->GetClass()->GetName());

    TArray<TSharedPtr<FJsonValue>> ChildrenArray;
    UPanelWidget* PanelWidget = Cast<UPanelWidget>(CurrentWidget);
    if (PanelWidget)
    {
        for (UWidget* ChildWidget : PanelWidget->GetAllChildren()) // Use GetAllChildren()
        {
            ChildrenArray.Add(MakeShareable(new FJsonValueObject(BuildWidgetJson(ChildWidget))));
        }
    }
    WidgetJson->SetArrayField(TEXT("Children"), ChildrenArray);

    return WidgetJson;
}