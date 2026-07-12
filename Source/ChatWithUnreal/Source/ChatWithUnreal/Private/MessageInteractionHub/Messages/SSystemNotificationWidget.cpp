// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#include "Messages/SSystemNotificationWidget.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Editor.h"

void SSystemNotificationWidget::Construct(const FArguments& InArgs)
{
	MessageText = InArgs._MessageText;
	bIsError = InArgs._IsError;
	OnRetryDelegate = InArgs._OnRetry;

	ChildSlot
	[
		SNew(SBox)
		.HAlign(HAlign_Center)
		.Padding(FMargin(0.0f, 4.0f))
		[
			SAssignNew(RetryButton, SButton)
			.ButtonStyle(FAppStyle::Get(), "SimpleButton")
			.ContentPadding(FMargin(12.0f, 4.0f))
			.OnClicked(this, &SSystemNotificationWidget::OnRetryClicked)
			.Visibility(bIsError && OnRetryDelegate.IsBound() ? EVisibility::Visible : EVisibility::Collapsed)
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("Retry")))
				.Font(FAppStyle::Get().GetFontStyle("SmallFont"))
				.ColorAndOpacity(FLinearColor(0.2f, 0.6f, 1.0f, 1.0f)) // Soft blue
			]
		]
	];
}

FReply SSystemNotificationWidget::OnRetryClicked()
{
	// 触发重试逻辑
	OnRetryDelegate.ExecuteIfBound();

	return FReply::Handled();
}
