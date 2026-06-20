// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "UmgCatalogSubsystem.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogUmgCatalog, Log, All);

/**
 * Semantic component catalog above raw list_assets.
 * Scans WidgetBlueprint assets with tags and exposes spawn parameters.
 */
UCLASS()
class UMGMCP_API UUmgCatalogSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** List reusable WBP components under a content root. */
	FString ListComponents(const FString& RootPath, bool bRecursive = true) const;

	/** Describe slots and Expose-on-Spawn params for a WBP asset path. */
	FString DescribeComponent(const FString& AssetPath) const;

private:
	FString ResolveWidgetBlueprintPath(const FString& AssetPath) const;
};
