// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#include "SChatWelcome.h"
#include "SChatHistoryItem.h"

#include "Internationalization/Culture.h"
#include "Internationalization/Internationalization.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"
#include "HAL/PlatformProcess.h"

namespace
{
	FText GetLocText(const FString& English, const FString& Chinese)
	{
		const FString Culture = FInternationalization::Get().GetCurrentCulture()->GetName();
		return FText::FromString(Culture.StartsWith(TEXT("zh")) ? Chinese : English);
	}

	FString FormatDateTime(const FDateTime& DateTime)
	{
		return DateTime.ToString(TEXT("%Y-%m-%d"));
	}
}

void SChatWelcome::Construct(const FArguments& InArgs)
{
	OnSessionSelectedEvent = InArgs._OnSessionSelected;
	OnSessionDeletedEvent = InArgs._OnSessionDeleted;

	ChildSlot
	.Padding(FMargin(40.0f, 20.0f))
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::Get().GetBrush("ToolPanel.GroupBorder"))
		[
			SNew(SVerticalBox)

			// 1. 标题区域
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Center)
			.Padding(FMargin(0.0f, 40.0f, 0.0f, 5.0f))
			[
				SNew(STextBlock)
				.Text(GetLocText(TEXT("Unreal Motion Graphics AI assist"), TEXT("Unreal Motion Graphics AI assist")))
				.Font(FAppStyle::Get().GetFontStyle("NormalFontBold"))
				.ColorAndOpacity(FLinearColor(1.0f, 0.8f, 0.2f))
			]

			// 2. GitHub 链接
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Center)
			.Padding(FMargin(0.0f, 0.0f, 0.0f, 40.0f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(STextBlock).Text(FText::FromString(TEXT("("))).Font(FAppStyle::Get().GetFontStyle("SmallFont")).ColorAndOpacity(FLinearColor(0.3f, 0.3f, 0.3f))
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SButton)
					.ButtonStyle(FAppStyle::Get(), "NoBorder")
					.Cursor(EMouseCursor::Hand)
					.OnClicked_Lambda([]() {
						FPlatformProcess::LaunchURL(TEXT("https://github.com/winyunq/UnrealMotionGraphicsMCP"), nullptr, nullptr);
						return FReply::Handled();
					})
					[
						SNew(STextBlock)
						.Text(GetLocText(TEXT("MCP version is Free in https://github.com/winyunq/UnrealMotionGraphicsMCP"), TEXT("开源 MCP 核心服务可于 github.com/winyunq 获取")))
						.Font(FAppStyle::Get().GetFontStyle("SmallFont"))
						.ColorAndOpacity(FLinearColor(0.3f, 0.5f, 0.9f))
					]
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(STextBlock).Text(FText::FromString(TEXT(")"))).Font(FAppStyle::Get().GetFontStyle("SmallFont")).ColorAndOpacity(FLinearColor(0.3f, 0.3f, 0.3f))
				]
			]

			// 3. 引导提示
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Center)
			.Padding(FMargin(0.0f, 0.0f, 0.0f, 40.0f))
			[
				SNew(STextBlock)
				.Text(GetLocText(
					TEXT("Enter your request below and send to start a new chat\nor pick a history session from right side"),
					TEXT("在屏幕底部聊天框输入你的需要，发送即可开始新对话\n或直接从右侧点击历史对话恢复！")))
				.Font(FAppStyle::Get().GetFontStyle("NormalFont"))
				.ColorAndOpacity(FLinearColor(0.8f, 0.8f, 0.8f))
				.Justification(ETextJustify::Center)
			]

			// 4. 操作区 (0.4/0.6 水平分栏)
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(0.4f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.FillHeight(1.0f)
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(FText::FromString(TEXT("✨")))
						.Font(FCoreStyle::GetDefaultFontStyle("Regular", 60))
						.ColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, 0.1f))
					]
				]
				+ SHorizontalBox::Slot()
				.FillWidth(0.6f)
				.Padding(FMargin(20.0f, 0.0f, 0.0f, 0.0f))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(FMargin(0.0f, 0.0f, 0.0f, 10.0f))
					[
						SNew(STextBlock)
						.Text(GetLocText(TEXT("History"), TEXT("历史记录")))
						.Font(FAppStyle::Get().GetFontStyle("NormalFontBold"))
					]
					+ SVerticalBox::Slot()
					.FillHeight(1.0f)
					[
						SNew(SOverlay)
						+ SOverlay::Slot()
						[
							SNew(SScrollBox)
							.ScrollBarThickness(FVector2D(4.0f, 4.0f))
							+ SScrollBox::Slot()
							[
								SAssignNew(HistoryListBox, SVerticalBox)
							]
						]
						+ SOverlay::Slot()
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Text(GetLocText(TEXT("No history"), TEXT("你还没有对话过")))
							.Font(FAppStyle::Get().GetFontStyle("NormalFont"))
							.ColorAndOpacity(FLinearColor(0.4f, 0.4f, 0.4f))
							.Visibility(this, &SChatWelcome::GetEmptyListVisibility)
						]
					]
				]
			]
		]
	];
}

