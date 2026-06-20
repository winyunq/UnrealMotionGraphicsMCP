// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#include "Storybook/UmgStorybookSubsystem.h"

#include "WidgetBlueprint.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/PanelWidget.h"
#include "Components/Widget.h"
#include "Editor.h"
#include "Engine/LocalPlayer.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/FileManager.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "ImageUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/Base64.h"
#include "Misc/FileHelper.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Preview/UmgPreviewRenderUtils.h"
#include "Slate/WidgetRenderer.h"
#include "Widgets/SVirtualWindow.h"
#include "TextureResource.h"

DEFINE_LOG_CATEGORY(LogUmgStorybook);

namespace UmgStorybookInternal
{
	static constexpr int32 MinViewportSize = 16;
	static constexpr int32 MaxViewportSize = 4096;

	static int32 ClampViewportSize(int32 Value, int32 DefaultValue)
	{
		if (Value <= 0)
		{
			return DefaultValue;
		}
		return FMath::Clamp(Value, MinViewportSize, MaxViewportSize);
	}

	static TSharedPtr<FJsonObject> MakeErrorObject(const FString& Message)
	{
		TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
		Result->SetBoolField(TEXT("success"), false);
		Result->SetStringField(TEXT("status"), TEXT("error"));
		Result->SetStringField(TEXT("error"), Message);
		return Result;
	}

	static FString SerializeJsonObject(const TSharedPtr<FJsonObject>& JsonObject)
	{
		FString Output;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
		FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);
		return Output;
	}

	static UWidget* FindWidgetByName(UUserWidget* UserWidget, const FString& WidgetName)
	{
		if (!UserWidget || WidgetName.IsEmpty())
		{
			return nullptr;
		}

		if (UWidgetTree* Tree = UserWidget->WidgetTree)
		{
			return Tree->FindWidget(FName(*WidgetName));
		}

		return nullptr;
	}

	static TSharedPtr<SWidget> ResolveRenderSlate(UUserWidget* UserWidget, const FString& WidgetName, FString& OutResolvedWidgetName, FString& OutError)
	{
		if (!UserWidget)
		{
			OutError = TEXT("Preview widget instance is null.");
			return nullptr;
		}

		UserWidget->SetDesignerFlags(EWidgetDesignFlags::Designing | EWidgetDesignFlags::ExecutePreConstruct);
		UserWidget->Initialize();

		if (UWorld* World = UserWidget->GetWorld())
		{
			if (ULocalPlayer* LocalPlayer = World->GetFirstLocalPlayerFromController())
			{
				UserWidget->SetPlayerContext(FLocalPlayerContext(LocalPlayer));
			}
		}

		TSharedRef<SWidget> SlateWidget = UserWidget->TakeWidget();

		if (!WidgetName.IsEmpty())
		{
			if (UWidget* TargetWidget = FindWidgetByName(UserWidget, WidgetName))
			{
				OutResolvedWidgetName = TargetWidget->GetName();
				if (TSharedPtr<SWidget> TargetSlate = TargetWidget->GetCachedWidget())
				{
					return TargetSlate;
				}

				// Off-screen CreateWidget often only exposes UserWidget-level Slate reliably.
				UE_LOG(LogUmgStorybook, Warning,
					TEXT("Preview: widget '%s' has no direct Slate cache; falling back to UserWidget root for screenshot."),
					*WidgetName);

				if (UWidget* RootWidget = UserWidget->WidgetTree ? UserWidget->WidgetTree->RootWidget : nullptr)
				{
					OutResolvedWidgetName = RootWidget->GetName();
				}

				if (TSharedPtr<SWidget> RootSlate = UserWidget->GetCachedWidget())
				{
					return RootSlate;
				}

				return SlateWidget;
			}

			OutError = FString::Printf(TEXT("Widget '%s' not found in preview instance."), *WidgetName);
			return nullptr;
		}

		if (UWidget* RootWidget = UserWidget->WidgetTree ? UserWidget->WidgetTree->RootWidget : nullptr)
		{
			OutResolvedWidgetName = RootWidget->GetName();
		}
		else
		{
			OutResolvedWidgetName = UserWidget->GetName();
		}

		if (TSharedPtr<SWidget> RootSlate = UserWidget->GetCachedWidget())
		{
			return RootSlate;
		}

		return SlateWidget;
	}
}

void UUmgStorybookSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	UE_LOG(LogUmgStorybook, Log, TEXT("UmgStorybookSubsystem initialized."));
}

void UUmgStorybookSubsystem::Deinitialize()
{
	UE_LOG(LogUmgStorybook, Log, TEXT("UmgStorybookSubsystem deinitialized."));
	Super::Deinitialize();
}

FString UUmgStorybookSubsystem::GetCatalogPath() const
{
	if (const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("UmgMcp")))
	{
		return FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/storybook_catalog.json"));
	}

	return FPaths::ConvertRelativePathToFull(
		FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("UnrealMotionGraphicsMCP-main/Resources/storybook_catalog.json")));
}

bool UUmgStorybookSubsystem::LoadCatalogVariants(const FString& CatalogComponentId, const FString& AssetPath, TArray<FString>& OutVariants, FString& OutError) const
{
	const FString CatalogPath = GetCatalogPath();
	if (!FPaths::FileExists(CatalogPath))
	{
		OutError = FString::Printf(TEXT("Catalog file not found: %s"), *CatalogPath);
		return false;
	}

	FString CatalogContent;
	if (!FFileHelper::LoadFileToString(CatalogContent, *CatalogPath))
	{
		OutError = FString::Printf(TEXT("Failed to read catalog file: %s"), *CatalogPath);
		return false;
	}

	TSharedPtr<FJsonObject> CatalogJson;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(CatalogContent);
	if (!FJsonSerializer::Deserialize(Reader, CatalogJson) || !CatalogJson.IsValid())
	{
		OutError = TEXT("Failed to parse storybook_catalog.json.");
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* ComponentsArray = nullptr;
	if (!CatalogJson->TryGetArrayField(TEXT("components"), ComponentsArray))
	{
		OutError = TEXT("storybook_catalog.json is missing 'components' array.");
		return false;
	}

	for (const TSharedPtr<FJsonValue>& ComponentValue : *ComponentsArray)
	{
		const TSharedPtr<FJsonObject>* ComponentObject = nullptr;
		if (!ComponentValue->TryGetObject(ComponentObject) || !ComponentObject || !ComponentObject->IsValid())
		{
			continue;
		}

		FString ComponentId;
		FString ComponentAssetPath;
		(*ComponentObject)->TryGetStringField(TEXT("id"), ComponentId);
		(*ComponentObject)->TryGetStringField(TEXT("asset_path"), ComponentAssetPath);

		const bool bIdMatch = !CatalogComponentId.IsEmpty() && ComponentId == CatalogComponentId;
		const bool bAssetMatch = CatalogComponentId.IsEmpty() && !AssetPath.IsEmpty() && ComponentAssetPath == AssetPath;
		if (!bIdMatch && !bAssetMatch)
		{
			continue;
		}

		const TArray<TSharedPtr<FJsonValue>>* VariantsArray = nullptr;
		if ((*ComponentObject)->TryGetArrayField(TEXT("variants"), VariantsArray))
		{
			for (const TSharedPtr<FJsonValue>& VariantValue : *VariantsArray)
			{
				FString VariantName;
				const TSharedPtr<FJsonObject>* VariantObject = nullptr;
				if (VariantValue->TryGetObject(VariantObject) && VariantObject && VariantObject->IsValid())
				{
					if ((*VariantObject)->TryGetStringField(TEXT("widget_name"), VariantName) && !VariantName.IsEmpty())
					{
						OutVariants.Add(VariantName);
					}
				}
				else if (VariantValue->TryGetString(VariantName) && !VariantName.IsEmpty())
				{
					OutVariants.Add(VariantName);
				}
			}
		}

		if (OutVariants.Num() > 0)
		{
			return true;
		}
	}

	OutError = CatalogComponentId.IsEmpty()
		? FString::Printf(TEXT("No catalog variants found for asset '%s'."), *AssetPath)
		: FString::Printf(TEXT("No catalog variants found for catalog component '%s'."), *CatalogComponentId);
	return false;
}

bool UUmgStorybookSubsystem::EncodeRenderTargetToBase64Png(UTextureRenderTarget2D* RenderTarget, FString& OutBase64, FString& OutError) const
{
	if (!RenderTarget)
	{
		OutError = TEXT("Render target is null.");
		return false;
	}

	FTextureRenderTargetResource* RenderTargetResource = RenderTarget->GameThread_GetRenderTargetResource();
	if (!RenderTargetResource)
	{
		OutError = TEXT("Render target resource is unavailable.");
		return false;
	}

	TArray<FColor> Pixels;
	if (!RenderTargetResource->ReadPixels(Pixels))
	{
		OutError = TEXT("Failed to read pixels from render target.");
		return false;
	}

	const int32 Width = RenderTarget->SizeX;
	const int32 Height = RenderTarget->SizeY;
	if (Width <= 0 || Height <= 0 || Pixels.Num() != Width * Height)
	{
		OutError = TEXT("Render target pixel buffer size mismatch.");
		return false;
	}

	TArray<uint8> RawData;
	RawData.SetNumUninitialized(Width * Height * 4);
	for (int32 Index = 0; Index < Pixels.Num(); ++Index)
	{
		const FColor& Color = Pixels[Index];
		RawData[Index * 4 + 0] = Color.R;
		RawData[Index * 4 + 1] = Color.G;
		RawData[Index * 4 + 2] = Color.B;
		RawData[Index * 4 + 3] = Color.A;
	}

	IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
	const TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);
	if (!ImageWrapper.IsValid() || !ImageWrapper->SetRaw(RawData.GetData(), RawData.Num(), Width, Height, ERGBFormat::RGBA, 8))
	{
		OutError = TEXT("Failed to initialize PNG encoder.");
		return false;
	}

	const TArray64<uint8>& CompressedData = ImageWrapper->GetCompressed(100);
	if (CompressedData.Num() <= 0)
	{
		OutError = TEXT("PNG compression returned empty data.");
		return false;
	}

	OutBase64 = FBase64::Encode(CompressedData.GetData(), CompressedData.Num());
	return true;
}

