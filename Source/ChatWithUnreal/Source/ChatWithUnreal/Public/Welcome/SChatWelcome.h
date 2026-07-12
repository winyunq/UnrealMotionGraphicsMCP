// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

DECLARE_DELEGATE_OneParam(FOnChatWelcomeSessionSelected, const FString& /*SessionId*/);
DECLARE_DELEGATE_OneParam(FOnChatWelcomeSessionDeleted, const FString& /*SessionId*/);

/**
 * SChatWelcome：欢迎页，显示引导信息和历史列表。
 */
class CHATWITHUNREAL_API SChatWelcome : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SChatWelcome) {}
		SLATE_EVENT(FOnChatWelcomeSessionSelected, OnSessionSelected)
		SLATE_EVENT(FOnChatWelcomeSessionDeleted, OnSessionDeleted)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** 协议接口：由后端调用，直接传入基础数据 */
	void ClearHistoryList();
	void AddHistoryItem(const FString& SessionId, const FString& Title, int32 MessageCount, const FDateTime& LastModified);

private:
	void OnHistoryItemSelected(const FString& SessionId);
	void OnHistoryItemDeleted(const FString& SessionId);
	EVisibility GetEmptyListVisibility() const;

	TSharedPtr<class SVerticalBox> HistoryListBox;

	FOnChatWelcomeSessionSelected OnSessionSelectedEvent;
	FOnChatWelcomeSessionDeleted OnSessionDeletedEvent;
};