void SChatWelcome::ClearHistoryList()
{
	if (HistoryListBox.IsValid())
	{
		HistoryListBox->ClearChildren();
	}
}

void SChatWelcome::AddHistoryItem(const FString& SessionId, const FString& Title, int32 MessageCount, const FDateTime& LastModified)
{
	if (!HistoryListBox.IsValid()) return;

	FString LocalSessionId = SessionId;

	HistoryListBox->AddSlot()
	.AutoHeight()
	.Padding(FMargin(0.0f, 2.0f))
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
		.Padding(FMargin(2.0f))
		[
			SNew(SButton)
			.ButtonStyle(FAppStyle::Get(), "FlatButton")
			.OnClicked_Lambda([this, LocalSessionId]() {
				OnHistoryItemSelected(LocalSessionId);
				return FReply::Handled();
			})
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.Padding(8, 4)
				.VAlign(VAlign_Center)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(STextBlock)
						.Text(FText::FromString(Title))
						.Font(FAppStyle::Get().GetFontStyle("NormalFont"))
					]
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(STextBlock)
						.Text(FText::FromString(FormatDateTime(LastModified)))
						.Font(FAppStyle::Get().GetFontStyle("SmallFont"))
						.ColorAndOpacity(FLinearColor::Gray)
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(8, 0)
				[
					SNew(STextBlock)
					.Text(FText::AsNumber(MessageCount))
					.Font(FAppStyle::Get().GetFontStyle("SmallFont"))
					.ColorAndOpacity(FLinearColor(0.5f, 0.5f, 0.5f))
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0, 0, 4, 0)
				[
					SNew(SButton)
					.ButtonStyle(FAppStyle::Get(), "HoverHintOnly")
					.OnClicked_Lambda([this, LocalSessionId]() {
						OnHistoryItemDeleted(LocalSessionId);
						return FReply::Handled();
					})
					.ToolTipText(FText::FromString(TEXT("Delete this conversation")))
					[
						SNew(STextBlock)
						.Text(FText::FromString(TEXT("x")))
						.Font(FAppStyle::Get().GetFontStyle("NormalFontBold"))
						.ColorAndOpacity(FLinearColor::Gray)
					]
				]
			]
		]
	];
}

void SChatWelcome::OnHistoryItemSelected(const FString& SessionId)
{
	UE_LOG(LogTemp, Log, TEXT("Winyunq UI Signal: History Item [%s] Clicked. Executing Broadcast."), *SessionId);
	OnSessionSelectedEvent.ExecuteIfBound(SessionId);
}

void SChatWelcome::OnHistoryItemDeleted(const FString& SessionId)
{
	OnSessionDeletedEvent.ExecuteIfBound(SessionId);
}

EVisibility SChatWelcome::GetEmptyListVisibility() const
{
	return (HistoryListBox.IsValid() && HistoryListBox->GetChildren()->Num() > 0) ? EVisibility::Collapsed : EVisibility::Visible;
}
