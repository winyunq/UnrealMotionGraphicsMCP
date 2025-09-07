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
    // else if (Command == TEXT("set_widget_properties"))
    // {
    //     FString WidgetId;
    //     const TSharedPtr<FJsonObject>* PropertiesJsonObj;
    //     if (Params && Params->TryGetStringField(TEXT("widget_id"), WidgetId) && Params->TryGetObjectField(TEXT("properties"), PropertiesJsonObj))
    //     {
    //         FString PropertiesJsonString;
    //         TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&PropertiesJsonString);
    //         FJsonSerializer::Serialize(PropertiesJsonObj->ToSharedRef(), Writer);

    //         UUmgSetSubsystem* UmgSetSubsystem = GEditor->GetEditorSubsystem<UUmgSetSubsystem>();
    //         if (UmgSetSubsystem && UmgSetSubsystem->SetWidgetProperties(WidgetId, PropertiesJsonString))
    //         {
    //             Response->SetStringField(TEXT("status"), TEXT("success"));
    //         }
    //         else
    //         {
    //             Response->SetStringField(TEXT("message"), TEXT("Failed to set widget properties."));
    //         }
    //     }
    //     else
    //     {
    //         Response->SetStringField(TEXT("message"), TEXT("Missing 'widget_id' or 'properties' parameter."));
    //     }
    // }
    // ... other commands
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