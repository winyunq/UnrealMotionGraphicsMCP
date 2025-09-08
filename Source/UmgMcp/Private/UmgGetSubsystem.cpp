#include "UmgGetSubsystem.h"
#include "UmgAttentionSubsystem.h"
#include "Editor.h"

// --- Necessary Includes ---
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetTree.h"
#include "Components/PanelWidget.h"
#include "Components/PanelSlot.h"
#include "JsonObjectConverter.h"
#include "Serialization/JsonWriter.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "UObject/UnrealType.h"
#include "Misc/PackageName.h"
#include "Widgets/SWidget.h"
#include "Layout/Geometry.h"
// --- End Includes ---

DEFINE_LOG_CATEGORY(LogUmgGet);

// Helper function to get the cached blueprint from the attention subsystem
static UWidgetBlueprint* GetCachedWidgetBlueprint(FString& OutError)
{
    if (!GEditor)
    {
        OutError = TEXT("GEditor is not available.");
        return nullptr;
    }
    UUmgAttentionSubsystem* AttentionSubsystem = GEditor->GetEditorSubsystem<UUmgAttentionSubsystem>();
    if (!AttentionSubsystem)
    {
        OutError = TEXT("UmgAttentionSubsystem is not available.");
        return nullptr;
    }
    UWidgetBlueprint* WidgetBlueprint = AttentionSubsystem->GetCachedTargetWidgetBlueprint();
    if (!WidgetBlueprint)
    {
        OutError = TEXT("No cached target UMG asset found. Please set a target first.");
        return nullptr;
    }
    return WidgetBlueprint;
}


