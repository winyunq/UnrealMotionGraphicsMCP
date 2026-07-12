// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

class CHATWITHUNREAL_API SChatWelcomeHeader : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SChatWelcomeHeader) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
};
