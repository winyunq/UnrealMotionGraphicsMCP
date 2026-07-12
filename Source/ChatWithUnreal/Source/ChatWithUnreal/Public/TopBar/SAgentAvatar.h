// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#pragma once

#include "TopBar/SChatAvatar.h"

// AI 头像派生类：点击弹出 @提及 面板
class CHATWITHUNREAL_API SAgentAvatar : public SAvatar
{
public:
	SLATE_BEGIN_ARGS(SAgentAvatar) {}
		SLATE_ARGUMENT(FString, AgentName)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	// 实现菜单内容：@提及选项
	virtual TSharedRef<SWidget> OnGetMenuContent() override;

private:
	FString AgentName;
};
