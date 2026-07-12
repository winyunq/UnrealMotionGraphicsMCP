// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#pragma once

#include "TopBar/SChatAvatar.h"

// 用户头像派生类：点击弹出登录/登出面板
class CHATWITHUNREAL_API SUserAvatar : public SAvatar
{
public:
	SLATE_BEGIN_ARGS(SUserAvatar) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	// 实现菜单内容：登录选项
	virtual TSharedRef<SWidget> OnGetMenuContent() override;
};