// --- Helper function for recursive JSON export ---
static TSharedPtr<FJsonObject> ExportWidgetToJson(UWidget* Widget)
{
    if (!Widget)
    {
        return nullptr;
    }

    TSharedPtr<FJsonObject> WidgetJson = MakeShared<FJsonObject>();
    UObject* DefaultWidget = Widget->GetClass()->GetDefaultObject();

    WidgetJson->SetStringField(TEXT("widget_name"), Widget->GetName());
    WidgetJson->SetStringField(TEXT("widget_class"), Widget->GetClass()->GetPathName());
    WidgetJson->SetStringField(TEXT("widget_id"), Widget->GetPathName());

    TSharedPtr<FJsonObject> PropertiesJson = MakeShared<FJsonObject>();
    
    for (TFieldIterator<FProperty> PropIt(Widget->GetClass()); PropIt; ++PropIt)
    {
        FProperty* Property = *PropIt;
        bool bIsEditorOnly = false;
#if WITH_EDITOR
        bIsEditorOnly = Property->HasAnyPropertyFlags(CPF_EditorOnly);
#endif

        if (Property->HasAnyPropertyFlags(CPF_Edit) && !Property->HasAnyPropertyFlags(CPF_Transient) && !bIsEditorOnly)
        {
            void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Widget);
            void* DefaultValuePtr = Property->ContainerPtrToValuePtr<void>(DefaultWidget);

            if (!Property->Identical(ValuePtr, DefaultValuePtr))
            {
                if (Property->GetFName() == TEXT("Slot"))
                {
                    if (UPanelSlot* SlotObject = Cast<UPanelSlot>(CastField<FObjectProperty>(Property)->GetObjectPropertyValue_InContainer(Widget)))
                    {
                        TSharedPtr<FJsonObject> SlotPropertiesJson = MakeShared<FJsonObject>();
                        UObject* DefaultSlotObject = SlotObject->GetClass()->GetDefaultObject();

                        for (TFieldIterator<FProperty> SlotPropIt(SlotObject->GetClass()); SlotPropIt; ++SlotPropIt)
                        {
                            FProperty* SlotProperty = *SlotPropIt;
                            if ((SlotProperty->GetFName() != TEXT("Content")) && (SlotProperty->GetFName() != TEXT("Parent")) && SlotProperty->HasAnyPropertyFlags(CPF_Edit) && !SlotProperty->HasAnyPropertyFlags(CPF_Transient))
                            {
                                void* SlotValuePtr = SlotProperty->ContainerPtrToValuePtr<void>(SlotObject);
                                void* DefaultSlotValuePtr = SlotProperty->ContainerPtrToValuePtr<void>(DefaultSlotObject);
                                if (!SlotProperty->Identical(SlotValuePtr, DefaultSlotValuePtr))
                                {
                                    TSharedPtr<FJsonValue> SlotPropertyJsonValue = FJsonObjectConverter::UPropertyToJsonValue(SlotProperty, SlotValuePtr);
                                    if (SlotPropertyJsonValue.IsValid())
                                    {
                                        SlotPropertiesJson->SetField(SlotProperty->GetName(), SlotPropertyJsonValue);
                                    }
                                }
                            }
                        }
                        if(SlotPropertiesJson->Values.Num() > 0)
                        {
                           PropertiesJson->SetObjectField(TEXT("Slot"), SlotPropertiesJson);
                        }
                    }
                }
                else
                {
                    TSharedPtr<FJsonValue> PropertyJsonValue = FJsonObjectConverter::UPropertyToJsonValue(Property, ValuePtr);
                    if (PropertyJsonValue.IsValid())
                    {
                        PropertiesJson->SetField(Property->GetName(), PropertyJsonValue);
                    }
                }
            }
        }
    }
    
    if (PropertiesJson->Values.Num() > 0)
    {
        WidgetJson->SetObjectField(TEXT("properties"), PropertiesJson);
    }

    TArray<TSharedPtr<FJsonValue>> ChildrenJsonArray;
    if (UPanelWidget* PanelWidget = Cast<UPanelWidget>(Widget))
    {
        for (int32 i = 0; i < PanelWidget->GetChildrenCount(); ++i)
        {
            if (UWidget* ChildWidget = PanelWidget->GetChildAt(i))
            {
                TSharedPtr<FJsonObject> ChildJson = ExportWidgetToJson(ChildWidget);
                if (ChildJson.IsValid())
                {
                    ChildrenJsonArray.Add(MakeShared<FJsonValueObject>(ChildJson));
                }
            }
        }
    }
    
    if (ChildrenJsonArray.Num() > 0)
    {
        WidgetJson->SetArrayField(TEXT("children"), ChildrenJsonArray);
    }

    return WidgetJson;
}


// --- Subsystem Implementation ---

void UUmgGetSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	UE_LOG(LogUmgGet, Warning, TEXT("UmgGetSubsystem Initialized."));
}

void UUmgGetSubsystem::Deinitialize()
{
	UE_LOG(LogUmgGet, Log, TEXT("UmgGetSubsystem Deinitialized."));
	Super::Deinitialize();
}

FString UUmgGetSubsystem::GetWidgetTree()
{
    FString ErrorMessage;
    UWidgetBlueprint* WidgetBlueprint = GetCachedWidgetBlueprint(ErrorMessage);
    if (!WidgetBlueprint)
    {
        UE_LOG(LogUmgGet, Error, TEXT("GetWidgetTree: %s"), *ErrorMessage);
        return FString();
    }
    
    if (!WidgetBlueprint->WidgetTree)
    {
        UE_LOG(LogUmgGet, Error, TEXT("GetWidgetTree: WidgetTree is null in UWidgetBlueprint '%s'."), *WidgetBlueprint->GetPathName());
        return FString();
    }

    UWidget* RootWidget = WidgetBlueprint->WidgetTree->RootWidget;
    if (!RootWidget)
    {
        UE_LOG(LogUmgGet, Warning, TEXT("GetWidgetTree: Root widget not found in UWidgetBlueprint '%s'."), *WidgetBlueprint->GetPathName());
        return TEXT("{}");
    }

    TSharedPtr<FJsonObject> RootJsonObject = ExportWidgetToJson(RootWidget);
    if (!RootJsonObject.IsValid())
    {
        UE_LOG(LogUmgGet, Error, TEXT("GetWidgetTree: Failed to convert root widget of '%s' to FJsonObject."), *WidgetBlueprint->GetPathName());
        return FString();
    }

    FString JsonString;
    TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> JsonWriter = TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&JsonString);
    if (FJsonSerializer::Serialize(RootJsonObject.ToSharedRef(), JsonWriter))
    {
        JsonWriter->Close();
        return JsonString;
    }
    
    JsonWriter->Close();
    UE_LOG(LogUmgGet, Error, TEXT("GetWidgetTree: Failed to serialize FJsonObject to string for asset '%s'."), *WidgetBlueprint->GetPathName());
    return FString();
}

