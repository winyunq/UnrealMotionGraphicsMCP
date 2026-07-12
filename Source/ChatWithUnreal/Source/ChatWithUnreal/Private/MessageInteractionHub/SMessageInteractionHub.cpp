// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#include "SMessageInteractionHub.h"
#include "SChatWelcome.h"
#include "MessageInteractionHub/Messages/SAgentResponseGroup.h"
#include "MessageInteractionHub/Messages/SSystemNotificationWidget.h"
#include "MessageInteractionHub/Messages/SUserMessageWidget.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"

TWeakPtr<SMessageInteractionHub> SMessageInteractionHub::Instance = nullptr;

void SMessageInteractionHub::Construct(const FArguments& InArgs)
{
	Instance = SharedThis(this);
	OnSessionSelectedEvent = InArgs._OnSessionSelected;
	OnSessionDeletedEvent = InArgs._OnSessionDeleted;

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			SAssignNew(WelcomeWidget, SChatWelcome)
			.OnSessionSelected_Lambda([this](const FString& SessionId) {
				OnSessionSelectedEvent.ExecuteIfBound(SessionId);
			})
			.OnSessionDeleted_Lambda([this](const FString& SessionId) {
				OnSessionDeletedEvent.ExecuteIfBound(SessionId);
			})
		]
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			SAssignNew(MessageListBorder, SBorder)
			.BorderImage(FAppStyle::Get().GetBrush("ToolPanel.DarkGroupBorder"))
			.Padding(FMargin(8.0f))
			[
				SAssignNew(ScrollBoxWidget, SScrollBox)
				.ScrollBarVisibility(EVisibility::Collapsed)
				+ SScrollBox::Slot()
				[
					SAssignNew(MessageList, SVerticalBox)
				]
			]
		]
	];

	RefreshVisibility();
}

void SMessageInteractionHub::ClearMessages()
{
	if (MessageList.IsValid())
	{
		MessageList->ClearChildren();
		RefreshVisibility();
	}
}

void SMessageInteractionHub::RefreshVisibility()
{
	bool bHasVisibleMessages = false;
	if (MessageList.IsValid())
	{
		FChildren* Children = MessageList->GetChildren();
		for (int32 i = 0; i < Children->Num(); ++i)
		{
			if (Children->GetChildAt(i)->GetVisibility().IsVisible())
			{
				bHasVisibleMessages = true;
				break;
			}
		}
	}

	if (bHasVisibleMessages)
	{
		if (WelcomeWidget.IsValid()) WelcomeWidget->SetVisibility(EVisibility::Collapsed);
		if (MessageListBorder.IsValid()) MessageListBorder->SetVisibility(EVisibility::Visible);
	}
	else
	{
		if (WelcomeWidget.IsValid()) WelcomeWidget->SetVisibility(EVisibility::Visible);
		if (MessageListBorder.IsValid()) MessageListBorder->SetVisibility(EVisibility::Collapsed);
	}
}

void SMessageInteractionHub::AddMessageWidget(TSharedRef<SWidget> InWidget)
{
	if (MessageList.IsValid())
	{
		MessageList->AddSlot().AutoHeight()[InWidget];
		RefreshVisibility();
		if (ScrollBoxWidget.IsValid()) ScrollBoxWidget->ScrollToEnd();
	}
}

void SMessageInteractionHub::MoveWidgetToBottom(TSharedRef<SWidget> InWidget)
{
	if (MessageList.IsValid())
	{
		MessageList->RemoveSlot(InWidget);
		MessageList->AddSlot().AutoHeight()[InWidget];
	}
}

void SMessageInteractionHub::RemoveMessageWidget(TSharedRef<SWidget> InWidget)
{
	if (MessageList.IsValid())
	{
		MessageList->RemoveSlot(InWidget);
		RefreshVisibility();
	}
}

void SMessageInteractionHub::RestoreHistoryMessage(const FString& AgentName, const FString& Content, const TArray<FString>& Base64Images, bool bIsUser)
{
	if (bIsUser)
	{
		TSharedRef<SUserMessageWidget> UserMsg = SNew(SUserMessageWidget)
			.MessageText(Content)
			.Base64Images(Base64Images);
		AddMessageWidget(UserMsg);
	}
	else
	{
		TSharedRef<SAgentResponseGroup> AgentMsg = SNew(SAgentResponseGroup).AgentName(AgentName);
		AgentMsg->AddTextOutputBlock(Content);
		AgentMsg->SetAgentStatus(NSLOCTEXT("UmgMcp", "StatusHistory", "History"), false, FLinearColor::Gray);
		AddMessageWidget(AgentMsg);
	}
	RefreshVisibility();
}
