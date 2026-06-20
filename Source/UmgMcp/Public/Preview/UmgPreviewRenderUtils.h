// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#pragma once

#include "CoreMinimal.h"

class SVirtualWindow;
class UUserWidget;
class UWidgetBlueprint;
class UWorld;

/**
 * Hosts a Slate tree inside SVirtualWindow so off-screen FWidgetRenderer gets valid geometry
 * without opening the UMG Designer or attaching to a viewport.
 */
class UMGMCP_API FUmgPreviewRenderUtils
{
public:
	/** Recompile so GeneratedClass matches the latest WidgetTree (required after MCP layout edits). */
	static void CompileWidgetBlueprint(UWidgetBlueprint* WidgetBlueprint);

	/**
	 * Create a transient preview instance and build Slate for UserWidget + all tree widgets.
	 * Caller must destroy the returned widget with ConditionalBeginDestroy().
	 */
	static UUserWidget* CreatePreviewInstance(UWidgetBlueprint* WidgetBlueprint, UWorld* World, FString& OutError);

	/** Wrap content in a virtual window, run SlatePrepass, optionally register with SlateApplication. */
	static TSharedRef<SWidget> PrepareOffscreenRenderTree(
		TSharedRef<SWidget> Content,
		const FVector2D& DrawSize,
		TSharedPtr<SVirtualWindow>& OutVirtualWindow);

	/** Unregister and release a virtual window created by PrepareOffscreenRenderTree. */
	static void ReleaseOffscreenRenderHost(TSharedPtr<SVirtualWindow>& VirtualWindow);
};
