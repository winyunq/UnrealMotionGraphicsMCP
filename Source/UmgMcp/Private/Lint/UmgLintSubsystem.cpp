// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#include "Lint/UmgLintSubsystem.h"

#include "Algo/Reverse.h"
#include "WidgetBlueprint.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/OverlaySlot.h"
#include "Components/PanelWidget.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/Widget.h"
#include "Editor.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Interfaces/IPluginManager.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Preview/UmgPreviewRenderUtils.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Slate/WidgetRenderer.h"
#include "Widgets/SVirtualWindow.h"
#include "TextureResource.h"
#include "Widgets/SWidget.h"

DEFINE_LOG_CATEGORY(LogUmgLint);

namespace UmgLintInternal
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

	static bool RectsIntersect(const FSlateRect& A, const FSlateRect& B)
	{
		return FSlateRect::DoRectanglesIntersect(A, B);
	}

	static float RectArea(const FSlateRect& R)
	{
		return FMath::Max(0.f, R.Right - R.Left) * FMath::Max(0.f, R.Bottom - R.Top);
	}

	static bool IsDegenerateRect(const FSlateRect& R)
	{
		return RectArea(R) <= 0.01f;
	}
}

void UUmgLintSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	UE_LOG(LogUmgLint, Log, TEXT("UmgLintSubsystem initialized."));
}

void UUmgLintSubsystem::Deinitialize()
{
	UE_LOG(LogUmgLint, Log, TEXT("UmgLintSubsystem deinitialized."));
	Super::Deinitialize();
}

FString UUmgLintSubsystem::GetRulesConfigPath() const
{
	if (const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("UmgMcp")))
	{
		return FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/umg_lint_rules.json"));
	}

	return FPaths::ConvertRelativePathToFull(
		FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("UnrealMotionGraphicsMCP-main/Resources/umg_lint_rules.json")));
}

bool UUmgLintSubsystem::LoadRulesConfig(int32& OutDepthThreshold, TArray<TPair<FString, FString>>& OutNamingRules) const
{
	OutDepthThreshold = 10;
	OutNamingRules.Reset();

	const FString ConfigPath = GetRulesConfigPath();
	if (!FPaths::FileExists(ConfigPath))
	{
		UE_LOG(LogUmgLint, Warning, TEXT("Lint rules file not found: %s"), *ConfigPath);
		return false;
	}

	FString FileContent;
	if (!FFileHelper::LoadFileToString(FileContent, *ConfigPath))
	{
		UE_LOG(LogUmgLint, Warning, TEXT("Failed to read lint rules file: %s"), *ConfigPath);
		return false;
	}

	TSharedPtr<FJsonObject> ConfigJson;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(FileContent);
	if (!FJsonSerializer::Deserialize(Reader, ConfigJson) || !ConfigJson.IsValid())
	{
		UE_LOG(LogUmgLint, Warning, TEXT("Failed to parse lint rules JSON: %s"), *ConfigPath);
		return false;
	}

	double DepthValue = OutDepthThreshold;
	if (ConfigJson->TryGetNumberField(TEXT("depth_threshold"), DepthValue))
	{
		OutDepthThreshold = FMath::Max(1, static_cast<int32>(DepthValue));
	}

	const TArray<TSharedPtr<FJsonValue>>* ConventionsArray = nullptr;
	if (ConfigJson->TryGetArrayField(TEXT("naming_conventions"), ConventionsArray))
	{
		for (const TSharedPtr<FJsonValue>& Entry : *ConventionsArray)
		{
			const TSharedPtr<FJsonObject>* EntryObject = nullptr;
			if (!Entry->TryGetObject(EntryObject) || !EntryObject || !EntryObject->IsValid())
			{
				continue;
			}

			FString ClassSuffix;
			FString Prefix;
			(*EntryObject)->TryGetStringField(TEXT("class_suffix"), ClassSuffix);
			(*EntryObject)->TryGetStringField(TEXT("prefix"), Prefix);
			if (!ClassSuffix.IsEmpty() && !Prefix.IsEmpty())
			{
				OutNamingRules.Add(TPair<FString, FString>(ClassSuffix, Prefix));
			}
		}
	}

	return true;
}

