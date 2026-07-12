// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

class SEditableText;

/**
 * SEditableName：纯粹的名字展示与编辑组件。
 */
class CHATWITHUNREAL_API SEditableName : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SEditableName) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** 还原原始接口：设置用户信息 */
	void SetUserMetadata(const FString& InName, bool bInIsLoggedIn);

private:
	TSharedPtr<SEditableText> NameInput;
	bool bIsLoggedIn = false;
};
