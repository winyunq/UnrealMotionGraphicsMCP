// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#include "SConfigByAuthentication.h"

#include "Editor.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Text/STextBlock.h"

void SConfigByAuthentication::Construct(const FArguments& InArgs)
{
	ChildSlot
	[
		SNew(STextBlock)
		.Text(FText::FromString(TEXT("Provider")))
	];
}