bool UUmgLintSubsystem::ShouldRunRule(const FUmgLintOptions& Options, const FString& RuleId) const
{
	if (Options.Rules.Num() == 0)
	{
		return true;
	}

	for (const FString& Rule : Options.Rules)
	{
		if (Rule.Equals(RuleId, ESearchCase::IgnoreCase))
		{
			return true;
		}
	}
	return false;
}

UUserWidget* UUmgLintSubsystem::CreatePreviewInstance(UWidgetBlueprint* WidgetBlueprint, FString& OutError) const
{
	if (!GEditor)
	{
		OutError = TEXT("GEditor is unavailable.");
		return nullptr;
	}

	UWorld* World = GEditor->GetEditorWorldContext().World();
	return FUmgPreviewRenderUtils::CreatePreviewInstance(WidgetBlueprint, World, OutError);
}

void UUmgLintSubsystem::DestroyPreviewInstance(UUserWidget* PreviewWidget) const
{
	if (PreviewWidget)
	{
		PreviewWidget->ConditionalBeginDestroy();
	}
}

bool UUmgLintSubsystem::ArrangePreviewGeometry(UUserWidget* PreviewWidget, int32 ViewportWidth, int32 ViewportHeight) const
{
	if (!PreviewWidget)
	{
		return false;
	}

	TSharedPtr<SWidget> RootSlate = PreviewWidget->GetCachedWidget();
	if (!RootSlate.IsValid())
	{
		return false;
	}

	const FVector2D DrawSize(
		static_cast<float>(UmgLintInternal::ClampViewportSize(ViewportWidth, 1920)),
		static_cast<float>(UmgLintInternal::ClampViewportSize(ViewportHeight, 1080)));

	TSharedPtr<SVirtualWindow> VirtualHost;
	const TSharedRef<SWidget> RenderTree = FUmgPreviewRenderUtils::PrepareOffscreenRenderTree(
		RootSlate.ToSharedRef(),
		DrawSize,
		VirtualHost);

	UTextureRenderTarget2D* RenderTarget = NewObject<UTextureRenderTarget2D>(GetTransientPackage());
	if (!RenderTarget)
	{
		return false;
	}

	RenderTarget->RenderTargetFormat = RTF_RGBA8;
	RenderTarget->ClearColor = FLinearColor(0.f, 0.f, 0.f, 0.f);
	RenderTarget->InitAutoFormat(
		UmgLintInternal::ClampViewportSize(ViewportWidth, 1920),
		UmgLintInternal::ClampViewportSize(ViewportHeight, 1080));
	RenderTarget->UpdateResourceImmediate(true);

	FWidgetRenderer WidgetRenderer(true);
	WidgetRenderer.DrawWidget(RenderTarget, RenderTree, DrawSize, 0.f, false);
	FlushRenderingCommands();

	FUmgPreviewRenderUtils::ReleaseOffscreenRenderHost(VirtualHost);
	return true;
}

bool UUmgLintSubsystem::IsRectValidForOverlap(const FSlateRect& Rect)
{
	return !UmgLintInternal::IsDegenerateRect(Rect);
}

