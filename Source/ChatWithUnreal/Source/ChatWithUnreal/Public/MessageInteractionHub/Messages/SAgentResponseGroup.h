// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

/**
 * SAgentResponseGroup
 * AI 消息表现层：实现经典的左侧对齐气泡，集成头像与回复文本。
 */
class CHATWITHUNREAL_API SAgentResponseGroup : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAgentResponseGroup) {}
		SLATE_ARGUMENT(FString, AgentName)
		SLATE_ARGUMENT(FString, MessageText)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	
	// 设置状态区内容
	void SetAgentStatus(const FText& StatusText, bool bShowSpinner, const FLinearColor& StatusColor = FLinearColor::White);
	// 设置名字内容
	void SetAgentName(const FString& InName);

	/** 判断此组件是否为白纸（无文本且无工具块） */
	bool IsEmpty() const;

	FString GetAgentName() const;
	FString GetMessageText() const { return MessageText; }
	void ClearMessages();

	/** 将一个复杂的工具请求组控件（如 SMcpApprovalQueue）添加到消息气泡中 */
	void AddToolExecutionWidget(TSharedRef<SWidget> ToolWidget);
	void RemoveWidget(TSharedRef<SWidget> WidgetToRemove);

	void AddTextOutputBlock(const FString& NewText);
	void AppendToCurrentTextOutputBlock(const FString& PartialText);
	void AddToolExecutionBlock(const FString& ToolName, const FText& Status, bool bIsError);

private:
	FString AgentName;
	FString MessageText;
	TSharedPtr<class SAgentStatusBar> AgentStatusBarWidget;
	TSharedPtr<class STextBlock> AgentNameTextBlock;
	TSharedPtr<class SVerticalBox> TurnContentBox;

	// pointer to the latest active text block to append text directly
	TSharedPtr<class SRichTextBlock> ActiveTextBlock;
};
