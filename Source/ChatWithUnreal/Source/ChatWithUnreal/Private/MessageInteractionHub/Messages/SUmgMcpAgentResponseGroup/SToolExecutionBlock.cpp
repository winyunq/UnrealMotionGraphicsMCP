// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#include "SToolExecutionBlock.h"

#include "Styling/AppStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"

void SToolExecutionBlock::Construct(const FArguments& InArgs)
{
	const FLinearColor Color = InArgs._IsError ? FLinearColor(0.9f, 0.3f, 0.3f, 1.0f) : FLinearColor(0.3f, 0.8f, 0.4f, 1.0f);
	const FString Label = FString::Printf(TEXT("Tool %s: %s"), *InArgs._ToolName, *InArgs._Status);

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::Get().GetBrush("ToolPanel.GroupBorder"))
		.Padding(FMargin(8.0f))
		[
			SNew(STextBlock)
			.Text(FText::FromString(Label))
			.ColorAndOpacity(Color)
		]
	];
}