TSharedPtr<FJsonObject> UUmgLintSubsystem::BuildOverlapFixSuggestion(
	UWidget* MoveWidget,
	UPanelWidget* ParentPanel,
	float OverlapHeight,
	float OverlapWidth)
{
	if (!MoveWidget || !ParentPanel || !MoveWidget->Slot)
	{
		return nullptr;
	}

	const float PadValue = FMath::CeilToFloat(FMath::Max(OverlapHeight, OverlapWidth) + 1.f);
	TSharedPtr<FJsonObject> Fix = MakeShared<FJsonObject>();
	TSharedPtr<FJsonObject> Params = MakeShared<FJsonObject>();
	TSharedPtr<FJsonObject> Properties = MakeShared<FJsonObject>();
	TSharedPtr<FJsonObject> Slot = MakeShared<FJsonObject>();
	TSharedPtr<FJsonObject> Meta = MakeShared<FJsonObject>();

	Params->SetStringField(TEXT("widget_name"), MoveWidget->GetName());
	Meta->SetBoolField(TEXT("autoApplicable"), true);
	Meta->SetStringField(TEXT("slotClass"), MoveWidget->Slot->GetClass()->GetName());
	Meta->SetStringField(TEXT("parentClass"), ParentPanel->GetClass()->GetName());

	if (MoveWidget->Slot->IsA(UCanvasPanelSlot::StaticClass()))
	{
		TSharedPtr<FJsonObject> LayoutData = MakeShared<FJsonObject>();
		TSharedPtr<FJsonObject> Offsets = MakeShared<FJsonObject>();
		Offsets->SetNumberField(TEXT("Top"), FMath::CeilToFloat(OverlapHeight + 1.f));
		Offsets->SetNumberField(TEXT("Left"), FMath::CeilToFloat(OverlapWidth + 1.f));
		LayoutData->SetObjectField(TEXT("Offsets"), Offsets);
		Slot->SetObjectField(TEXT("LayoutData"), LayoutData);
	}
	else if (MoveWidget->Slot->IsA(UVerticalBoxSlot::StaticClass()))
	{
		TSharedPtr<FJsonObject> Padding = MakeShared<FJsonObject>();
		Padding->SetNumberField(TEXT("Bottom"), PadValue);
		Slot->SetObjectField(TEXT("Padding"), Padding);
	}
	else if (MoveWidget->Slot->IsA(UHorizontalBoxSlot::StaticClass()))
	{
		TSharedPtr<FJsonObject> Padding = MakeShared<FJsonObject>();
		Padding->SetNumberField(TEXT("Right"), PadValue);
		Slot->SetObjectField(TEXT("Padding"), Padding);
	}
	else if (MoveWidget->Slot->IsA(UOverlaySlot::StaticClass()))
	{
		TSharedPtr<FJsonObject> Padding = MakeShared<FJsonObject>();
		Padding->SetNumberField(TEXT("Top"), PadValue);
		Slot->SetObjectField(TEXT("Padding"), Padding);
	}
	else
	{
		return nullptr;
	}

	Properties->SetObjectField(TEXT("Slot"), Slot);
	Params->SetObjectField(TEXT("properties"), Properties);
	Fix->SetStringField(TEXT("action"), TEXT("set_widget_properties"));
	Fix->SetObjectField(TEXT("params"), Params);
	Fix->SetObjectField(TEXT("meta"), Meta);
	return Fix;
}

FString UUmgLintSubsystem::BuildWidgetPath(UWidget* Widget)
{
	TArray<FString> Parts;
	while (Widget)
	{
		Parts.Add(Widget->GetName());
		Widget = Widget->GetParent();
	}
	Algo::Reverse(Parts);
	return FString::Join(Parts, TEXT("/"));
}

int32 UUmgLintSubsystem::GetWidgetDepth(UWidget* Widget)
{
	int32 Depth = 0;
	while (Widget)
	{
		++Depth;
		Widget = Widget->GetParent();
	}
	return Depth;
}

bool UUmgLintSubsystem::IsWidgetVisibleForLint(UWidget* Widget)
{
	if (!Widget)
	{
		return false;
	}

	const ESlateVisibility Visibility = Widget->GetVisibility();
	return Visibility != ESlateVisibility::Collapsed && Visibility != ESlateVisibility::Hidden;
}

bool UUmgLintSubsystem::IsWidgetHitTestBlocking(UWidget* Widget)
{
	if (!Widget || !IsWidgetVisibleForLint(Widget))
	{
		return false;
	}

	const ESlateVisibility Visibility = Widget->GetVisibility();
	return Visibility == ESlateVisibility::Visible;
}

bool UUmgLintSubsystem::ShouldSkipOverlapCheck(UWidget* Widget)
{
	return !Widget || !IsWidgetVisibleForLint(Widget);
}