FString UUmgGetSubsystem::QueryWidgetProperties(const FString& WidgetName, const TArray<FString>& Properties)
{
    FString ErrorMessage;
    UWidgetBlueprint* WidgetBlueprint = GetCachedWidgetBlueprint(ErrorMessage);
    if (!WidgetBlueprint)
    {
        UE_LOG(LogUmgGet, Error, TEXT("QueryWidgetProperties: %s"), *ErrorMessage);
        return FString();
    }

    if (!WidgetBlueprint->WidgetTree)
    {
        UE_LOG(LogUmgGet, Error, TEXT("QueryWidgetProperties: WidgetTree is null for asset '%s'."), *WidgetBlueprint->GetPathName());
        return FString();
    }

    UWidget* FoundWidget = WidgetBlueprint->WidgetTree->FindWidget(FName(*WidgetName));
    if (!FoundWidget)
    {
        UE_LOG(LogUmgGet, Error, TEXT("QueryWidgetProperties: Failed to find widget '%s' in asset '%s'."), *WidgetName, *WidgetBlueprint->GetPathName());
        return FString();
    }

    TSharedPtr<FJsonObject> PropertiesJson = MakeShared<FJsonObject>();
    for (const FString& PropName : Properties)
    {
        FProperty* Property = FindFProperty<FProperty>(FoundWidget->GetClass(), FName(*PropName));
        if (Property)
        {
            void* ValuePtr = Property->ContainerPtrToValuePtr<void>(FoundWidget);
            TSharedPtr<FJsonValue> PropertyJsonValue = FJsonObjectConverter::UPropertyToJsonValue(Property, ValuePtr);
            if (PropertyJsonValue.IsValid())
            {
                PropertiesJson->SetField(Property->GetName(), PropertyJsonValue);
            }
        }
    }

    FString JsonString;
    TSharedRef<TJsonWriter<>> JsonWriter = TJsonWriterFactory<>::Create(&JsonString);
    FJsonSerializer::Serialize(PropertiesJson.ToSharedRef(), JsonWriter);
    return JsonString;
}

