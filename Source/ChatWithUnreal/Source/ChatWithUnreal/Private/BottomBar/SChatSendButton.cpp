// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#include "SChatSendButton.h"
#include "SChatInput.h"
#include "SAttachmentList.h"
#include "Editor.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Images/SThrobber.h"
#include "Styling/AppStyle.h"
#include "Misc/DateTime.h"

TWeakPtr<SChatSendButton> SChatSendButton::Instance = nullptr;

void SChatSendButton::Construct(const FArguments& InArgs)
{
	Instance = SharedThis(this);
	OnSendClicked = InArgs._OnSendClicked;
	OnInterruptClicked = InArgs._OnInterruptClicked;

	ChildSlot
	[
		SNew(SButton)
		.ButtonStyle(FAppStyle::Get(), "PrimaryButton")
		.OnClicked(this, &SChatSendButton::HandleOnClicked)
		.ContentPadding(FMargin(16.0f, 4.0f))
		.ButtonColorAndOpacity(this, &SChatSendButton::GetButtonBackgroundColor)
		[
			SNew(SBox)
			.MinDesiredWidth(60.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(this, &SChatSendButton::GetButtonText)
					.Font(FAppStyle::Get().GetFontStyle("NormalFont"))
					.ColorAndOpacity(this, &SChatSendButton::GetButtonColor)
				]

				// Generating State (Throbber visible ONLY when running)
				+ SHorizontalBox::Slot()
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					SNew(SThrobber)
					.PieceImage(FAppStyle::GetBrush("Throbber.CircleChunk"))
					.NumPieces(3)
					.Animate(SThrobber::Opacity)
					.Visibility_Lambda([this]() {
						return bIsRunning ? EVisibility::Visible : EVisibility::Collapsed;
					})
				]
			]
		]
	];
}

void SChatSendButton::SetIsRunning(bool bRunning)
{
	bIsRunning = bRunning;
	bInterruptConfirmArmed = false;
}

bool SChatSendButton::IsRunning() const
{
	return bIsRunning;
}

FReply SChatSendButton::HandleOnClicked()
{
	if (bIsRunning)
	{
		FDateTime Now = FDateTime::UtcNow();
		if (!bInterruptConfirmArmed || Now > InterruptConfirmExpireAt)
		{
			bInterruptConfirmArmed = true;
			InterruptConfirmExpireAt = Now + FTimespan::FromSeconds(3.0);
			return FReply::Handled();
		}

		// 中断逻辑：执行绑定的外部中断处理委托
		OnInterruptClicked.ExecuteIfBound();
		return FReply::Handled();
	}

	bInterruptConfirmArmed = false;
	
	// 发送逻辑：执行绑定的外部发送委托
	OnSendClicked.ExecuteIfBound();

	return FReply::Handled();
}

FText SChatSendButton::GetButtonText() const
{
	if (bIsRunning)
	{
		if (bInterruptConfirmArmed && FDateTime::UtcNow() <= InterruptConfirmExpireAt)
		{
			return FText::FromString(TEXT("3s内再次点击中断"));
		}
		return FText::FromString(TEXT("Interrupt"));
	}
	return FText::FromString(TEXT("Send"));
}

FSlateColor SChatSendButton::GetButtonBackgroundColor() const
{
	if (bIsRunning)
	{
		if (bInterruptConfirmArmed && FDateTime::UtcNow() <= InterruptConfirmExpireAt)
		{
			return FLinearColor(1.0f, 0.0f, 0.0f); // Bright red
		}
		return FLinearColor(0.8f, 0.2f, 0.2f); // Dull red
	}
	return FLinearColor::White;
}

FSlateColor SChatSendButton::GetButtonColor() const
{
	return FSlateColor::UseForeground();
}
