// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

/**
 * SMcpToolItem: 单个 MCP 工具请求的 UI 项。
 * 负责展示工具名、参数详情及执行状态。
 */
class CHATWITHUNREAL_API SMcpToolItem : public SCompoundWidget
{
public:
	enum class EToolStatus
	{
		Pending,
		Running,
		Success,
		Failed,
		Rejected
	};

	SLATE_BEGIN_ARGS(SMcpToolItem) {}
		SLATE_ARGUMENT(FString, ToolName)
		SLATE_ARGUMENT(FString, ArgumentsJson)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** 更新当前工具的执行状态 */
	void SetStatus(EToolStatus InStatus, const FString& ErrorMessage = TEXT(""), const FString& InResultString = TEXT(""));

	/** 获取工具名 */
	FString GetToolName() const { return ToolName; }
	/** 获取参数 JSON */
	FString GetArgumentsJson() const { return ArgumentsJson; }

private:
	/** 重新构建详情内容框（入参或结果） */
	void RebuildDetailsBox();

	/** 根据 JSON 动态创建精致的键值对表格或者代码块 */
	TSharedRef<SWidget> CreateKeyValueTable(const FString& InJson, bool bIsResult);

	/** 点击折叠/展开按钮时的回调 */
	FReply OnToggleDetailsClicked();

private:
	FString ToolName;
	FString ArgumentsJson;
	FString ResultString;
	EToolStatus CurrentStatus = EToolStatus::Pending;
	bool bIsExpanded = false;

	TSharedPtr<class STextBlock> StatusTextBlock;
	TSharedPtr<class SVerticalBox> DetailsBox;
	TSharedPtr<class STextBlock> ToggleButtonTextBlock;
};
