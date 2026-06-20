// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#include "Preview/UmgPreviewRenderUtils.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Widget.h"
#include "Engine/LocalPlayer.h"
#include "Framework/Application/SlateApplication.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Widgets/SVirtualWindow.h"
#include "WidgetBlueprint.h"

void FUmgPreviewRenderUtils::CompileWidgetBlueprint(UWidgetBlueprint* WidgetBlueprint)
{
	if (!WidgetBlueprint)
	{
		return;
	}

	FKismetEditorUtilities::CompileBlueprint(WidgetBlueprint);
}

UUserWidget* FUmgPreviewRenderUtils::CreatePreviewInstance(
	UWidgetBlueprint* WidgetBlueprint,
	UWorld* World,
	FString& OutError)
{
	OutError.Reset();

	if (!WidgetBlueprint)
	{
		OutError = TEXT("WidgetBlueprint is null.");
		return nullptr;
	}

	if (!WidgetBlueprint->GeneratedClass)
	{
		CompileWidgetBlueprint(WidgetBlueprint);
	}

	UClass* WidgetClass = WidgetBlueprint->GeneratedClass;
	if (!WidgetClass)
	{
		OutError = TEXT("Blueprint has no GeneratedClass. Compile the Widget Blueprint first.");
		return nullptr;
	}

	if (!World)
	{
		OutError = TEXT("Editor world is unavailable.");
		return nullptr;
	}

	if (!WidgetBlueprint->WidgetTree || !WidgetBlueprint->WidgetTree->RootWidget)
	{
		OutError = TEXT("Widget tree has no root widget.");
		return nullptr;
	}

	UUserWidget* PreviewWidget = NewObject<UUserWidget>(World, WidgetClass, NAME_None, RF_Transient);
	if (!PreviewWidget)
	{
		OutError = TEXT("Failed to create preview widget instance.");
		return nullptr;
	}

	PreviewWidget->ClearFlags(RF_Transactional);
	PreviewWidget->SetFlags(RF_Transient);
	PreviewWidget->SetDesignerFlags(EWidgetDesignFlags::Designing | EWidgetDesignFlags::ExecutePreConstruct);

	if (ULocalPlayer* LocalPlayer = World->GetFirstLocalPlayerFromController())
	{
		PreviewWidget->SetPlayerContext(FLocalPlayerContext(LocalPlayer));
	}

	PreviewWidget->Initialize();
	PreviewWidget->TakeWidget();
	return PreviewWidget;
}

TSharedRef<SWidget> FUmgPreviewRenderUtils::PrepareOffscreenRenderTree(
	TSharedRef<SWidget> Content,
	const FVector2D& DrawSize,
	TSharedPtr<SVirtualWindow>& OutVirtualWindow)
{
	OutVirtualWindow = SNew(SVirtualWindow).Size(DrawSize);
	OutVirtualWindow->SetContent(Content);
	OutVirtualWindow->SlatePrepass(1.0f);

	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().RegisterVirtualWindow(OutVirtualWindow.ToSharedRef());
	}

	return OutVirtualWindow.ToSharedRef();
}

void FUmgPreviewRenderUtils::ReleaseOffscreenRenderHost(TSharedPtr<SVirtualWindow>& VirtualWindow)
{
	if (VirtualWindow.IsValid() && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().UnregisterVirtualWindow(VirtualWindow.ToSharedRef());
	}

	VirtualWindow.Reset();
}
