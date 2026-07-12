// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

DECLARE_DELEGATE_OneParam(FOnImageTagErased, int32 /*ErasedIndex*/);
DECLARE_DELEGATE_OneParam(FOnFilesDropped, const TArray<FString>& /*Files*/);
DECLARE_DELEGATE_RetVal(FReply, FOnPasteShortcutTriggered);

DECLARE_DELEGATE_OneParam(FOnAtAgentTriggered, const FString& /*FilterText*/);

/**
 * SChatInput：纯多行文本输入控件，不包含任何智能体业务或菜单，仅通知退格与文本变化。
 */
class CHATWITHUNREAL_API SChatInput : public SCompoundWidget
{
public:
	static TWeakPtr<SChatInput> Instance;

public:
	SLATE_BEGIN_ARGS(SChatInput) {}
		SLATE_EVENT(FSimpleDelegate, OnSendShortcutTriggered)
		SLATE_EVENT(FOnPasteShortcutTriggered, OnPasteShortcutTriggered)
		SLATE_EVENT(FOnFilesDropped, OnFilesDropped)
		SLATE_EVENT(FOnAtAgentTriggered, OnAtAgentTriggered)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	FText GetText() const;
	void SetText(const FText& InText);
	void ClearText();
	void FocusInput();
	
	void RemoveImageTag(int32 TargetIndex);
	void InsertImageTag(const FString& InImageId);

	void SetAttachmentList(TSharedPtr<class SAttachmentList> InList);

	virtual FReply OnDragOver(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;
	virtual FReply OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;

	FSimpleDelegate OnSendShortcutTriggeredEvent;
	FOnPasteShortcutTriggered OnPasteShortcutTriggeredEvent;
	FOnFilesDropped OnFilesDroppedEvent;
	FOnAtAgentTriggered OnAtAgentTriggeredEvent;

private:
	FReply OnInputKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent);
	void HandleTextChanged(const FText& NewText);

	TSharedPtr<class SMultiLineEditableTextBox> InputTextBox;

	FString LastText;
	bool bIsUpdatingText = false;

	TWeakPtr<class SAttachmentList> AttachmentListWidget;
};
