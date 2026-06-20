// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#include "Storybook/UmgMcpStorybookCommands.h"

#include "Bridge/UmgMcpCommonUtils.h"
#include "Editor.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Storybook/UmgStorybookSubsystem.h"
#include "WidgetBlueprint.h"

int32 FUmgMcpStorybookCommands::ResolveViewportDimension(
	const TSharedPtr<FJsonObject>& Params,
	const TCHAR* PrimaryKey,
	const TCHAR* AlternateKey,
	int32 DefaultValue)
{
	if (!Params.IsValid())
	{
		return DefaultValue;
	}

	double NumericValue = 0.0;
	if (Params->TryGetNumberField(PrimaryKey, NumericValue))
	{
		return static_cast<int32>(NumericValue);
	}
	if (Params->TryGetNumberField(AlternateKey, NumericValue))
	{
		return static_cast<int32>(NumericValue);
	}
	return DefaultValue;
}

TSharedPtr<FJsonObject> FUmgMcpStorybookCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();

	if (!GEditor)
	{
		Response->SetBoolField(TEXT("success"), false);
		Response->SetStringField(TEXT("error"), TEXT("GEditor not available."));
		return Response;
	}

	FString ErrorMessage;
	UWidgetBlueprint* TargetBlueprint = FUmgMcpCommonUtils::GetTargetWidgetBlueprint(Params, ErrorMessage);
	if (!TargetBlueprint)
	{
		Response->SetBoolField(TEXT("success"), false);
		Response->SetStringField(TEXT("error"), ErrorMessage);
		return Response;
	}

	UUmgStorybookSubsystem* StorybookSubsystem = GEditor->GetEditorSubsystem<UUmgStorybookSubsystem>();
	if (!StorybookSubsystem)
	{
		Response->SetBoolField(TEXT("success"), false);
		Response->SetStringField(TEXT("error"), TEXT("UmgStorybookSubsystem is unavailable."));
		return Response;
	}

	auto DeserializeSubsystemJson = [&Response](const FString& JsonString) -> bool
	{
		TSharedPtr<FJsonObject> ParsedObject;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
		if (!FJsonSerializer::Deserialize(Reader, ParsedObject) || !ParsedObject.IsValid())
		{
			Response->SetBoolField(TEXT("success"), false);
			Response->SetStringField(TEXT("error"), TEXT("Failed to parse storybook subsystem response."));
			return false;
		}

		for (const auto& Field : ParsedObject->Values)
		{
			Response->SetField(Field.Key.ToView(), Field.Value);
		}
		return true;
	};

	if (CommandType == TEXT("render_widget_preview"))
	{
		FString WidgetName;
		// Only render a subtree when widget_name is explicitly provided in the request.
		// Do not inherit Active Widget scope — that breaks full-screen AI screenshots.
		Params->TryGetStringField(TEXT("widget_name"), WidgetName);

		FString Theme;
		Params->TryGetStringField(TEXT("theme"), Theme);

		const int32 ViewportWidth = ResolveViewportDimension(Params, TEXT("viewport_w"), TEXT("viewport_width"), 400);
		const int32 ViewportHeight = ResolveViewportDimension(Params, TEXT("viewport_h"), TEXT("viewport_height"), 300);
		const int32 ResolvedWidth = ResolveViewportDimension(Params, TEXT("width"), TEXT("width"), ViewportWidth);
		const int32 ResolvedHeight = ResolveViewportDimension(Params, TEXT("height"), TEXT("height"), ViewportHeight);

		const FString ResultJson = StorybookSubsystem->RenderWidgetPreview(
			TargetBlueprint,
			WidgetName,
			ResolvedWidth,
			ResolvedHeight,
			Theme);
		DeserializeSubsystemJson(ResultJson);
	}
	else if (CommandType == TEXT("storybook_list_variants"))
	{
		FString ParentWidgetName;
		FString CatalogComponentId;
		Params->TryGetStringField(TEXT("parent_widget"), ParentWidgetName);
		Params->TryGetStringField(TEXT("catalog_component_id"), CatalogComponentId);
		if (CatalogComponentId.IsEmpty())
		{
			Params->TryGetStringField(TEXT("component_id"), CatalogComponentId);
		}

		const FString ResultJson = StorybookSubsystem->ListVariants(TargetBlueprint, ParentWidgetName, CatalogComponentId);
		DeserializeSubsystemJson(ResultJson);
	}
	else if (CommandType == TEXT("storybook_render"))
	{
		TArray<FString> WidgetNames;
		const TArray<TSharedPtr<FJsonValue>>* WidgetNamesArray = nullptr;
		if (Params->TryGetArrayField(TEXT("widget_names"), WidgetNamesArray))
		{
			for (const TSharedPtr<FJsonValue>& Value : *WidgetNamesArray)
			{
				FString WidgetName = Value->AsString();
				if (!WidgetName.IsEmpty())
				{
					WidgetNames.Add(WidgetName);
				}
			}
		}

		FString Theme;
		Params->TryGetStringField(TEXT("theme"), Theme);

		const int32 ViewportWidth = ResolveViewportDimension(Params, TEXT("viewport_w"), TEXT("viewport_width"), 400);
		const int32 ViewportHeight = ResolveViewportDimension(Params, TEXT("viewport_h"), TEXT("viewport_height"), 300);

		const FString ResultJson = StorybookSubsystem->RenderVariants(
			TargetBlueprint,
			WidgetNames,
			ViewportWidth,
			ViewportHeight,
			Theme);
		DeserializeSubsystemJson(ResultJson);
	}
	else
	{
		Response->SetBoolField(TEXT("success"), false);
		Response->SetStringField(TEXT("error"), FString::Printf(TEXT("Unknown storybook command: %s"), *CommandType));
	}

	return Response;
}
