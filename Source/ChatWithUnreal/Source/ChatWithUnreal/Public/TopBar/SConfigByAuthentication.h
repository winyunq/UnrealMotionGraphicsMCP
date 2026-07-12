// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class CHATWITHUNREAL_API SConfigByAuthentication : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SConfigByAuthentication) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
};
