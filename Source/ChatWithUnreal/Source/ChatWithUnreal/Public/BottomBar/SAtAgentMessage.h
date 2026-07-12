// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

/**
 * SAtAgentMessage: @智能体状态展示与清除组件。
 * 
 * 职责说明：
 * 1. 负责在前端展示和清除选中的 @智能体 状态。
 * 2. 只有当选中了具体的智能体或被主动清空时（UpdateAgent 接口被调用时），才会改变折叠状态。
 * 3. 输入 @ 时的联想提示阶段不改变本控件的折叠状态。
 */
class CHATWITHUNREAL_API SAtAgentMessage : public SCompoundWidget
{
public:
	/** 全局静态弱指针，便于同级控件（如输入框）直接访问 */
	static TWeakPtr<SAtAgentMessage> Instance;

public:
	SLATE_BEGIN_ARGS(SAtAgentMessage) {}
	SLATE_END_ARGS()

	/** 构造前端控件，初始化默认为 Collapsed 折叠状态 */
	void Construct(const FArguments& InArgs);

	/** 
	 * 确定选中或清除 @智能体 的唯一设置接口。
	 * 只有该接口传入有效智能体时，控件才变为 SelfHitTestInvisible（不折叠展示状态）；
	 * 传入空字符串时，控件重置并 Collapsed（折叠隐藏状态）。
	 */
	void UpdateAgent(const FString& InAgentName, const struct FSlateBrush* InBrush);
	
	/** 获取当前被选中/At 的智能体名字 */
	FString GetAgentName() const { return AgentName; }

	/** 展现输入联想建议列表菜单（与 @agent 折叠展现为不同的两个概念） */
	void ShowSuggestionsMenu(const TArray<FString>& InSuggestions);
	
	/** 关闭联想建议列表菜单 */
	void CloseSuggestionsMenu();

private:
	/** 点击清除按钮（×）时的回调，内部直接调用 UpdateAgent(TEXT(""), nullptr) 触发清除并折叠 */
	FReply OnClearBtnClicked();

	/** 联想菜单内容生成回调 */
	TSharedRef<SWidget> OnGenerateAgentMenu();
	
	/** 联想建议列表行生成回调 */
	TSharedRef<ITableRow> OnGenerateAgentRow(TSharedPtr<FString> Item, const TSharedRef<STableViewBase>& OwnerTable);
	
	/** 用户选中了联想建议列表中的某个智能体时的回调，内部调用 UpdateAgent 确定选中 */
	void OnAgentSelected(TSharedPtr<FString> SelectedAgent, ESelectInfo::Type SelectInfo);
	
	/** 控制智能体标志框 SBox 显示折叠状态的内部绑定回调 */
	EVisibility GetAgentBoxVisibility() const;
	
	/** 动态获取当前选中的智能体头像 Brush 的内部绑定回调 */
	const struct FSlateBrush* GetAgentBrush() const;
	
	/** 动态获取当前选中的智能体名称文本的内部绑定回调 */
	FText GetAgentText() const;

	/** UI 控件及数据维护成员 */
	TSharedPtr<class SMenuAnchor> AgentMenuAnchor;
	TSharedPtr<class SListView<TSharedPtr<FString>>> AgentListView;
	TArray<TSharedPtr<FString>> AgentSuggestions;

	FString AgentName;
	const struct FSlateBrush* AgentBrush = nullptr;
};
