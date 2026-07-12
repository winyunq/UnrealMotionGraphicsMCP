// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

DECLARE_DELEGATE_OneParam(FOnInteractionModeChanged, const FString& /*NewMode*/);

// 对话模式选择原子控件
class CHATWITHUNREAL_API SInteractionModeSelector : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SInteractionModeSelector) {}
		SLATE_EVENT(FOnInteractionModeChanged, OnInteractionModeChanged)
	SLATE_END_ARGS()

	virtual ~SInteractionModeSelector();

	void Construct(const FArguments& InArgs);
	FString GetCurrentMode() const { return CurrentMode; }
	void SetCurrentModeDirect(const FString& NewMode);

private:
	TSharedRef<SWidget> MakeOptionWidget(TSharedPtr<FString> Option);
	void OnSelectionChanged(TSharedPtr<FString> NewValue, ESelectInfo::Type SelectInfo);
	
	// 修复：补全成员函数声明
	FText GetModeDisplayTextAttr() const;

	TArray<TSharedPtr<FString>> Options;
	
	// 修复：补全成员变量声明
	FString CurrentMode;

	TSharedPtr<class SComboBox<TSharedPtr<FString>>> ComboBox;
	FOnInteractionModeChanged OnInteractionModeChangedEvent;
};
