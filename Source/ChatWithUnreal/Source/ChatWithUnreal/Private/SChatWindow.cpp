// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#include "SChatWindow.h"
#include "SBottomBar.h"
#include "SMessageInteractionHub.h"
#include "STopBar.h"
#include "SChatWelcome.h"
#include "SChatSendButton.h"
#include "Styling/AppStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Editor.h"

TWeakPtr<SChatWindow> SChatWindow::Instance = nullptr;

void SChatWindow::Construct(const FArguments& InArgs)
{
	Instance = SharedThis(this);
	OnShowHistoryEvent = InArgs._OnShowHistory;
	OnNewConversationEvent = InArgs._OnNewConversation;
	OnSendClickedEvent = InArgs._OnSendClicked;

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::Get().GetBrush("ToolPanel.GroupBorder"))
		.Padding(FMargin(10.0f, 6.0f))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SAssignNew(TopBarWidget, STopBar)
				.OnNewConversation(FSimpleDelegate::CreateSP(this, &SChatWindow::OnNewConversationClicked))
				.OnShowHistory(FSimpleDelegate::CreateSP(this, &SChatWindow::OnShowHistoryClicked))
			]
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			.Padding(0.0f, 6.0f, 0.0f, 6.0f)
			[
				SAssignNew(MessageHubWidget, SMessageInteractionHub)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SAssignNew(ChatInputWidget, SBottomBar)
				.OnSendClicked(FSimpleDelegate::CreateSP(this, &SChatWindow::OnSendClickedClicked))
				.OnInterruptClicked(FSimpleDelegate::CreateSP(this, &SChatWindow::OnInterruptClickedClicked))
				.OnInteractionModeChanged(FOnBottomBarInteractionModeChanged::CreateSP(this, &SChatWindow::OnInteractionModeChangedClicked))
			]
		]
	];
}

SChatWindow::~SChatWindow()
{
	if (Instance.Pin().Get() == this)
	{
		Instance = nullptr;
	}
}

void SChatWindow::OnShowHistoryClicked()
{
	OnShowHistory.ExecuteIfBound();
}

void SChatWindow::OnNewConversationClicked()
{
	OnNewConversation.ExecuteIfBound();
}

void SChatWindow::OnSendClickedClicked()
{
	OnSendClicked.ExecuteIfBound();
}

void SChatWindow::OnInterruptClickedClicked()
{
	OnInterruptClicked.ExecuteIfBound();
}

void SChatWindow::OnInteractionModeChangedClicked(const FString& NewMode)
{
	OnInteractionModeChanged.ExecuteIfBound(NewMode);
}

void SChatWindow::OnWelcomeSessionSelected(const FString& SessionId)
{
}
