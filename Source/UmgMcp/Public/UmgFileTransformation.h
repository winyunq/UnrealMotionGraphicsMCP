// UmgFileTransformation.h
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "UmgFileTransformation.generated.h"

class UWidget;
class FJsonObject;

UCLASS()
class UMGMCP_API UUmgFileTransformation : public UObject
{
    GENERATED_BODY()

public:
    /**
     * Recursively exports a UMG widget and its children to a JSON object.
     * This is a helper function for internal use.
     * @param Widget The UMG widget to export.
     * @return A shared pointer to the FJsonObject representing the widget hierarchy, or nullptr if invalid.
     */
    static TSharedPtr<FJsonObject> ExportWidgetToJson(UWidget* Widget);

    /**
     * Exports a UMG asset (UWidgetBlueprint) to a JSON string.
     * This function loads the asset, converts its widget tree to JSON, and returns the JSON string.
     * @param AssetPath The Unreal Engine asset path (e.g., "/Game/MyWidget.MyMyWidget").
     * @return The JSON string representation of the UMG asset, or an empty string if failed.
     */
    static FString ExportUmgAssetToJsonString(const FString& AssetPath);

    /**
     * Applies JSON data to a UMG asset, creating or modifying it.
     * This function will parse the JSON and reconstruct/update the UMG widget tree.
     * @param AssetPath The Unreal Engine asset path to create/modify.
     * @param JsonData The JSON string data to apply.
     * @return True if the operation was successful, false otherwise.
     */
    static bool ApplyJsonStringToUmgAsset(const FString& AssetPath, const FString& JsonData);
};