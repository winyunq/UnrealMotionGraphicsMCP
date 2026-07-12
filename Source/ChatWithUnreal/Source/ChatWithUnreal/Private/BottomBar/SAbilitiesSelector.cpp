// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#include "SAbilitiesSelector.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
SAbilitiesSelector::~SAbilitiesSelector()
{
}

void SAbilitiesSelector::Construct(const FArguments& InArgs)
{
	OnGetInteractionMode = InArgs._OnGetInteractionMode;

	ChildSlot
	.VAlign(VAlign_Center)
	[
		SAssignNew(ComboButton, SComboButton)
		.Visibility(this, &SAbilitiesSelector::GetVisibilityBasedOnMode)
		.ButtonStyle(FAppStyle::Get(), "SimpleButton")
		.HasDownArrow(false)
		.ContentPadding(0.0f)
		.ButtonContent()
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::Get().GetBrush("ToolPanel.GroupBorder"))
			.BorderBackgroundColor(FLinearColor(0.15f, 0.15f, 0.15f, 1.0f))
			.Padding(FMargin(8, 2))
			[
				SNew(STextBlock)
				.Text(this, &SAbilitiesSelector::GetDisplayAlphaText)
				.Font(FAppStyle::Get().GetFontStyle("NormalFontBold"))
			]
		]
		.MenuContent()
		[
			CreateAbilitiesMenuContent()
		]
	];
}

FText SAbilitiesSelector::GetDisplayAlphaText() const
{
	return FText::FromString(TEXT("Abilities (2)"));
}

EVisibility SAbilitiesSelector::GetVisibilityBasedOnMode() const
{
	if (OnGetInteractionMode.IsBound())
	{
		return OnGetInteractionMode.Execute() == TEXT("Task") ? EVisibility::Visible : EVisibility::Collapsed;
	}
	return EVisibility::Collapsed;
}

TSharedRef<SWidget> SAbilitiesSelector::CreateAbilitiesMenuContent()
{
	return SNew(SBox)
		.WidthOverride(300.0f)
		.MaxDesiredHeight(400.0f)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::Get().GetBrush("Menu.Background"))
			.Padding(FMargin(8.0f))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(4)
				[
					SNew(STextBlock)
					.Text(FText::FromString(TEXT("Editor Tools")))
					.Font(FAppStyle::Get().GetFontStyle("NormalFontBold"))
					.ColorAndOpacity(FLinearColor::Gray)
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(8, 4)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[ SNew(SCheckBox).IsChecked(ECheckBoxState::Checked) ]
					+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(8, 0, 0, 0)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()[ SNew(STextBlock).Text(FText::FromString(TEXT("🛠️ UMG Layout Master"))) ]
						+ SVerticalBox::Slot().AutoHeight()[ SNew(STextBlock).Text(FText::FromString(TEXT("UI Layout editing."))).Font(FAppStyle::Get().GetFontStyle("SmallFont")).ColorAndOpacity(FLinearColor(0.5f, 0.5f, 0.5f)) ]
					]
				]
			]
		];
}
