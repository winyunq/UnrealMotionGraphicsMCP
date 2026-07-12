// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#include "SChatModeLabel.h"

#include "Internationalization/Culture.h"
#include "Internationalization/Internationalization.h"
#include "Widgets/Text/STextBlock.h"

void SChatModeLabel::Construct(const FArguments& InArgs)
{
	ModeText = InArgs._ModeText;

	ChildSlot
	[
		SNew(STextBlock)
		.Text(this, &SChatModeLabel::GetModeDisplayText)
	];
}

FText SChatModeLabel::GetModeDisplayText() const
{
	const FString CurrentText = ModeText.Get(FString());
	if (CurrentText.IsEmpty())
	{
		return FText::GetEmpty();
	}

	if (CurrentText == TEXT("Chat")) return GetLocText(TEXT("Chat"), TEXT("聊天"));
	if (CurrentText == TEXT("Learning")) return GetLocText(TEXT("Learning"), TEXT("学习"));
	if (CurrentText == TEXT("Develop")) return GetLocText(TEXT("Develop"), TEXT("开发"));
	if (CurrentText == TEXT("Task")) return GetLocText(TEXT("Task"), TEXT("任务"));
	return FText::FromString(CurrentText);
}

FText SChatModeLabel::GetLocText(const FString& English, const FString& Chinese) const
{
	const FString Culture = FInternationalization::Get().GetCurrentCulture()->GetName();
	return FText::FromString(Culture.StartsWith(TEXT("zh")) ? Chinese : English);
}
