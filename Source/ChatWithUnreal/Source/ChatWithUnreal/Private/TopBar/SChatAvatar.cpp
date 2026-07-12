// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#include "TopBar/SChatAvatar.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "ChatWithUnrealStyle.h"

void SAvatar::Construct(const FArguments& InArgs)
{
	ChildSlot
	[
		SAssignNew(ComboButton, SComboButton)
		.HasDownArrow(false)
		.ButtonStyle(FAppStyle::Get(), "NoBorder")
		.ContentPadding(0.0f)
		.OnGetMenuContent(this, &SAvatar::OnGetMenuContent)
		.ButtonContent()
		[
			SNew(SBox)
			.WidthOverride(AvatarSize)
			.HeightOverride(AvatarSize)
			[
				SNew(SImage)
				.Image(this, &SAvatar::GetAvatarBrush)
			]
		]
	];
}

const FSlateBrush* SAvatar::GetSourceBrush() const
{
	return SourceBrush ? SourceBrush : FAppStyle::Get().GetBrush("Icons.User");
}

const FSlateBrush* SAvatar::GetAvatarBrush() const
{
	const FSlateBrush* EffectiveSource = GetSourceBrush();
	if (!EffectiveSource)
	{
		EffectiveSource = FChatWithUnrealStyle::Get().GetBrush("ChatWithUnreal.Agent.Agent");
	}

	if (!EffectiveSource)
	{
		return nullptr;
	}

	if (!CachedRoundedAvatarBrush.IsValid())
	{
		CachedRoundedAvatarBrush = MakeShared<FSlateRoundedBoxBrush>(
			EffectiveSource->GetResourceName(),
			FLinearColor::White,
			FVector4(AvatarSize * 0.5f, AvatarSize * 0.5f, AvatarSize * 0.5f, AvatarSize * 0.5f),
			FLinearColor::Transparent,
			0.0f,
			FVector2D(AvatarSize, AvatarSize)
		);
	}

	return CachedRoundedAvatarBrush.Get();
}


TOptional<FSlateRenderTransform> SAvatar::GetAvatarRenderTransform() const
{
	return FSlateRenderTransform(FVector2D(0.5f, 0.5f));
}
