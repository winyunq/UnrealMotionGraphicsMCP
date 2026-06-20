// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "UmgStorybookSubsystem.generated.h"

class UWidgetBlueprint;
class UTextureRenderTarget2D;

DECLARE_LOG_CATEGORY_EXTERN(LogUmgStorybook, Log, All);

/**
 * Isolated UMG widget preview rendering for AI visual feedback (Storybook).
 * Renders a widget subtree off-screen via FWidgetRenderer and returns PNG/base64.
 */
UCLASS()
class UMGMCP_API UUmgStorybookSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Render preview; returns JSON with image_base64, width, height, widget_path. */
	FString RenderWidgetPreview(
		UWidgetBlueprint* WidgetBlueprint,
		const FString& WidgetName,
		int32 ViewportWidth,
		int32 ViewportHeight,
		const FString& Theme);

	/** List child widget names suitable as storybook variants. */
	FString ListVariants(
		UWidgetBlueprint* WidgetBlueprint,
		const FString& ParentWidgetName,
		const FString& CatalogComponentId);

	/** Batch-render multiple widget variants; returns JSON array of preview results. */
	FString RenderVariants(
		UWidgetBlueprint* WidgetBlueprint,
		const TArray<FString>& WidgetNames,
		int32 ViewportWidth,
		int32 ViewportHeight,
		const FString& Theme);

private:
	bool EncodeRenderTargetToBase64Png(UTextureRenderTarget2D* RenderTarget, FString& OutBase64, FString& OutError) const;
	bool LoadCatalogVariants(const FString& CatalogComponentId, const FString& AssetPath, TArray<FString>& OutVariants, FString& OutError) const;
	FString GetCatalogPath() const;
};
