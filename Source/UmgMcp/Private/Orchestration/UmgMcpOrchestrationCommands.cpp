// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#include "Orchestration/UmgMcpOrchestrationCommands.h"

#include "Bridge/UmgMcpCommonUtils.h"
#include "Catalog/UmgCatalogSubsystem.h"
#include "Dom/JsonObject.h"
#include "Editor.h"
#include "FileManage/UmgAttentionSubsystem.h"
#include "Patch/UmgPatchSubsystem.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Theme/UmgThemeSubsystem.h"
#include "WidgetBlueprint.h"

namespace UmgOrchestrationInternal
{
	static bool MergeJsonResponse(const FString& JsonString, TSharedPtr<FJsonObject>& OutResponse)
	{
		TSharedPtr<FJsonObject> Parsed;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
		if (!FJsonSerializer::Deserialize(Reader, Parsed) || !Parsed.IsValid())
		{
			return false;
		}

		for (const TPair<FString, TSharedPtr<FJsonValue>>& Field : Parsed->Values)
		{
			OutResponse->SetField(Field.Key, Field.Value);
		}
		return true;
	}
}

TSharedPtr<FJsonObject> FUmgMcpOrchestrationCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
	if (!GEditor)
	{
		return FUmgMcpCommonUtils::CreateErrorResponse(TEXT("Editor unavailable."));
	}

	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();

	if (CommandType == TEXT("theme_get"))
	{
		UUmgThemeSubsystem* Theme = GEditor->GetEditorSubsystem<UUmgThemeSubsystem>();
		if (!Theme)
		{
			return FUmgMcpCommonUtils::CreateErrorResponse(TEXT("UmgThemeSubsystem unavailable."));
		}

		FString OptionalPath;
		if (Params.IsValid())
		{
			Params->TryGetStringField(TEXT("theme_path"), OptionalPath);
		}
		Theme->ReloadThemeCache(OptionalPath);

		TSharedPtr<FJsonObject> ThemeObject;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Theme->GetThemeJsonString());
		FJsonSerializer::Deserialize(Reader, ThemeObject);

		Response->SetBoolField(TEXT("success"), true);
		Response->SetStringField(TEXT("theme_path"), Theme->GetActiveThemeAssetPath());
		Response->SetObjectField(TEXT("theme"), ThemeObject.IsValid() ? ThemeObject : MakeShared<FJsonObject>());
		return Response;
	}

	if (CommandType == TEXT("theme_apply"))
	{
		UUmgThemeSubsystem* Theme = GEditor->GetEditorSubsystem<UUmgThemeSubsystem>();
		if (!Theme)
		{
			return FUmgMcpCommonUtils::CreateErrorResponse(TEXT("UmgThemeSubsystem unavailable."));
		}

		const TSharedPtr<FJsonObject>* PatchObject = nullptr;
		if (!Params.IsValid() || !Params->TryGetObjectField(TEXT("patch"), PatchObject) || !PatchObject || !(*PatchObject).IsValid())
		{
			return FUmgMcpCommonUtils::CreateErrorResponse(TEXT("Missing 'patch' object."));
		}

		FString OptionalPath;
		Params->TryGetStringField(TEXT("theme_path"), OptionalPath);

		const bool bOk = Theme->ApplyThemePatch(*PatchObject, OptionalPath);
		Response->SetBoolField(TEXT("success"), bOk);
		Response->SetStringField(TEXT("theme_path"), Theme->GetActiveThemeAssetPath());
		if (bOk)
		{
			TSharedPtr<FJsonObject> ThemeObject;
			const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Theme->GetThemeJsonString());
			FJsonSerializer::Deserialize(Reader, ThemeObject);
			Response->SetObjectField(TEXT("theme"), ThemeObject.IsValid() ? ThemeObject : MakeShared<FJsonObject>());
		}
		else
		{
			Response->SetStringField(TEXT("error"), TEXT("Failed to apply theme patch."));
		}
		return Response;
	}

	if (CommandType == TEXT("theme_resolve_token"))
	{
		UUmgThemeSubsystem* Theme = GEditor->GetEditorSubsystem<UUmgThemeSubsystem>();
		if (!Theme)
		{
			return FUmgMcpCommonUtils::CreateErrorResponse(TEXT("UmgThemeSubsystem unavailable."));
		}

		FString Token;
		if (!Params.IsValid() || !Params->TryGetStringField(TEXT("token"), Token))
		{
			return FUmgMcpCommonUtils::CreateErrorResponse(TEXT("Missing 'token' parameter."));
		}

		const FString Resolved = Theme->ResolveToken(Token);
		Response->SetBoolField(TEXT("success"), !Resolved.IsEmpty());
		Response->SetStringField(TEXT("token"), Token);
		Response->SetStringField(TEXT("resolved"), Resolved);
		return Response;
	}

	if (CommandType == TEXT("catalog_list"))
	{
		UUmgCatalogSubsystem* Catalog = GEditor->GetEditorSubsystem<UUmgCatalogSubsystem>();
		if (!Catalog)
		{
			return FUmgMcpCommonUtils::CreateErrorResponse(TEXT("UmgCatalogSubsystem unavailable."));
		}

		FString Root = TEXT("/Game/UI");
		bool bRecursive = true;
		if (Params.IsValid())
		{
			Params->TryGetStringField(TEXT("root"), Root);
			Params->TryGetBoolField(TEXT("recursive"), bRecursive);
		}

		Response->SetBoolField(TEXT("success"), true);
		UmgOrchestrationInternal::MergeJsonResponse(Catalog->ListComponents(Root, bRecursive), Response);
		return Response;
	}

	if (CommandType == TEXT("catalog_describe"))
	{
		UUmgCatalogSubsystem* Catalog = GEditor->GetEditorSubsystem<UUmgCatalogSubsystem>();
		if (!Catalog)
		{
			return FUmgMcpCommonUtils::CreateErrorResponse(TEXT("UmgCatalogSubsystem unavailable."));
		}

		FString Path;
		if (!Params.IsValid() || !Params->TryGetStringField(TEXT("path"), Path))
		{
			return FUmgMcpCommonUtils::CreateErrorResponse(TEXT("Missing 'path' parameter."));
		}

		UmgOrchestrationInternal::MergeJsonResponse(Catalog->DescribeComponent(Path), Response);
		return Response;
	}

	if (CommandType == TEXT("get_patch_revision"))
	{
		UUmgAttentionSubsystem* Attention = GEditor->GetEditorSubsystem<UUmgAttentionSubsystem>();
		if (!Attention)
		{
			return FUmgMcpCommonUtils::CreateErrorResponse(TEXT("UmgAttentionSubsystem unavailable."));
		}

		Response->SetBoolField(TEXT("success"), true);
		Response->SetNumberField(TEXT("revision"), Attention->GetPatchRevision());
		return Response;
	}

	if (CommandType == TEXT("patch_apply"))
	{
		UUmgPatchSubsystem* PatchSystem = GEditor->GetEditorSubsystem<UUmgPatchSubsystem>();
		UUmgAttentionSubsystem* Attention = GEditor->GetEditorSubsystem<UUmgAttentionSubsystem>();
		if (!PatchSystem || !Attention)
		{
			return FUmgMcpCommonUtils::CreateErrorResponse(TEXT("Patch or Attention subsystem unavailable."));
		}

		const TArray<TSharedPtr<FJsonValue>>* PatchArray = nullptr;
		if (!Params.IsValid() || !Params->TryGetArrayField(TEXT("patch"), PatchArray))
		{
			return FUmgMcpCommonUtils::CreateErrorResponse(TEXT("Missing 'patch' array."));
		}

		TOptional<int32> ExpectedRevision;
		double ExpectedRevisionNumber = 0.0;
		if (Params->TryGetNumberField(TEXT("expected_revision"), ExpectedRevisionNumber))
		{
			ExpectedRevision = static_cast<int32>(ExpectedRevisionNumber);
		}

		UWidgetBlueprint* TargetBP = Attention->GetCachedTargetWidgetBlueprint();
		UmgOrchestrationInternal::MergeJsonResponse(PatchSystem->ApplyPatch(TargetBP, *PatchArray, ExpectedRevision), Response);
		return Response;
	}

	return FUmgMcpCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown orchestration command: %s"), *CommandType));
}
