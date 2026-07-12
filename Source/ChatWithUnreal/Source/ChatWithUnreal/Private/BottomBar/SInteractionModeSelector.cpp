// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#include "SInteractionModeSelector.h"
#include "Internationalization/Culture.h"
#include "Internationalization/Internationalization.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBorder.h"
#include "Styling/AppStyle.h"
#include "Editor.h"

namespace
{
	FText GetLocText(const FString& English, const FString& Chinese)
	{
		const FString Culture = FInternationalization::Get().GetCurrentCulture()->GetName();
		return FText::FromString(Culture.StartsWith(TEXT("zh")) ? Chinese : English);
	}

	FText GetModeDisplayText(const FString& ModeKey)
	{
		if (ModeKey == TEXT("Chat")) return GetLocText(TEXT("Chat"), TEXT("聊天"));
		if (ModeKey == TEXT("Learning")) return GetLocText(TEXT("Learning"), TEXT("学习"));
		if (ModeKey == TEXT("Develop")) return GetLocText(TEXT("Develop"), TEXT("开发"));
		if (ModeKey == TEXT("Task")) return GetLocText(TEXT("Task"), TEXT("任务"));
		return FText::FromString(ModeKey);
	}
}

SInteractionModeSelector::~SInteractionModeSelector()
{
	GConfig->SetString(TEXT("UmgMcp.UIState"), TEXT("LastSelectedMode"), *CurrentMode, GEditorPerProjectIni);
}

void SInteractionModeSelector::Construct(const FArguments& InArgs)
{
	CurrentMode = TEXT("Chat");
	OnInteractionModeChangedEvent = InArgs._OnInteractionModeChanged;

	Options = {
		MakeShared<FString>(TEXT("Chat")),
		MakeShared<FString>(TEXT("Learning")),
		MakeShared<FString>(TEXT("Develop")),
		MakeShared<FString>(TEXT("Task"))
	};
	TSharedPtr<FString> InitialSelection = Options.Num() > 0 ? Options[0] : nullptr;
	FString SavedMode;
	if (GConfig->GetString(TEXT("UmgMcp.UIState"), TEXT("LastSelectedMode"), SavedMode, GEditorPerProjectIni))
	{
		for (const TSharedPtr<FString>& Option : Options)
		{
			if (Option.IsValid() && Option->Equals(SavedMode, ESearchCase::IgnoreCase))
			{
				InitialSelection = Option;
				CurrentMode = *Option;
				break;
			}
		}
	}

	ChildSlot
	[
		SAssignNew(ComboBox, SComboBox<TSharedPtr<FString>>)
		.OptionsSource(&Options)
		.OnGenerateWidget(this, &SInteractionModeSelector::MakeOptionWidget)
		.OnSelectionChanged(this, &SInteractionModeSelector::OnSelectionChanged)
		.InitiallySelectedItem(InitialSelection)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::Get().GetBrush("ToolPanel.GroupBorder"))
			.BorderBackgroundColor(FLinearColor(0.15f, 0.15f, 0.15f, 1.0f))
			.Padding(FMargin(8, 2))
			[
				SNew(STextBlock)
				.Text(this, &SInteractionModeSelector::GetModeDisplayTextAttr)
				.Font(FAppStyle::Get().GetFontStyle("NormalFontBold"))
			]
		]
	];

	OnInteractionModeChangedEvent.ExecuteIfBound(CurrentMode);
}

FText SInteractionModeSelector::GetModeDisplayTextAttr() const
{
	return GetModeDisplayText(CurrentMode);
}

TSharedRef<SWidget> SInteractionModeSelector::MakeOptionWidget(TSharedPtr<FString> Option)
{
	return SNew(STextBlock)
		.Text(GetModeDisplayText(*Option))
		.Margin(FMargin(8, 4));
}

void SInteractionModeSelector::OnSelectionChanged(TSharedPtr<FString> NewValue, ESelectInfo::Type SelectInfo)
{
	if (NewValue.IsValid())
	{
		CurrentMode = *NewValue;
		
		GConfig->SetString(TEXT("UmgMcp.UIState"), TEXT("LastSelectedMode"), *CurrentMode, GEditorPerProjectIni);
		GConfig->Flush(false, GEditorPerProjectIni);

		OnInteractionModeChangedEvent.ExecuteIfBound(CurrentMode);
	}
}

void SInteractionModeSelector::SetCurrentModeDirect(const FString& NewMode)
{
	for (const auto& Opt : Options)
	{
		if (Opt.IsValid() && *Opt == NewMode)
		{
			ComboBox->SetSelectedItem(Opt);
			CurrentMode = NewMode;
			break;
		}
	}
}
