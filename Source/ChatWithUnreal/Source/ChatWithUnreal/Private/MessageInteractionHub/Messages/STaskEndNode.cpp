// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#include "Messages/STaskEndNode.h"

#include "Styling/AppStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"

void STaskEndNode::Construct(const FArguments& InArgs)
{
	const FString TaskId = InArgs._TaskId;
	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::Get().GetBrush("ToolPanel.DarkGroupBorder"))
		.Padding(FMargin(8.0f))
		[
			SNew(STextBlock)
			.Text(FText::FromString(FString::Printf(TEXT("task_end: %s"), *TaskId)))
		]
	];
}