void UUmgLintSubsystem::RunNamingConventionRule(
	UWidgetBlueprint* WidgetBlueprint,
	const TArray<TPair<FString, FString>>& NamingRules,
	TArray<FUmgLintIssue>& OutIssues)
{
	if (!WidgetBlueprint || !WidgetBlueprint->WidgetTree || NamingRules.Num() == 0)
	{
		return;
	}

	TArray<UWidget*> AllWidgets;
	WidgetBlueprint->WidgetTree->GetAllWidgets(AllWidgets);

	for (UWidget* Widget : AllWidgets)
	{
		if (!Widget)
		{
			continue;
		}

		const FString ClassName = Widget->GetClass()->GetName();
		const FString WidgetName = Widget->GetName();

		for (const TPair<FString, FString>& Rule : NamingRules)
		{
			if (!ClassName.Contains(Rule.Key))
			{
				continue;
			}

			if (WidgetName.StartsWith(Rule.Value))
			{
				break;
			}

			FUmgLintIssue Issue;
			Issue.RuleId = TEXT("naming-convention");
			Issue.Severity = TEXT("warning");
			Issue.Message = FString::Printf(
				TEXT("Widget '%s' (%s) should use prefix '%s' per naming convention."),
				*WidgetName, *ClassName, *Rule.Value);
			Issue.WidgetPath = BuildWidgetPath(Widget);
			Issue.Source = TEXT("asset-time");
			OutIssues.Add(Issue);
			break;
		}
	}
}

void UUmgLintSubsystem::RunNestingDepthRule(
	UWidgetBlueprint* WidgetBlueprint,
	int32 DepthThreshold,
	TArray<FUmgLintIssue>& OutIssues)
{
	if (!WidgetBlueprint || !WidgetBlueprint->WidgetTree)
	{
		return;
	}

	TArray<UWidget*> AllWidgets;
	WidgetBlueprint->WidgetTree->GetAllWidgets(AllWidgets);

	for (UWidget* Widget : AllWidgets)
	{
		if (!Widget)
		{
			continue;
		}

		const int32 Depth = GetWidgetDepth(Widget);
		if (Depth <= DepthThreshold)
		{
			continue;
		}

		FUmgLintIssue Issue;
		Issue.RuleId = TEXT("nesting-depth-limit");
		Issue.Severity = TEXT("warning");
		Issue.Message = FString::Printf(
			TEXT("Widget hierarchy depth is %d (limit %d). Consider splitting into a nested UserWidget."),
			Depth, DepthThreshold);
		Issue.WidgetPath = BuildWidgetPath(Widget);
		Issue.Source = TEXT("asset-time");
		OutIssues.Add(Issue);
	}
}

