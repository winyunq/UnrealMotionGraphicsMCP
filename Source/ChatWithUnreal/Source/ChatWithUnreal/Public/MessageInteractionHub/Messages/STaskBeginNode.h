// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class CHATWITHUNREAL_API STaskBeginNode : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(STaskBeginNode) {}
		SLATE_ARGUMENT(FString, TaskId)
		SLATE_ARGUMENT(FString, InitiatorAgent)
		SLATE_ARGUMENT(FString, ReceiverAgent)
		SLATE_ARGUMENT(FString, TargetAsset)
		SLATE_ARGUMENT(TArray<FString>, Items)
		SLATE_ARGUMENT(FString, Status)
		SLATE_EVENT(FSimpleDelegate, OnAccepted)
		SLATE_EVENT(FSimpleDelegate, OnRejected)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	void SetOnAccepted(const FSimpleDelegate& InDelegate) { OnAccepted = InDelegate; }
	void SetOnRejected(const FSimpleDelegate& InDelegate) { OnRejected = InDelegate; }
	void SetTargetAsset(const FString& InTargetAsset);

private:
	FString TaskId;
	FString InitiatorAgent;
	FString ReceiverAgent;
	FString TargetAsset;
	TArray<FString> Items;
	TArray<FString> ItemFeedbacks;
	FString Status;
	FSimpleDelegate OnAccepted;
	FSimpleDelegate OnRejected;

	FText GetTargetAssetHyperlinkText() const;

	FReply OnAcceptClicked();
	FReply OnRejectClicked();
	void OnHyperlinkNavigate();

	EVisibility GetAcceptButtonVisibility() const;
	EVisibility GetStatusTextVisibility() const;
	FText GetStatusText() const;
	FSlateColor GetStatusTextColor() const;

	FString FormatTaskItemSingleLine(const FString& InText);
};
