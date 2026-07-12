// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

/**
 * SUserMessageWidget
 * 用户消息表现层：实现经典的右侧对齐布局，包含气泡和头像。
 */
class CHATWITHUNREAL_API SUserMessageWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SUserMessageWidget) {}
		SLATE_ARGUMENT(FString, MessageText)
		SLATE_ARGUMENT(TArray<FString>, Base64Images)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	~SUserMessageWidget();

private:
	FString MessageText;
	TArray<FString> Base64Images;
	TArray<TSharedPtr<struct FSlateDynamicImageBrush>> AttachedSlateBrushes;
};
