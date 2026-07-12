// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#include "SUserAvatar.h"
#include "ChatWithUnrealStyle.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Internationalization/Culture.h"
#include "Internationalization/Internationalization.h"

namespace
{
	FText GetLocText(const FString& English, const FString& Chinese)
	{
		const FString Culture = FInternationalization::Get().GetCurrentCulture()->GetName();
		return FText::FromString(Culture.StartsWith(TEXT("zh")) ? Chinese : English);
	}
}

void SUserAvatar::Construct(const FArguments& InArgs)
{
	SourceBrush = FChatWithUnrealStyle::Get().GetBrush("ChatWithUnreal.ChatAvatar");
	SAvatar::Construct(SAvatar::FArguments());
}

TSharedRef<SWidget> SUserAvatar::OnGetMenuContent()
{
	FMenuBuilder MenuBuilder(true, nullptr);

	MenuBuilder.BeginSection("Authentication", GetLocText(TEXT("Login"), TEXT("登录")));
	{
		MenuBuilder.AddMenuEntry(
			GetLocText(TEXT("Login with Google"), TEXT("使用 Google 登录")),
			FText::GetEmpty(),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateLambda([]() {
				// TODO: 触发登录逻辑
			}))
		);
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}