FString UUmgStorybookSubsystem::RenderWidgetPreview(
	UWidgetBlueprint* WidgetBlueprint,
	const FString& WidgetName,
	int32 ViewportWidth,
	int32 ViewportHeight,
	const FString& Theme)
{
	using namespace UmgStorybookInternal;

	if (!WidgetBlueprint)
	{
		return SerializeJsonObject(MakeErrorObject(TEXT("WidgetBlueprint is null.")));
	}

	ViewportWidth = ClampViewportSize(ViewportWidth, 400);
	ViewportHeight = ClampViewportSize(ViewportHeight, 300);

	FUmgPreviewRenderUtils::CompileWidgetBlueprint(WidgetBlueprint);

	UClass* WidgetClass = WidgetBlueprint->GeneratedClass;
	if (!WidgetClass)
	{
		return SerializeJsonObject(MakeErrorObject(TEXT("Blueprint has no GeneratedClass. Compile the Widget Blueprint first.")));
	}

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		return SerializeJsonObject(MakeErrorObject(TEXT("Editor world is unavailable.")));
	}

	if (!WidgetBlueprint->WidgetTree || !WidgetBlueprint->WidgetTree->RootWidget)
	{
		return SerializeJsonObject(MakeErrorObject(
			TEXT("Widget tree has no root widget. Add a panel root (e.g. VerticalBox or CanvasPanel) before preview.")));
	}

	UUserWidget* PreviewWidget = NewObject<UUserWidget>(World, WidgetClass, NAME_None, RF_Transient);
	if (!PreviewWidget)
	{
		return SerializeJsonObject(MakeErrorObject(TEXT("Failed to create preview widget instance.")));
	}

	PreviewWidget->ClearFlags(RF_Transactional);

	FString ResolvedWidgetName;
	FString ResolveError;
	TSharedPtr<SWidget> RenderSlate = ResolveRenderSlate(PreviewWidget, WidgetName, ResolvedWidgetName, ResolveError);
	if (!RenderSlate.IsValid())
	{
		PreviewWidget->ConditionalBeginDestroy();
		return SerializeJsonObject(MakeErrorObject(ResolveError));
	}

	const FVector2D DrawSize(static_cast<float>(ViewportWidth), static_cast<float>(ViewportHeight));

	TSharedPtr<SVirtualWindow> VirtualHost;
	const TSharedRef<SWidget> RenderTree = FUmgPreviewRenderUtils::PrepareOffscreenRenderTree(
		RenderSlate.ToSharedRef(),
		DrawSize,
		VirtualHost);

	UTextureRenderTarget2D* RenderTarget = NewObject<UTextureRenderTarget2D>(GetTransientPackage());
	RenderTarget->RenderTargetFormat = RTF_RGBA8;
	RenderTarget->ClearColor = FLinearColor(0.f, 0.f, 0.f, 0.f);
	RenderTarget->InitAutoFormat(ViewportWidth, ViewportHeight);
	RenderTarget->UpdateResourceImmediate(true);

	FWidgetRenderer WidgetRenderer(true);
	WidgetRenderer.DrawWidget(RenderTarget, RenderTree, DrawSize, 0.f, false);
	FlushRenderingCommands();

	FUmgPreviewRenderUtils::ReleaseOffscreenRenderHost(VirtualHost);

	FString Base64Png;
	FString EncodeError;
	if (!EncodeRenderTargetToBase64Png(RenderTarget, Base64Png, EncodeError))
	{
		PreviewWidget->ConditionalBeginDestroy();
		return SerializeJsonObject(MakeErrorObject(EncodeError));
	}

	const FString AssetPath = WidgetBlueprint->GetPathName();
	const FString WidgetPath = AssetPath + TEXT(".") + ResolvedWidgetName;

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("status"), TEXT("ok"));
	Result->SetStringField(TEXT("image_base64"), Base64Png);
	Result->SetNumberField(TEXT("width"), ViewportWidth);
	Result->SetNumberField(TEXT("height"), ViewportHeight);
	Result->SetStringField(TEXT("widget_path"), WidgetPath);
	Result->SetStringField(TEXT("asset_path"), AssetPath);
	Result->SetStringField(TEXT("widget_name"), ResolvedWidgetName);

	if (!Theme.IsEmpty())
	{
		Result->SetStringField(TEXT("theme"), Theme);
		Result->SetStringField(TEXT("theme_note"), TEXT("Theme application is not implemented yet; value returned as metadata only."));
	}

	Result->SetStringField(
		TEXT("runtime_note"),
		TEXT("Off-screen preview uses TakeWidget + SVirtualWindow + SlatePrepass; Designer does not need to be open. BindWidget/ViewModel/PIE-only logic may still render empty."));

	PreviewWidget->ConditionalBeginDestroy();
	return SerializeJsonObject(Result);
}

