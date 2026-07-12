// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#include "SEditableName.h"
#include "Widgets/Input/SEditableText.h"
#include "Styling/AppStyle.h"

void SEditableName::Construct(const FArguments& InArgs)
{
	ChildSlot
	[
		SAssignNew(NameInput, SEditableText)
		.Font(FAppStyle::Get().GetFontStyle("NormalFontBold"))
		.ColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, 0.8f))
		.Text(FText::FromString(TEXT("Local User")))
	];
}

void SEditableName::SetUserMetadata(const FString& InName, bool bInIsLoggedIn)
{
	bIsLoggedIn = bInIsLoggedIn;
	if (NameInput.IsValid())
	{
		NameInput->SetText(FText::FromString(InName));
		// 如果已登录，稍微调亮一点名字
		NameInput->SetColorAndOpacity(bIsLoggedIn ? FLinearColor::White : FLinearColor(1.0f, 1.0f, 1.0f, 0.6f));
	}
}
