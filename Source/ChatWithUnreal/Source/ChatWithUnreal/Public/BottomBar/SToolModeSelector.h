// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

DECLARE_DELEGATE_RetVal(FString, FOnGetToolSelectorInteractionMode);
DECLARE_DELEGATE_OneParam(FOnToolModeChanged, const FString& /*NewTool*/);

// 工具模式选择原子控件
class CHATWITHUNREAL_API SToolModeSelector : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SToolModeSelector) {}
		SLATE_EVENT(FOnGetToolSelectorInteractionMode, OnGetInteractionMode)
		SLATE_EVENT(FOnToolModeChanged, OnToolModeChanged)
	SLATE_END_ARGS()

	virtual ~SToolModeSelector();

	void Construct(const FArguments& InArgs);
	void SyncWithInteractionMode(const FString& NewInteractionMode);
	FString GetCurrentTool() const { return CurrentTool; }

	EVisibility GetVisibilityBasedOnMode() const;

private:
	TSharedRef<SWidget> MakeOptionWidget(TSharedPtr<FString> Option);
	void OnSelectionChanged(TSharedPtr<FString> NewValue, ESelectInfo::Type SelectInfo);
	
	// 修复：补全成员函数声明
	FText GetToolDisplayTextAttr() const;

	TArray<TSharedPtr<FString>> Options;
	
	// 修复：补全成员变量声明
	FString CurrentTool;

	TSharedPtr<class SComboBox<TSharedPtr<FString>>> ComboBox;
	FOnGetToolSelectorInteractionMode OnGetInteractionModeEvent;
	FOnToolModeChanged OnToolModeChangedEvent;
};
