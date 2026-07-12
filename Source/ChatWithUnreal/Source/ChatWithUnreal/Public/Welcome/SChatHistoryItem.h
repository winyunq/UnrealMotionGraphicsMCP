// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

// Forward declaration of the struct defined in SChatWelcome.h
struct FChatSessionData;

DECLARE_DELEGATE_OneParam(FOnChatHistoryItemClicked, const FString&);
DECLARE_DELEGATE_OneParam(FOnChatHistoryItemDeleted, const FString&);

/**
 * SChatHistoryItem
 */
class CHATWITHUNREAL_API SChatHistoryItem : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SChatHistoryItem) {}
		SLATE_ARGUMENT(FString, SessionId)
		SLATE_ARGUMENT(FString, SessionName)
		SLATE_EVENT(FOnChatHistoryItemClicked, OnSelected)
		SLATE_EVENT(FOnChatHistoryItemDeleted, OnDeleted)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	FReply HandleClicked();
	FReply HandleDeleteClicked();

	FString SessionId;
	FString SessionName;
	FOnChatHistoryItemClicked OnSelectedDelegate;
	FOnChatHistoryItemDeleted OnDeletedDelegate;
};
