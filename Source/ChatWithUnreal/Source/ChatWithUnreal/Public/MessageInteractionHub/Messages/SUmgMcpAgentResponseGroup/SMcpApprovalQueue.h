// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Dom/JsonObject.h"

class SMcpToolItem;

DECLARE_DELEGATE_RetVal_ThreeParams(FString, FOnExecuteTool, const FString& /*ToolName*/, const FString& /*ArgumentsJson*/, FString& /*OutToolCallId*/);
DECLARE_DELEGATE_OneParam(FOnMcpTaskFinished, const TArray<TSharedPtr<FJsonObject>>& /*Results*/);

/**
 * SMcpApprovalQueue: MCP 审批队列控件。
 */
class CHATWITHUNREAL_API SMcpApprovalQueue : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMcpApprovalQueue) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** 添加一个工具请求到队列末尾 */
	void AddToolRequest(const FString& ToolName, const FString& ArgumentsJson);

	/** 呈现第一个审批按钮（定界启动） */
	void PresentFirstApprovalButtons();

	/** 判定队列是否为空（逻辑上是否有请求） */
	bool IsQueueEmpty() const { return ToolItems.Num() == 0; }

	/** 已收集的执行结果 */
	TArray<TSharedPtr<FJsonObject>> GetCollectedResults() const { return CollectedResults; }

public: // Made public for direct backend wiring
	FOnExecuteTool OnExecuteTool;
	FSimpleDelegate OnToolRejected;
	FOnMcpTaskFinished OnMcpTaskFinished;

private:
	/** 当前焦点索引（待处理的第一个 Item） */
	int32 CurrentFocusIndex = 0;

	/** 工具请求项列表 */
	TArray<TSharedPtr<SMcpToolItem>> ToolItems;

	/** 已收集的执行结果 */
	TArray<TSharedPtr<FJsonObject>> CollectedResults;

	/** 容器布局 */
	TSharedPtr<class SVerticalBox> ListContainer;

	// --- 交互执行节点 ---
	void RenderActiveButtons();
	FReply OnAcceptClicked();
	FReply OnRejectClicked();
	void AdvanceFocus();
};
