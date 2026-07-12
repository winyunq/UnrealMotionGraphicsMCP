// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Styling/SlateBrush.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

/**
 * SAvatar：基类头像组件
 */
class CHATWITHUNREAL_API SAvatar : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAvatar) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	// 获取菜单内容（由派生类实现）
	virtual TSharedRef<SWidget> OnGetMenuContent() = 0;

protected:
	const FSlateBrush* GetSourceBrush() const;
	const FSlateBrush* GetAvatarBrush() const;
	TOptional<FSlateRenderTransform> GetAvatarRenderTransform() const;

	const float AvatarSize = 40.0f;
	const FSlateBrush* SourceBrush = nullptr;

	TSharedPtr<class SComboButton> ComboButton;
	mutable TSharedPtr<struct FSlateRoundedBoxBrush> CachedRoundedAvatarBrush;
};
