// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#include "SAgentStatusBar.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Images/SThrobber.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Images/SImage.h"
#include "HAL/PlatformApplicationMisc.h"

void SAgentStatusBar::Construct(const FArguments& InArgs)
{
	ChildSlot
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(0.0f, 0.0f, 4.0f, 0.0f)
		[
			SAssignNew(SpinnerWidget, SThrobber)
			.PieceImage(FAppStyle::GetBrush("Throbber.CircleChunk"))
			.NumPieces(3)
			.Animate(SThrobber::Opacity)
			.Visibility(InArgs._bShowSpinner ? EVisibility::Visible : EVisibility::Collapsed)
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SAssignNew(StatusTextWidget, STextBlock)
			.Text(FText::FromString(InArgs._StatusText))
			.ColorAndOpacity(FLinearColor::White)
			.Font(FAppStyle::Get().GetFontStyle("SmallFontBold"))
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(4.0f, 0.0f, 0.0f, 0.0f)
		[
			SNew(SButton)
			.ButtonStyle(FAppStyle::Get(), "SimpleButton")
			.ContentPadding(FMargin(4.0f, 2.0f))
			.ToolTipText(FText::FromString(TEXT("Copy Status Text")))
			.OnClicked(this, &SAgentStatusBar::OnCopyButtonClicked)
			[
				SNew(SImage)
				.Image(FAppStyle::Get().GetBrush("GenericCommands.Copy"))
				.DesiredSizeOverride(FVector2D(12, 12))
				.ColorAndOpacity(FSlateColor::UseForeground())
			]
		]
	];
}

FReply SAgentStatusBar::OnCopyButtonClicked()
{
	if (StatusTextWidget.IsValid())
	{
		FString TextToCopy = StatusTextWidget->GetText().ToString();
		FPlatformApplicationMisc::ClipboardCopy(*TextToCopy);
	}
	return FReply::Handled();
}

void SAgentStatusBar::SetStatus(const FString& InStatusText, bool bInShowSpinner, const FLinearColor& StatusColor)
{
	if (SpinnerWidget.IsValid())
	{
		SpinnerWidget->SetVisibility(bInShowSpinner ? EVisibility::Visible : EVisibility::Collapsed);
	}
	if (StatusTextWidget.IsValid())
	{
		StatusTextWidget->SetText(FText::FromString(InStatusText));
		StatusTextWidget->SetColorAndOpacity(StatusColor);
	}
}