FString UUmgStorybookSubsystem::ListVariants(
	UWidgetBlueprint* WidgetBlueprint,
	const FString& ParentWidgetName,
	const FString& CatalogComponentId)
{
	using namespace UmgStorybookInternal;

	if (!WidgetBlueprint)
	{
		return SerializeJsonObject(MakeErrorObject(TEXT("WidgetBlueprint is null.")));
	}

	if (!WidgetBlueprint->WidgetTree)
	{
		return SerializeJsonObject(MakeErrorObject(TEXT("WidgetTree is null.")));
	}

	TArray<FString> VariantNames;
	FString CatalogError;
	const bool bCatalogRequested = !CatalogComponentId.IsEmpty();

	if (bCatalogRequested)
	{
		if (!LoadCatalogVariants(CatalogComponentId, WidgetBlueprint->GetPathName(), VariantNames, CatalogError))
		{
			return SerializeJsonObject(MakeErrorObject(CatalogError));
		}

		if (VariantNames.Num() == 0)
		{
			return SerializeJsonObject(MakeErrorObject(
				FString::Printf(TEXT("Catalog component '%s' matched but defines no variants."), *CatalogComponentId)));
		}
	}
	else if (LoadCatalogVariants(CatalogComponentId, WidgetBlueprint->GetPathName(), VariantNames, CatalogError))
	{
		// Optional asset-path catalog match succeeded.
	}
	else
	{
		VariantNames.Reset();
	}

	if (VariantNames.Num() == 0)
	{
		UWidget* ParentWidget = nullptr;
		if (!ParentWidgetName.IsEmpty())
		{
			ParentWidget = WidgetBlueprint->WidgetTree->FindWidget(FName(*ParentWidgetName));
			if (!ParentWidget)
			{
				return SerializeJsonObject(MakeErrorObject(FString::Printf(TEXT("Parent widget '%s' not found."), *ParentWidgetName)));
			}
		}
		else
		{
			ParentWidget = WidgetBlueprint->WidgetTree->RootWidget;
		}

		if (UPanelWidget* PanelWidget = Cast<UPanelWidget>(ParentWidget))
		{
			const int32 ChildCount = PanelWidget->GetChildrenCount();
			for (int32 ChildIndex = 0; ChildIndex < ChildCount; ++ChildIndex)
			{
				if (UWidget* ChildWidget = PanelWidget->GetChildAt(ChildIndex))
				{
					VariantNames.Add(ChildWidget->GetName());
				}
			}
		}
		else if (ParentWidget)
		{
			VariantNames.Add(ParentWidget->GetName());
		}
	}

	TArray<TSharedPtr<FJsonValue>> VariantJsonArray;
	for (const FString& VariantName : VariantNames)
	{
		TSharedPtr<FJsonObject> VariantObject = MakeShared<FJsonObject>();
		VariantObject->SetStringField(TEXT("widget_name"), VariantName);
		VariantJsonArray.Add(MakeShared<FJsonValueObject>(VariantObject));
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("status"), TEXT("ok"));
	Result->SetStringField(TEXT("asset_path"), WidgetBlueprint->GetPathName());
	Result->SetArrayField(TEXT("variants"), VariantJsonArray);
	Result->SetNumberField(TEXT("count"), VariantNames.Num());
	if (!CatalogComponentId.IsEmpty())
	{
		Result->SetStringField(TEXT("catalog_component_id"), CatalogComponentId);
	}
	return SerializeJsonObject(Result);
}