void UUmgLintSubsystem::RunLayoutOverlapRule(
	UUserWidget* PreviewWidget,
	int32 ViewportWidth,
	int32 ViewportHeight,
	TArray<FUmgLintIssue>& OutIssues,
	bool& bPreviewReady)
{
	bPreviewReady = false;
	if (!PreviewWidget || !PreviewWidget->WidgetTree)
	{
		return;
	}

	if (!ArrangePreviewGeometry(PreviewWidget, ViewportWidth, ViewportHeight))
	{
		return;
	}

	TArray<UWidget*> AllWidgets;
	PreviewWidget->WidgetTree->GetAllWidgets(AllWidgets);

	TMap<UPanelWidget*, TArray<UWidget*>> ChildrenByParent;
	int32 ValidGeometryCount = 0;
	for (UWidget* Widget : AllWidgets)
	{
		if (!Widget || ShouldSkipOverlapCheck(Widget))
		{
			continue;
		}

		if (!Widget->GetCachedWidget().IsValid())
		{
			continue;
		}

		const FSlateRect Rect = Widget->GetCachedWidget()->GetTickSpaceGeometry().GetLayoutBoundingRect();
		if (IsRectValidForOverlap(Rect))
		{
			++ValidGeometryCount;
		}

		UPanelWidget* ParentPanel = Cast<UPanelWidget>(Widget->GetParent());
		if (!ParentPanel)
		{
			continue;
		}

		ChildrenByParent.FindOrAdd(ParentPanel).Add(Widget);
	}

	if (ChildrenByParent.Num() == 0 || ValidGeometryCount == 0)
	{
		return;
	}

	bPreviewReady = true;

	for (const TPair<UPanelWidget*, TArray<UWidget*>>& Pair : ChildrenByParent)
	{
		UPanelWidget* ParentPanel = Pair.Key;
		const TArray<UWidget*>& Siblings = Pair.Value;
		for (int32 I = 0; I < Siblings.Num(); ++I)
		{
			UWidget* WidgetA = Siblings[I];
			if (!WidgetA || !WidgetA->GetCachedWidget().IsValid())
			{
				continue;
			}

			const FSlateRect RectA = WidgetA->GetCachedWidget()->GetTickSpaceGeometry().GetLayoutBoundingRect();
			if (!IsRectValidForOverlap(RectA))
			{
				continue;
			}

			for (int32 J = I + 1; J < Siblings.Num(); ++J)
			{
				UWidget* WidgetB = Siblings[J];
				if (!WidgetB || !WidgetB->GetCachedWidget().IsValid())
				{
					continue;
				}

				if (ShouldSkipOverlapCheck(WidgetA) || ShouldSkipOverlapCheck(WidgetB))
				{
					continue;
				}

				const FSlateRect RectB = WidgetB->GetCachedWidget()->GetTickSpaceGeometry().GetLayoutBoundingRect();
				if (!IsRectValidForOverlap(RectB) || !UmgLintInternal::RectsIntersect(RectA, RectB))
				{
					continue;
				}

				const bool bAHitTest = IsWidgetHitTestBlocking(WidgetA);
				const bool bBHitTest = IsWidgetHitTestBlocking(WidgetB);
				const bool bBothHitTest = bAHitTest && bBHitTest;

				FUmgLintIssue Issue;
				Issue.RuleId = TEXT("layout-overlap");
				Issue.Severity = bBothHitTest ? TEXT("error") : TEXT("warning");
				Issue.WidgetPath = BuildWidgetPath(WidgetA);
				Issue.RelatedWidgets.Add(WidgetB->GetName());
				Issue.Source = TEXT("asset-time");

				if (bBothHitTest)
				{
					Issue.Message = FString::Printf(
						TEXT("'%s' overlaps '%s' under %s and both are hit-test visible; clicks may be intercepted."),
						*WidgetA->GetName(), *WidgetB->GetName(), *ParentPanel->GetClass()->GetName());
				}
				else
				{
					Issue.Message = FString::Printf(
						TEXT("'%s' visually overlaps '%s' under %s."),
						*WidgetA->GetName(), *WidgetB->GetName(), *ParentPanel->GetClass()->GetName());
				}

				const bool bMoveA = UmgLintInternal::RectArea(RectA) <= UmgLintInternal::RectArea(RectB);
				UWidget* MoveWidget = bMoveA ? WidgetA : WidgetB;
				const FSlateRect OverlapRect(
					FMath::Max(RectA.Left, RectB.Left),
					FMath::Max(RectA.Top, RectB.Top),
					FMath::Min(RectA.Right, RectB.Right),
					FMath::Min(RectA.Bottom, RectB.Bottom));
				const float OverlapHeight = FMath::Max(0.f, OverlapRect.Bottom - OverlapRect.Top);
				const float OverlapWidth = FMath::Max(0.f, OverlapRect.Right - OverlapRect.Left);

				if (TSharedPtr<FJsonObject> Fix = BuildOverlapFixSuggestion(MoveWidget, ParentPanel, OverlapHeight, OverlapWidth))
				{
					Issue.FixSuggestion = Fix;
				}
				else
				{
					Issue.Message += TEXT(" Manual fix required: adjust Slot properties compatible with the parent panel type.");
				}

				OutIssues.Add(Issue);
			}
		}
	}
}

