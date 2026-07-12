// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

class STopBar;
class SChatWelcome;
class SMessageInteractionHub;
class SBottomBar;

DECLARE_DELEGATE_OneParam(FOnChatWindowInteractionModeChanged, const FString& /*NewMode*/);

class CHATWITHUNREAL_API SChatWindow : public SCompoundWidget
{
public:
	static TWeakPtr<SChatWindow> Instance;

public:
	SLATE_BEGIN_ARGS(SChatWindow) {}
		SLATE_EVENT(FSimpleDelegate, OnShowHistory)
		SLATE_EVENT(FSimpleDelegate, OnNewConversation)
		SLATE_EVENT(FSimpleDelegate, OnSendClicked)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SChatWindow();

	FSimpleDelegate OnShowHistory;
	FSimpleDelegate OnNewConversation;
	FSimpleDelegate OnSendClicked;
	FSimpleDelegate OnInterruptClicked;
	FOnChatWindowInteractionModeChanged OnInteractionModeChanged;

private:
	void OnShowHistoryClicked();
	void OnNewConversationClicked();
	void OnWelcomeSessionSelected(const FString& SessionId);
	void OnSendClickedClicked();
	void OnInterruptClickedClicked();
	void OnInteractionModeChangedClicked(const FString& NewMode);

	TSharedPtr<STopBar> TopBarWidget;
	TSharedPtr<SMessageInteractionHub> MessageHubWidget;
	TSharedPtr<SBottomBar> ChatInputWidget;

	FSimpleDelegate OnShowHistoryEvent;
	FSimpleDelegate OnNewConversationEvent;
	FSimpleDelegate OnSendClickedEvent;
};
