// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "UmgLintSubsystem.generated.h"

class UWidgetBlueprint;
class UUserWidget;
class UWidget;
class UPanelWidget;

DECLARE_LOG_CATEGORY_EXTERN(LogUmgLint, Log, All);

USTRUCT()
struct FUmgLintOptions
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FString> Rules;

	UPROPERTY()
	int32 ViewportWidth = 1920;

	UPROPERTY()
	int32 ViewportHeight = 1080;

	UPROPERTY()
	int32 DepthThreshold = 10;
};

USTRUCT()
struct FUmgLintIssue
{
	GENERATED_BODY()

	UPROPERTY()
	FString RuleId;

	UPROPERTY()
	FString Severity;

	UPROPERTY()
	FString Message;

	UPROPERTY()
	FString WidgetPath;

	UPROPERTY()
	TArray<FString> RelatedWidgets;

	UPROPERTY()
	FString Source = TEXT("asset-time");

	TSharedPtr<FJsonObject> FixSuggestion;
};

/**
 * ESLint-style UMG asset linting for AI-driven UI iteration.
 */
UCLASS()
class UMGMCP_API UUmgLintSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Run lint rules and return a JSON report string. */
	FString AnalyzeAsset(UWidgetBlueprint* WidgetBlueprint, const FUmgLintOptions& Options);

	/** Collect layout bounding boxes after building a preview Slate tree. */
	FString GetLayoutDataFromPreview(UWidgetBlueprint* WidgetBlueprint, int32 ViewportWidth, int32 ViewportHeight);

private:
	bool ShouldRunRule(const FUmgLintOptions& Options, const FString& RuleId) const;
	bool LoadRulesConfig(int32& OutDepthThreshold, TArray<TPair<FString, FString>>& OutNamingRules) const;
	FString GetRulesConfigPath() const;

	UUserWidget* CreatePreviewInstance(UWidgetBlueprint* WidgetBlueprint, FString& OutError) const;
	void DestroyPreviewInstance(UUserWidget* PreviewWidget) const;
	bool ArrangePreviewGeometry(UUserWidget* PreviewWidget, int32 ViewportWidth, int32 ViewportHeight) const;

	static TSharedPtr<FJsonObject> BuildOverlapFixSuggestion(
		UWidget* MoveWidget,
		UPanelWidget* ParentPanel,
		float OverlapHeight,
		float OverlapWidth);

	static bool IsRectValidForOverlap(const FSlateRect& Rect);

	static FString BuildWidgetPath(UWidget* Widget);
	static int32 GetWidgetDepth(UWidget* Widget);
	static bool IsWidgetVisibleForLint(UWidget* Widget);
	static bool IsWidgetHitTestBlocking(UWidget* Widget);
	static bool ShouldSkipOverlapCheck(UWidget* Widget);

	void RunNamingConventionRule(UWidgetBlueprint* WidgetBlueprint, const TArray<TPair<FString, FString>>& NamingRules, TArray<FUmgLintIssue>& OutIssues);
	void RunNestingDepthRule(UWidgetBlueprint* WidgetBlueprint, int32 DepthThreshold, TArray<FUmgLintIssue>& OutIssues);
	void RunLayoutOverlapRule(UUserWidget* PreviewWidget, int32 ViewportWidth, int32 ViewportHeight, TArray<FUmgLintIssue>& OutIssues, bool& bPreviewReady);

	static TSharedPtr<FJsonObject> IssueToJson(const FUmgLintIssue& Issue);
	static TSharedPtr<FJsonObject> BuildReport(UWidgetBlueprint* WidgetBlueprint, const TArray<FUmgLintIssue>& Issues, bool bGeometryTrusted = true);
	static FString SerializeJsonObject(const TSharedPtr<FJsonObject>& JsonObject);
};
