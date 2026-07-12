// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class CHATWITHUNREAL_API SToolExecutionBlock : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SToolExecutionBlock) {}
		SLATE_ARGUMENT(FString, ToolName)
		SLATE_ARGUMENT(FString, Status)
		SLATE_ARGUMENT(bool, IsError)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
};
