// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

class SUserAvatar;
class SEditableName;

/**
 * STopBar：顶部栏容器。
 * 纯展示层，由后端驱动。
 */
class CHATWITHUNREAL_API STopBar : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(STopBar) {}
		SLATE_EVENT(FSimpleDelegate, OnNewConversation)
		SLATE_EVENT(FSimpleDelegate, OnShowHistory)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** 还原原始接口：设置用户信息 */
	void SetUserMetadata(const FString& UserName, bool bIsLoggedIn);

private:
	FReply OnNewConversationClicked();
	FReply OnShowHistoryClicked();

	FSimpleDelegate OnNewConversationDelegate;
	FSimpleDelegate OnShowHistoryDelegate;

	TSharedPtr<SUserAvatar> AvatarWidget;
	TSharedPtr<SEditableName> NameWidget;
};