TSharedPtr<FJsonObject> UUmgLintSubsystem::IssueToJson(const FUmgLintIssue& Issue)
{
	TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetStringField(TEXT("ruleId"), Issue.RuleId);
	Json->SetStringField(TEXT("severity"), Issue.Severity);
	Json->SetStringField(TEXT("message"), Issue.Message);
	Json->SetStringField(TEXT("widgetPath"), Issue.WidgetPath);
	Json->SetStringField(TEXT("source"), Issue.Source);

	if (Issue.RelatedWidgets.Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> Related;
		for (const FString& Name : Issue.RelatedWidgets)
		{
			Related.Add(MakeShared<FJsonValueString>(Name));
		}
		Json->SetArrayField(TEXT("relatedWidgets"), Related);
	}

	if (Issue.FixSuggestion.IsValid())
	{
		Json->SetObjectField(TEXT("fixSuggestion"), Issue.FixSuggestion);
	}

	return Json;
}

TSharedPtr<FJsonObject> UUmgLintSubsystem::BuildReport(UWidgetBlueprint* WidgetBlueprint, const TArray<FUmgLintIssue>& Issues, bool bGeometryTrusted)
{
	int32 ErrorCount = 0;
	int32 WarningCount = 0;
	int32 InfoCount = 0;

	TArray<TSharedPtr<FJsonValue>> IssuesArray;
	for (const FUmgLintIssue& Issue : Issues)
	{
		if (Issue.Severity == TEXT("error"))
		{
			++ErrorCount;
		}
		else if (Issue.Severity == TEXT("warning"))
		{
			++WarningCount;
		}
		else
		{
			++InfoCount;
		}
		IssuesArray.Add(MakeShared<FJsonValueObject>(IssueToJson(Issue)));
	}

	TSharedPtr<FJsonObject> Summary = MakeShared<FJsonObject>();
	Summary->SetNumberField(TEXT("error"), ErrorCount);
	Summary->SetNumberField(TEXT("warning"), WarningCount);
	Summary->SetNumberField(TEXT("info"), InfoCount);

	TSharedPtr<FJsonObject> Report = MakeShared<FJsonObject>();
	Report->SetBoolField(TEXT("success"), true);
	if (WidgetBlueprint)
	{
		Report->SetStringField(TEXT("assetPath"), WidgetBlueprint->GetPathName());
	}
	Report->SetBoolField(TEXT("geometryTrusted"), bGeometryTrusted);
	Report->SetBoolField(TEXT("ok"), ErrorCount == 0 && bGeometryTrusted);
	Report->SetObjectField(TEXT("summary"), Summary);
	Report->SetArrayField(TEXT("issues"), IssuesArray);
	return Report;
}

FString UUmgLintSubsystem::SerializeJsonObject(const TSharedPtr<FJsonObject>& JsonObject)
{
	FString Output;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
	FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);
	return Output;
}

FString UUmgLintSubsystem::AnalyzeAsset(UWidgetBlueprint* WidgetBlueprint, const FUmgLintOptions& Options)
{
	TArray<FUmgLintIssue> Issues;

	if (!WidgetBlueprint)
	{
		TSharedPtr<FJsonObject> ErrorReport = MakeShared<FJsonObject>();
		ErrorReport->SetBoolField(TEXT("success"), false);
		ErrorReport->SetStringField(TEXT("error"), TEXT("WidgetBlueprint is null."));
		return SerializeJsonObject(ErrorReport);
	}

	int32 DepthThreshold = Options.DepthThreshold > 0 ? Options.DepthThreshold : 10;
	TArray<TPair<FString, FString>> NamingRules;
	int32 ConfigDepth = DepthThreshold;
	if (LoadRulesConfig(ConfigDepth, NamingRules))
	{
		if (Options.DepthThreshold <= 0)
		{
			DepthThreshold = ConfigDepth;
		}
	}

	if (ShouldRunRule(Options, TEXT("naming-convention")))
	{
		RunNamingConventionRule(WidgetBlueprint, NamingRules, Issues);
	}

	if (ShouldRunRule(Options, TEXT("nesting-depth-limit")))
	{
		RunNestingDepthRule(WidgetBlueprint, DepthThreshold, Issues);
	}

	bool bGeometryTrusted = true;
	if (ShouldRunRule(Options, TEXT("layout-overlap")))
	{
		FString PreviewError;
		UUserWidget* PreviewWidget = CreatePreviewInstance(WidgetBlueprint, PreviewError);
		bool bPreviewReady = false;

		if (PreviewWidget)
		{
			RunLayoutOverlapRule(PreviewWidget, Options.ViewportWidth, Options.ViewportHeight, Issues, bPreviewReady);
			bGeometryTrusted = bPreviewReady;
			DestroyPreviewInstance(PreviewWidget);
		}
		else
		{
			bGeometryTrusted = false;
		}

		if (!bPreviewReady)
		{
			FUmgLintIssue InfoIssue;
			InfoIssue.RuleId = TEXT("preview-not-ready");
			InfoIssue.Severity = TEXT("warning");
			InfoIssue.Message = PreviewError.IsEmpty()
				? TEXT("Layout geometry was not arranged via off-screen render; overlap results are not trusted for preview gating.")
				: PreviewError;
			InfoIssue.WidgetPath = TEXT("");
			InfoIssue.Source = TEXT("asset-time");
			Issues.Add(InfoIssue);
		}
	}

	return SerializeJsonObject(BuildReport(WidgetBlueprint, Issues, bGeometryTrusted));
}

