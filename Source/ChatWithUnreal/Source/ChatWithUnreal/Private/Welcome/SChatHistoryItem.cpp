// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#include "SChatHistoryItem.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Styling/AppStyle.h"

void SChatHistoryItem::Construct(const FArguments& InArgs)
{
	SessionId = InArgs._SessionId;
	SessionName = InArgs._SessionName;
	OnSelectedDelegate = InArgs._OnSelected;
	OnDeletedDelegate = InArgs._OnDeleted;

	ChildSlot
	[
		SNew(SButton)
		.ButtonStyle(FAppStyle::Get(), "NoBorder")
		.OnClicked(this, &SChatHistoryItem::HandleClicked)
		.ContentPadding(FMargin(12, 8))
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("DetailsView.CategoryMiddle"))
			.BorderBackgroundColor(FLinearColor(0.2f, 0.2f, 0.2f, 0.4f))
			.Padding(FMargin(8, 4))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText::FromString(SessionName))
					.Font(FAppStyle::Get().GetFontStyle("NormalFont"))
					.ColorAndOpacity(FLinearColor(0.8f, 0.8f, 0.8f, 1.0f))
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SButton)
					.ButtonStyle(FAppStyle::Get(), "HoverHintOnly")
					.OnClicked(this, &SChatHistoryItem::HandleDeleteClicked)
					[
						SNew(STextBlock)
						.Text(FText::FromString(TEXT("")))
						.Font(FAppStyle::Get().GetFontStyle("FontAwesome.10"))
						.ColorAndOpacity(FLinearColor(0.4f, 0.4f, 0.4f, 0.6f))
					]
				]
			]
		]
	];
}

FReply SChatHistoryItem::HandleClicked()
{
	OnSelectedDelegate.ExecuteIfBound(SessionId);
	return FReply::Handled();
}

FReply SChatHistoryItem::HandleDeleteClicked()
{
	OnDeletedDelegate.ExecuteIfBound(SessionId);
	return FReply::Handled();
}
