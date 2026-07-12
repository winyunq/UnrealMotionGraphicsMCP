// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#include "STopBar.h"
#include "SUserAvatar.h"
#include "SEditableName.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SSpacer.h"
#include "Styling/AppStyle.h"

void STopBar::Construct(const FArguments& InArgs)
{
	OnNewConversationDelegate = InArgs._OnNewConversation;
	OnShowHistoryDelegate = InArgs._OnShowHistory;

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::Get().GetBrush("ToolPanel.GroupBorder"))
		.Padding(FMargin(10.0f, 5.0f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SAssignNew(AvatarWidget, SUserAvatar)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(10.0f, 0.0f, 0.0f, 0.0f)
			[
				SAssignNew(NameWidget, SEditableName)
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNew(SSpacer)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0.0f, 0.0f, 5.0f, 0.0f)
			[
				SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "SimpleButton")
				.OnClicked(this, &STopBar::OnShowHistoryClicked)
				.ContentPadding(FMargin(5.0f))
				[
					SNew(SImage)
					.Image(FAppStyle::Get().GetBrush("Icons.Layout"))
					.DesiredSizeOverride(FVector2D(16.0f, 16.0f))
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "SimpleButton")
				.OnClicked(this, &STopBar::OnNewConversationClicked)
				.ContentPadding(FMargin(5.0f))
				[
					SNew(SImage)
					.Image(FAppStyle::Get().GetBrush("Icons.Plus"))
					.DesiredSizeOverride(FVector2D(16.0f, 16.0f))
				]
			]
		]
	];
}

void STopBar::SetUserMetadata(const FString& UserName, bool bIsLoggedIn)
{
	if (NameWidget.IsValid())
	{
		NameWidget->SetUserMetadata(UserName, bIsLoggedIn);
	}
}

FReply STopBar::OnNewConversationClicked()
{
	OnNewConversationDelegate.ExecuteIfBound();
	return FReply::Handled();
}

FReply STopBar::OnShowHistoryClicked()
{
	OnShowHistoryDelegate.ExecuteIfBound();
	return FReply::Handled();
}