FString UUmgStorybookSubsystem::RenderVariants(
	UWidgetBlueprint* WidgetBlueprint,
	const TArray<FString>& WidgetNames,
	int32 ViewportWidth,
	int32 ViewportHeight,
	const FString& Theme)
{
	using namespace UmgStorybookInternal;

	if (!WidgetBlueprint)
	{
		return SerializeJsonObject(MakeErrorObject(TEXT("WidgetBlueprint is null.")));
	}

	TArray<FString> NamesToRender = WidgetNames;
	if (NamesToRender.Num() == 0)
	{
		FString ListJson = ListVariants(WidgetBlueprint, FString(), FString());
		TSharedPtr<FJsonObject> ListObject;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ListJson);
		if (FJsonSerializer::Deserialize(Reader, ListObject) && ListObject.IsValid())
		{
			const TArray<TSharedPtr<FJsonValue>>* VariantsArray = nullptr;
			if (ListObject->TryGetArrayField(TEXT("variants"), VariantsArray))
			{
				for (const TSharedPtr<FJsonValue>& VariantValue : *VariantsArray)
				{
					const TSharedPtr<FJsonObject>* VariantObject = nullptr;
					if (VariantValue->TryGetObject(VariantObject) && VariantObject && VariantObject->IsValid())
					{
						FString VariantName;
						if ((*VariantObject)->TryGetStringField(TEXT("widget_name"), VariantName))
						{
							NamesToRender.Add(VariantName);
						}
					}
				}
			}
		}
	}

	if (NamesToRender.Num() == 0)
	{
		return SerializeJsonObject(MakeErrorObject(TEXT("No variants to render. Provide widget_names or ensure the widget tree has children.")));
	}

	TArray<TSharedPtr<FJsonValue>> RenderResults;
	for (const FString& VariantName : NamesToRender)
	{
		const FString PreviewJson = RenderWidgetPreview(WidgetBlueprint, VariantName, ViewportWidth, ViewportHeight, Theme);
		TSharedPtr<FJsonObject> PreviewObject;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(PreviewJson);
		if (FJsonSerializer::Deserialize(Reader, PreviewObject) && PreviewObject.IsValid())
		{
			RenderResults.Add(MakeShared<FJsonValueObject>(PreviewObject));
		}
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("status"), TEXT("ok"));
	Result->SetStringField(TEXT("asset_path"), WidgetBlueprint->GetPathName());
	Result->SetArrayField(TEXT("renders"), RenderResults);
	Result->SetNumberField(TEXT("count"), RenderResults.Num());
	return SerializeJsonObject(Result);
}
