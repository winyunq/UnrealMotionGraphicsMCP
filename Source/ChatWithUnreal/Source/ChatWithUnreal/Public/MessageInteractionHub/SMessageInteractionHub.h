// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

class SScrollBox;
class SVerticalBox;
class SChatWelcome;
class SAgentResponseGroup;

DECLARE_DELEGATE_OneParam(FOnMcpSessionSelected, const FString& /*SessionId*/);
DECLARE_DELEGATE_OneParam(FOnMcpSessionDeleted, const FString& /*SessionId*/);

/**
 * SMessageInteractionHub：交互中枢。
 * 负责在欢迎页与消息列表之间切换，并暴露对话重演接口供后端驱动。
 */
class CHATWITHUNREAL_API SMessageInteractionHub : public SCompoundWidget
{
public:
	static TWeakPtr<SMessageInteractionHub> Instance;

public:
	SLATE_BEGIN_ARGS(SMessageInteractionHub) {}
		SLATE_EVENT(FOnMcpSessionSelected, OnSessionSelected)
		SLATE_EVENT(FOnMcpSessionDeleted, OnSessionDeleted)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** --- 底层 Widget 容器接口 (BE -> FE) --- */

	/** 清空屏幕 */
	void ClearMessages();

	/** 刷新布局显隐 */
	void RefreshVisibility();

	/** 向列表追加一个原始 Widget */
	void AddMessageWidget(TSharedRef<SWidget> InWidget);
	void MoveWidgetToBottom(TSharedRef<SWidget> InWidget);
	void RemoveMessageWidget(TSharedRef<SWidget> InWidget);
	void RestoreHistoryMessage(const FString& AgentName, const FString& Content, const TArray<FString>& Base64Images, bool bIsUser);

	TSharedPtr<SChatWelcome> WelcomeWidget;
	FOnMcpSessionSelected OnSessionSelectedEvent;
	FOnMcpSessionDeleted OnSessionDeletedEvent;

private:


	TSharedPtr<SVerticalBox> MessageList;
	TSharedPtr<SScrollBox> ScrollBoxWidget;
	TSharedPtr<class SBorder> MessageListBorder;
};
