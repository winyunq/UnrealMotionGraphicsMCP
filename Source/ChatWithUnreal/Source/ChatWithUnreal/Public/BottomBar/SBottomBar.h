// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Views/SListView.h"

class SChatInput;
class SAtAgentMessage;
class SAbilitiesSelector;
class SInteractionModeSelector;
class SToolModeSelector;
class SChatSendButton;
class SQuotaBar;
class SAttachmentList;

DECLARE_DELEGATE_OneParam(FOnBottomBarInteractionModeChanged, const FString& /*NewMode*/);
DECLARE_DELEGATE_OneParam(FOnBottomBarToolModeChanged, const FString& /*NewTool*/);
DECLARE_DELEGATE_OneParam(FOnBottomBarAtAgentChanged, const FString& /*NewAgent*/);

/**
 * SBottomBar: 组合各个底栏组件。
 */
class CHATWITHUNREAL_API SBottomBar : public SCompoundWidget
{
public:
	static TWeakPtr<SBottomBar> Instance;

public:
	SLATE_BEGIN_ARGS(SBottomBar) {}
		SLATE_EVENT(FSimpleDelegate, OnSendClicked)
		SLATE_EVENT(FSimpleDelegate, OnInterruptClicked)
		SLATE_EVENT(FOnBottomBarInteractionModeChanged, OnInteractionModeChanged)
		SLATE_EVENT(FOnBottomBarToolModeChanged, OnToolModeChanged)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	FString GetInteractionMode() const;



	// 原子微控件
	TSharedPtr<SAtAgentMessage> AtAgentMessage;
	TSharedPtr<SChatInput> ChatInput;
	TSharedPtr<SAbilitiesSelector> AbilitiesSelector;
	TSharedPtr<SInteractionModeSelector> InteractionModeSelector;
	TSharedPtr<SToolModeSelector> ToolModeSelector;
	TSharedPtr<SChatSendButton> SendButton;
	TSharedPtr<SAttachmentList> AttachmentList;
	TSharedPtr<SQuotaBar> QuotaBar;

private:
	void OnInteractionModeChanged(const FString& NewMode);
	void OnToolModeChanged(const FString& NewTool);
	void OnChatInputSendRequested();
	FReply OnPasteImageFromClipboard();
	FReply OnAddAttachmentClicked();
	void OnFilesDropped(const TArray<FString>& Files);



	FSimpleDelegate OnSendClickedEvent;
	FSimpleDelegate OnInterruptClickedEvent;
	FOnBottomBarInteractionModeChanged OnInteractionModeChangedEvent;
	FOnBottomBarToolModeChanged OnToolModeChangedEvent;
};
