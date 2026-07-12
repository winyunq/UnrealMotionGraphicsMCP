#include "BottomBar/SAtAgentMessage.h"
#include "TopBar/SAgentAvatar.h"
#include "BottomBar/SChatInput.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SMenuAnchor.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"
#include "Styling/AppStyle.h"
#include "ChatWithUnrealStyle.h"

TWeakPtr<SAtAgentMessage> SAtAgentMessage::Instance = nullptr;

void SAtAgentMessage::Construct(const FArguments& InArgs)
{
	Instance = SharedThis(this);

	ChildSlot
	[
		SNew(SHorizontalBox)
		
		// 左边：不占任何宽高的菜单定位锚点
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SAssignNew(AgentMenuAnchor, SMenuAnchor)
			.Placement(MenuPlacement_AboveAnchor)
			.OnGetMenuContent(this, &SAtAgentMessage::OnGenerateAgentMenu)
			[
				SNew(SSpacer)
				.Size(FVector2D(0.0f, 0.0f))
			]
		]

		// 右边：只在选中智能体时展现的气泡标志框
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SBox)
			.Visibility(this, &SAtAgentMessage::GetAgentBoxVisibility)
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("NoBorder"))
				.Padding(FMargin(0.0f, 0.0f, 4.0f, 0.0f)) // 仅在右侧预留 4px 间隙给输入框
				[
					SNew(SBorder)
					.BorderImage(FAppStyle::GetBrush("Menu.Background"))
					.Padding(FMargin(6.0f, 2.0f, 6.0f, 2.0f))
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						[
							SNew(SBox)
							.WidthOverride(16.0f)
							.HeightOverride(16.0f)
							[
								SNew(SImage)
								.Image(this, &SAtAgentMessage::GetAgentBrush)
							]
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(FMargin(4.0f, 0.0f, 0.0f, 0.0f))
						[
							SNew(STextBlock)
							.Text(this, &SAtAgentMessage::GetAgentText)
							.Font(FAppStyle::Get().GetFontStyle("BoldFont"))
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(FMargin(6.0f, 0.0f, 0.0f, 0.0f))
						[
							SNew(SButton)
							.ButtonStyle(FAppStyle::Get(), "HoverHintOnly")
							.ContentPadding(0.0f)
							.OnClicked(this, &SAtAgentMessage::OnClearBtnClicked)
							[
								SNew(SBorder)
								.BorderImage(FAppStyle::GetBrush("Menu.Background"))
								.BorderBackgroundColor(FLinearColor(0.2f, 0.2f, 0.2f, 0.8f))
								.Padding(FMargin(5.0f, 3.0f, 5.0f, 4.0f))
								[
									SNew(STextBlock)
									.Text(FText::FromString(TEXT("×")))
									.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
									.ColorAndOpacity(FLinearColor(0.75f, 0.75f, 0.75f, 1.0f))
								]
							]
						]
					]
				]
			]
		]
	];
}

void SAtAgentMessage::UpdateAgent(const FString& InAgentName, const struct FSlateBrush* InBrush)
{
	if (!InAgentName.IsEmpty())
	{
		AgentName = InAgentName;
		AgentBrush = InBrush;
		
		if (!AgentBrush)
		{
			AgentBrush = FChatWithUnrealStyle::Get().GetBrush("ChatWithUnreal.Agent.Agent");
		}
	}
	else
	{
		AgentName.Empty();
		AgentBrush = nullptr;
	}
}

void SAtAgentMessage::ShowSuggestionsMenu(const TArray<FString>& InSuggestions)
{
	AgentSuggestions.Empty();
	for (const FString& Name : InSuggestions)
	{
		AgentSuggestions.Add(MakeShared<FString>(Name));
	}

	if (AgentListView.IsValid())
	{
		AgentListView->RequestListRefresh();
	}

	if (AgentSuggestions.Num() > 0 && AgentMenuAnchor.IsValid())
	{
		AgentMenuAnchor->SetIsOpen(true);
	}
	else if (AgentMenuAnchor.IsValid())
	{
		AgentMenuAnchor->SetIsOpen(false);
	}
}

void SAtAgentMessage::CloseSuggestionsMenu()
{
	AgentSuggestions.Empty();
	if (AgentMenuAnchor.IsValid())
	{
		AgentMenuAnchor->SetIsOpen(false);
	}
}

FReply SAtAgentMessage::OnClearBtnClicked()
{
	UpdateAgent(TEXT(""), nullptr);
	return FReply::Handled();
}

TSharedRef<SWidget> SAtAgentMessage::OnGenerateAgentMenu()
{
	return SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("Menu.Background"))
		.Padding(FMargin(2.0f))
		[
			SNew(SBox)
			.WidthOverride(150.0f)
			.HeightOverride(120.0f)
			[
				SAssignNew(AgentListView, SListView<TSharedPtr<FString>>)
				.ItemHeight(24.0f)
				.ListItemsSource(&AgentSuggestions)
				.OnGenerateRow(this, &SAtAgentMessage::OnGenerateAgentRow)
				.OnSelectionChanged(this, &SAtAgentMessage::OnAgentSelected)
			]
		];
}

TSharedRef<ITableRow> SAtAgentMessage::OnGenerateAgentRow(TSharedPtr<FString> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	const FSlateBrush* Brush = nullptr;
	if (Item.IsValid())
	{
		Brush = FChatWithUnrealStyle::Get().GetOptionalBrush(FName(*(TEXT("ChatWithUnreal.Agent.") + *Item)), nullptr, nullptr);
		if (!Brush)
		{
			Brush = FChatWithUnrealStyle::Get().GetBrush("ChatWithUnreal.Agent.Agent");
		}
	}

	return SNew(STableRow<TSharedPtr<FString>>, OwnerTable)
		.Padding(FMargin(8.0f, 4.0f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SBox)
				.WidthOverride(16.0f)
				.HeightOverride(16.0f)
				[
					SNew(SImage)
					.Image(Brush)
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(FMargin(6.0f, 0.0f, 0.0f, 0.0f))
			[
				SNew(STextBlock)
				.Text(Item.IsValid() ? FText::FromString(*Item) : FText::GetEmpty())
			]
		];
}

void SAtAgentMessage::OnAgentSelected(TSharedPtr<FString> SelectedAgent, ESelectInfo::Type SelectInfo)
{
	if (!SelectedAgent.IsValid() || SelectInfo == ESelectInfo::OnNavigation)
	{
		return;
	}

	if (AgentMenuAnchor.IsValid())
	{
		AgentMenuAnchor->SetIsOpen(false);
	}

	const FSlateBrush* Brush = FChatWithUnrealStyle::Get().GetOptionalBrush(FName(*(TEXT("ChatWithUnreal.Agent.") + *SelectedAgent)), nullptr, nullptr);
	if (!Brush)
	{
		Brush = FChatWithUnrealStyle::Get().GetBrush("ChatWithUnreal.Agent.Agent");
	}

	UpdateAgent(*SelectedAgent, Brush);

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
}

EVisibility SAtAgentMessage::GetAgentBoxVisibility() const
{
	return AgentName.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible;
}

const FSlateBrush* SAtAgentMessage::GetAgentBrush() const
{
	return AgentBrush;
}

FText SAtAgentMessage::GetAgentText() const
{
	return FText::FromString(TEXT("@") + AgentName);
}
