// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#include "Catalog/UmgCatalogSubsystem.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Blueprint/WidgetTree.h"
#include "Components/NamedSlot.h"
#include "Components/PanelWidget.h"
#include "EditorAssetLibrary.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Misc/PackageName.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "WidgetBlueprint.h"

DEFINE_LOG_CATEGORY(LogUmgCatalog);

void UUmgCatalogSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	UE_LOG(LogUmgCatalog, Log, TEXT("UmgCatalogSubsystem initialized."));
}

FString UUmgCatalogSubsystem::ResolveWidgetBlueprintPath(const FString& AssetPath) const
{
	if (AssetPath.IsEmpty())
	{
		return FString();
	}

	FString Normalized = AssetPath;
	Normalized.ReplaceInline(TEXT("\\"), TEXT("/"));

	if (Normalized.EndsWith(TEXT("_C")))
	{
		return Normalized;
	}

	if (Normalized.Contains(TEXT(".")))
	{
		const FString PackagePath = FPaths::GetPath(Normalized);
		const FString AssetName = FPaths::GetBaseFilename(Normalized);
		return FString::Printf(TEXT("%s.%s_C"), *PackagePath, *AssetName);
	}

	const FString AssetName = FPaths::GetBaseFilename(Normalized);
	return FString::Printf(TEXT("%s.%s_C"), *Normalized, *AssetName);
}

FString UUmgCatalogSubsystem::ListComponents(const FString& RootPath, bool bRecursive) const
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	FARFilter Filter;
	Filter.ClassPaths.Add(UWidgetBlueprint::StaticClass()->GetClassPathName());
	Filter.bRecursivePaths = bRecursive;

	FString PackageRoot = RootPath;
	PackageRoot.ReplaceInline(TEXT("\\"), TEXT("/"));
	if (!PackageRoot.StartsWith(TEXT("/")))
	{
		PackageRoot = TEXT("/Game/") + PackageRoot;
	}
	Filter.PackagePaths.Add(FName(*PackageRoot));

	TArray<FAssetData> Assets;
	AssetRegistry.GetAssets(Filter, Assets);

	TArray<TSharedPtr<FJsonValue>> CatalogArray;
	for (const FAssetData& Asset : Assets)
	{
		const FString AssetName = Asset.AssetName.ToString();
		const FString CategoryTag = Asset.GetTagValueRef<FString>(TEXT("UmgCatalogCategory"));
		const bool bIsPrefixed = AssetName.StartsWith(TEXT("WBP_")) || AssetName.StartsWith(TEXT("UW_"));
		const bool bIsTagged = !CategoryTag.IsEmpty();
		if (!bIsPrefixed && !bIsTagged)
		{
			continue;
		}

		FString Description = Asset.GetTagValueRef<FString>(TEXT("BlueprintDescription"));
		if (Description.IsEmpty())
		{
			Description = TEXT("No description");
		}

		FString CategoryValue = CategoryTag;
		if (CategoryValue.IsEmpty())
		{
			CategoryValue = TEXT("component");
		}

		const FString PackageName = Asset.PackageName.ToString();
		const FString ClassPath = FString::Printf(TEXT("%s.%s_C"), *PackageName, *AssetName);

		TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
		Entry->SetStringField(TEXT("name"), AssetName);
		Entry->SetStringField(TEXT("path"), ClassPath);
		Entry->SetStringField(TEXT("package"), PackageName);
		Entry->SetStringField(TEXT("description"), Description);
		Entry->SetStringField(TEXT("category"), CategoryValue);
		CatalogArray.Add(MakeShared<FJsonValueObject>(Entry));
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("root"), PackageRoot);
	Result->SetNumberField(TEXT("count"), CatalogArray.Num());
	Result->SetArrayField(TEXT("components"), CatalogArray);

	FString Out;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
	FJsonSerializer::Serialize(Result.ToSharedRef(), Writer);
	return Out;
}

FString UUmgCatalogSubsystem::DescribeComponent(const FString& AssetPath) const
{
	const FString ClassPath = ResolveWidgetBlueprintPath(AssetPath);

	FString ObjectPath = AssetPath;
	ObjectPath.ReplaceInline(TEXT("\\"), TEXT("/"));
	if (!ObjectPath.Contains(TEXT(".")))
	{
		ObjectPath = FString::Printf(TEXT("%s.%s"), *ObjectPath, *FPaths::GetBaseFilename(ObjectPath));
	}

	UWidgetBlueprint* WidgetBP = LoadObject<UWidgetBlueprint>(nullptr, *ObjectPath);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("path"), ClassPath);

	if (!WidgetBP)
	{
		Result->SetBoolField(TEXT("success"), false);
		Result->SetStringField(TEXT("error"), FString::Printf(TEXT("Failed to load WidgetBlueprint: %s"), *AssetPath));
	}
	else
	{
		Result->SetBoolField(TEXT("success"), true);
		Result->SetStringField(TEXT("name"), WidgetBP->GetName());

		TArray<TSharedPtr<FJsonValue>> SlotsArray;
		TArray<TSharedPtr<FJsonValue>> ParamsArray;

		if (WidgetBP->WidgetTree)
		{
			TArray<UWidget*> AllWidgets;
			WidgetBP->WidgetTree->GetAllWidgets(AllWidgets);
			for (UWidget* Widget : AllWidgets)
			{
				if (Cast<UNamedSlot>(Widget))
				{
					SlotsArray.Add(MakeShared<FJsonValueString>(Widget->GetName()));
				}
			}
		}

		for (const FBPVariableDescription& Variable : WidgetBP->NewVariables)
		{
			if (Variable.PropertyFlags & CPF_ExposeOnSpawn)
			{
				TSharedPtr<FJsonObject> ParamObj = MakeShared<FJsonObject>();
				ParamObj->SetStringField(TEXT("name"), Variable.VarName.ToString());
				ParamObj->SetStringField(TEXT("type"), Variable.VarType.PinCategory.ToString());
				ParamsArray.Add(MakeShared<FJsonValueObject>(ParamObj));
			}
		}

		Result->SetArrayField(TEXT("slots"), SlotsArray);
		Result->SetArrayField(TEXT("params"), ParamsArray);
	}

	FString Out;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
	FJsonSerializer::Serialize(Result.ToSharedRef(), Writer);
	return Out;
}
