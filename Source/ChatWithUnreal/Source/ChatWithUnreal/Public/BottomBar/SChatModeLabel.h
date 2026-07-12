// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

class CHATWITHUNREAL_API SChatModeLabel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SChatModeLabel) {}
		SLATE_ATTRIBUTE(FString, ModeText)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	FText GetModeDisplayText() const;
	FText GetLocText(const FString& English, const FString& Chinese) const;

	TAttribute<FString> ModeText;
};
