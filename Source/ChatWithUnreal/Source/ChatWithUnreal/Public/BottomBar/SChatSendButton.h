// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

/**
 * SChatSendButton
 */
class CHATWITHUNREAL_API SChatSendButton : public SCompoundWidget
{
public:
	static TWeakPtr<SChatSendButton> Instance;

public:
	SLATE_BEGIN_ARGS(SChatSendButton) {}
		SLATE_EVENT(FSimpleDelegate, OnSendClicked)
		SLATE_EVENT(FSimpleDelegate, OnInterruptClicked)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	void SetIsRunning(bool bRunning);
	bool IsRunning() const;

	FSimpleDelegate OnSendClicked;
	FSimpleDelegate OnInterruptClicked;

private:
	FReply HandleOnClicked();
	FText GetButtonText() const;
	FSlateColor GetButtonBackgroundColor() const;
	FSlateColor GetButtonColor() const;

	bool bIsRunning = false;
	bool bInterruptConfirmArmed = false;
	FDateTime InterruptConfirmExpireAt;
};
