// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#include "SAgentAvatar.h"
#include "ChatWithUnrealStyle.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Internationalization/Culture.h"
#include "BottomBar/SAtAgentMessage.h"
#include "BottomBar/SChatInput.h"

namespace
{
	FText GetLocText(const FString& English, const FString& Chinese)
	{
		const FString Culture = FInternationalization::Get().GetCurrentCulture()->GetName();
		return FText::FromString(Culture.StartsWith(TEXT("zh")) ? Chinese : English);
	}
}

void SAgentAvatar::Construct(const FArguments& InArgs)
{
	AgentName = InArgs._AgentName;
	
	const FSlateBrush* TargetBrush = FChatWithUnrealStyle::Get().GetOptionalBrush(FName(*(TEXT("ChatWithUnreal.Agent.") + AgentName)), nullptr, nullptr);
	if (!TargetBrush)
	{
		TargetBrush = FChatWithUnrealStyle::Get().GetBrush("ChatWithUnreal.Agent.Agent");
	}

	SourceBrush = TargetBrush;
	SAvatar::Construct(SAvatar::FArguments());
}

TSharedRef<SWidget> SAgentAvatar::OnGetMenuContent()
{
	FMenuBuilder MenuBuilder(true, nullptr);

	MenuBuilder.BeginSection("AgentActions", GetLocText(TEXT("Agent Actions"), TEXT("Agent 操作")));
	{
		const FString MentionText = FString::Printf(TEXT("@%s"), *AgentName);
		MenuBuilder.AddMenuEntry(
			FText::Format(GetLocText(TEXT("Mention {0}"), TEXT("提及 {0}")), FText::FromString(MentionText)),
			FText::GetEmpty(),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateLambda([MentionText]() {
				FString CleanAgentName = MentionText;
				if (CleanAgentName.StartsWith(TEXT("@")))
				{
					CleanAgentName.RemoveAt(0, 1);
				}
				
				if (auto AtAgentMsg = SAtAgentMessage::Instance.Pin())
				{
					const FSlateBrush* Brush = FChatWithUnrealStyle::Get().GetOptionalBrush(FName(*(TEXT("ChatWithUnreal.Agent.") + CleanAgentName)), nullptr, nullptr);
					if (!Brush)
					{
						Brush = FChatWithUnrealStyle::Get().GetBrush("ChatWithUnreal.Agent.Agent");
					}
					AtAgentMsg->UpdateAgent(CleanAgentName, Brush);
				}

				if (auto ChatInput = SChatInput::Instance.Pin())
				{
					FString CurrentText = ChatInput->GetText().ToString();
					int32 AtIndex = INDEX_NONE;
					if (CurrentText.FindLastChar(TEXT('@'), AtIndex))
					{
						CurrentText.LeftInline(AtIndex);
					}
					ChatInput->SetText(FText::FromString(CurrentText));
					ChatInput->FocusInput();
				}
			}))
		);
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}