FString UUmgGetSubsystem::GetLayoutData(int32 ResolutionWidth, int32 ResolutionHeight)
{
    FString ErrorMessage;
    UWidgetBlueprint* WidgetBlueprint = GetCachedWidgetBlueprint(ErrorMessage);
    if (!WidgetBlueprint)
    {
        UE_LOG(LogUmgGet, Error, TEXT("GetLayoutData: %s"), *ErrorMessage);
        return FString();
    }

    if (!WidgetBlueprint->WidgetTree)
    {
        UE_LOG(LogUmgGet, Error, TEXT("GetLayoutData: WidgetTree is null for asset '%s'."), *WidgetBlueprint->GetPathName());
        return FString();
    }

    TArray<UWidget*> AllWidgets;
    WidgetBlueprint->WidgetTree->GetAllWidgets(AllWidgets);

    TArray<TSharedPtr<FJsonValue>> LayoutDataArray;

    for (UWidget* Widget : AllWidgets)
    {
        if (Widget && Widget->GetCachedWidget().IsValid())
        {
            FGeometry WidgetGeometry = Widget->GetCachedWidget()->GetTickSpaceGeometry();
            FSlateRect BoundingBox = WidgetGeometry.GetLayoutBoundingRect();

            TSharedPtr<FJsonObject> WidgetLayoutJson = MakeShared<FJsonObject>();
            WidgetLayoutJson->SetStringField(TEXT("widget_id"), Widget->GetPathName());
            WidgetLayoutJson->SetNumberField(TEXT("left"), BoundingBox.Left);
            WidgetLayoutJson->SetNumberField(TEXT("top"), BoundingBox.Top);
            WidgetLayoutJson->SetNumberField(TEXT("right"), BoundingBox.Right);
            WidgetLayoutJson->SetNumberField(TEXT("bottom"), BoundingBox.Bottom);

            LayoutDataArray.Add(MakeShared<FJsonValueObject>(WidgetLayoutJson));
        }
    }

    FString JsonString;
    TSharedRef<TJsonWriter<>> JsonWriter = TJsonWriterFactory<>::Create(&JsonString);
    FJsonSerializer::Serialize(LayoutDataArray, JsonWriter);
    return JsonString;
}

bool UUmgGetSubsystem::CheckWidgetOverlap(const TArray<FString>& WidgetIds)
{
    FString ErrorMessage;
    UWidgetBlueprint* WidgetBlueprint = GetCachedWidgetBlueprint(ErrorMessage);
    if (!WidgetBlueprint)
    {
        UE_LOG(LogUmgGet, Error, TEXT("CheckWidgetOverlap: %s"), *ErrorMessage);
        return false;
    }

    if (!WidgetBlueprint->WidgetTree)
    {
        UE_LOG(LogUmgGet, Error, TEXT("CheckWidgetOverlap: WidgetTree is null for asset '%s'."), *WidgetBlueprint->GetPathName());
        return false;
    }

    TArray<UWidget*> AllWidgets;
    WidgetBlueprint->WidgetTree->GetAllWidgets(AllWidgets);

    TArray<FSlateRect> BoundingBoxes;
    for (UWidget* Widget : AllWidgets)
    {
        if (Widget && Widget->GetCachedWidget().IsValid())
        {
            BoundingBoxes.Add(Widget->GetCachedWidget()->GetTickSpaceGeometry().GetLayoutBoundingRect());
        }
    }

    for (int32 i = 0; i < BoundingBoxes.Num(); ++i)
    {
        for (int32 j = i + 1; j < BoundingBoxes.Num(); ++j)
        {
            if (FSlateRect::DoRectanglesIntersect(BoundingBoxes[i], BoundingBoxes[j]))
            {
                UE_LOG(LogUmgGet, Warning, TEXT("CheckWidgetOverlap: Overlap detected in %s."), *WidgetBlueprint->GetPathName());
                return true; // Found an overlap
            }
        }
    }

    return false; // No overlaps found
}

FString UUmgGetSubsystem::GetAssetFileSystemPath(const FString& AssetPath)
{
    if (AssetPath.IsEmpty())
    {
        UE_LOG(LogUmgGet, Warning, TEXT("GetAssetFileSystemPath called with empty AssetPath."));
        return FString();
    }

    FString FileSystemPath;
    if (FPackageName::TryConvertLongPackageNameToFilename(AssetPath, FileSystemPath, FPackageName::GetAssetPackageExtension()))
    {
        UE_LOG(LogUmgGet, Log, TEXT("Converted AssetPath '%s' to FileSystemPath '%s'"), *AssetPath, *FileSystemPath);
        return FileSystemPath;
    }
    else
    {
        UE_LOG(LogUmgGet, Error, TEXT("Failed to convert AssetPath '%s' to FileSystemPath."), *AssetPath);
        return FString();
    }
}