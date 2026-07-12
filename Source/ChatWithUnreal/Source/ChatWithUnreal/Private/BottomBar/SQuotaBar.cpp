// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#include "SQuotaBar.h"
#include "Styling/AppStyle.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SBorder.h"
#include "Math/UnrealMathUtility.h"

void SQuotaBar::Construct(const FArguments& InArgs)
{
	Percent = InArgs._Percent;

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::Get().GetBrush("ToolPanel.GroupBorder"))
		.BorderBackgroundColor(FLinearColor(0.1f, 0.1f, 0.1f, 1.0f))
		.Padding(FMargin(2, 0))
		[
			SNew(SOverlay)
			+ SOverlay::Slot()
			[
				SNew(SProgressBar)
				.Percent(Percent)
				.FillColorAndOpacity(FLinearColor(0.2f, 0.6f, 0.2f, 0.8f))
				.BackgroundImage(FAppStyle::Get().GetBrush("NoBorder"))
			]
			+ SOverlay::Slot()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateSP(this, &SQuotaBar::GetQuotaText)))
				.Font(FAppStyle::GetFontStyle("NormalFontBold"))
				.ColorAndOpacity(FLinearColor::White)
				.ShadowOffset(FVector2D(1, 1))
			]
		]
	];
}

FText SQuotaBar::GetQuotaText() const
{
	const float Value = Percent.Get().Get(0.0f);
	return FText::Format(FText::FromString(TEXT("Quota: {0}%")), FText::AsNumber(FMath::RoundToInt(Value * 100)));
}