FString UUmgLintSubsystem::GetLayoutDataFromPreview(
	UWidgetBlueprint* WidgetBlueprint,
	int32 ViewportWidth,
	int32 ViewportHeight)
{
	if (!WidgetBlueprint)
	{
		TSharedPtr<FJsonObject> ErrorReport = MakeShared<FJsonObject>();
		ErrorReport->SetBoolField(TEXT("success"), false);
		ErrorReport->SetStringField(TEXT("error"), TEXT("WidgetBlueprint is null."));
		return SerializeJsonObject(ErrorReport);
	}

	FString PreviewError;
	UUserWidget* PreviewWidget = CreatePreviewInstance(WidgetBlueprint, PreviewError);
	if (!PreviewWidget)
	{
		TSharedPtr<FJsonObject> ErrorReport = MakeShared<FJsonObject>();
		ErrorReport->SetBoolField(TEXT("success"), false);
		ErrorReport->SetStringField(TEXT("error"), PreviewError);
		return SerializeJsonObject(ErrorReport);
	}

	const bool bGeometryTrusted = ArrangePreviewGeometry(PreviewWidget, ViewportWidth, ViewportHeight);

	TArray<UWidget*> AllWidgets;
	PreviewWidget->WidgetTree->GetAllWidgets(AllWidgets);

	TArray<TSharedPtr<FJsonValue>> LayoutDataArray;
	for (UWidget* Widget : AllWidgets)
	{
		if (!Widget || !Widget->GetCachedWidget().IsValid())
		{
			continue;
		}

		const FSlateRect BoundingBox = Widget->GetCachedWidget()->GetTickSpaceGeometry().GetLayoutBoundingRect();
		if (!IsRectValidForOverlap(BoundingBox))
		{
			continue;
		}

		TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
		Entry->SetStringField(TEXT("widget_name"), Widget->GetName());
		Entry->SetStringField(TEXT("widget_path"), BuildWidgetPath(Widget));
		Entry->SetNumberField(TEXT("left"), BoundingBox.Left);
		Entry->SetNumberField(TEXT("top"), BoundingBox.Top);
		Entry->SetNumberField(TEXT("right"), BoundingBox.Right);
		Entry->SetNumberField(TEXT("bottom"), BoundingBox.Bottom);
		LayoutDataArray.Add(MakeShared<FJsonValueObject>(Entry));
	}

	DestroyPreviewInstance(PreviewWidget);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetBoolField(TEXT("geometryTrusted"), bGeometryTrusted && LayoutDataArray.Num() > 0);
	Result->SetArrayField(TEXT("layout_data"), LayoutDataArray);
	Result->SetNumberField(TEXT("viewport_width"), UmgLintInternal::ClampViewportSize(ViewportWidth, 1920));
	Result->SetNumberField(TEXT("viewport_height"), UmgLintInternal::ClampViewportSize(ViewportHeight, 1080));
	return SerializeJsonObject(Result);
}
