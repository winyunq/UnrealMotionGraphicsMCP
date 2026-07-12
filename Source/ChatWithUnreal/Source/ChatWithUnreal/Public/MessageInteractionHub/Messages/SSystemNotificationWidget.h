// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

/**
 * SSystemNotificationWidget
 * 系统消息表现层：实现居中的通知文本，支持错误重试按钮。
 */
class CHATWITHUNREAL_API SSystemNotificationWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SSystemNotificationWidget) {}
		SLATE_ARGUMENT(FString, MessageText)
		SLATE_ARGUMENT(bool, IsError)
		SLATE_EVENT(FSimpleDelegate, OnRetry)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	FString MessageText;
	bool bIsError = false;
	FSimpleDelegate OnRetryDelegate;
	TSharedPtr<class SButton> RetryButton;

	FReply OnRetryClicked();
};
