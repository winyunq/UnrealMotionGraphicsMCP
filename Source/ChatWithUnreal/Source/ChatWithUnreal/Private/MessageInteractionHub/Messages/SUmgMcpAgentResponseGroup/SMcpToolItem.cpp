// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#include "SMcpToolItem.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Input/SButton.h"
#include "Styling/AppStyle.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"
#include "Dom/JsonObject.h"

#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 8)
#include "Containers/SharedString.h"
#endif

#define LOCTEXT_NAMESPACE "SMcpToolItem"

namespace
{
	FString JsonKeyToString(const FString& Key)
	{
		return Key;
	}

#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 8)
	FString JsonKeyToString(const UE::FSharedString& Key)
	{
		return FString(Key.ToView());
	}
#endif
}

void SMcpToolItem::Construct(const FArguments& InArgs)
{
	ToolName = InArgs._ToolName;
	ArgumentsJson = InArgs._ArgumentsJson;

	ChildSlot
	[
		SNew(SVerticalBox)
		// 头部：全新高级布局 (Run MCP -> 折叠箭头 -> 工具名高亮Tag -> 状态)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(4.0f, 2.0f)
		[
			SNew(SHorizontalBox)
			// Slot 1: Run MCP 前缀 (支持多语言)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0.0f, 0.0f, 6.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("RunMcpPrefix", "Run MCP"))
				.Font(FAppStyle::Get().GetFontStyle("NormalFont"))
				.ColorAndOpacity(FLinearColor(0.6f, 0.6f, 0.6f, 0.8f))
			]
			// Slot 2: 可点击的同高折叠箭头按钮
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0.0f, 0.0f, 8.0f, 0.0f)
			[
				SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "SimpleButton")
				.OnClicked(this, &SMcpToolItem::OnToggleDetailsClicked)
				.ContentPadding(FMargin(2.0f, 0.0f))
				[
					SAssignNew(ToggleButtonTextBlock, STextBlock)
					.Text(FText::FromString(TEXT("▶")))
					.Font(FAppStyle::Get().GetFontStyle("NormalFont"))
					.ColorAndOpacity(FLinearColor(0.8f, 0.8f, 0.8f))
				]
			]
			// Slot 3: 高亮且带精致海蓝色气泡底色的 MCP 工具名
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0.0f, 0.0f, 12.0f, 0.0f)
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("DetailsView.CategoryMiddle"))
				.BorderBackgroundColor(FLinearColor(0.14f, 0.22f, 0.33f, 1.0f))
				.Padding(FMargin(8.0f, 2.0f))
				[
					SNew(STextBlock)
					.Text(FText::FromString(ToolName))
					.Font(FAppStyle::Get().GetFontStyle("NormalFontBold"))
					.ColorAndOpacity(FLinearColor(0.4f, 0.85f, 1.0f))
				]
			]
			// Slot 4: 状态显示文本
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			[
				SAssignNew(StatusTextBlock, STextBlock)
				.Text(FText::FromString(TEXT("Pending approval...")))
				.Font(FAppStyle::Get().GetFontStyle("SmallFont"))
				.ColorAndOpacity(FLinearColor(0.5f, 0.5f, 0.5f))
			]
		]
		// 详情区 (可扩展展示参数/结果)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(16.0f, 4.0f, 4.0f, 4.0f)
		[
			SAssignNew(DetailsBox, SVerticalBox)
			.Visibility(EVisibility::Collapsed) // 默认收起
		]
	];

	// 动态填充参数表格
	RebuildDetailsBox();
}

void SMcpToolItem::SetStatus(EToolStatus InStatus, const FString& ErrorMessage, const FString& InResultString)
{
	CurrentStatus = InStatus;
	FString StatusText;
	FLinearColor StatusColor = FLinearColor::White;

	if (InStatus == EToolStatus::Success && !InResultString.IsEmpty())
	{
		ResultString = InResultString;
	}

	switch (InStatus)
	{
		case EToolStatus::Pending: StatusText = TEXT("Pending approval..."); StatusColor = FLinearColor(0.5f, 0.5f, 0.5f); break;
		case EToolStatus::Running: StatusText = TEXT("Executing..."); StatusColor = FLinearColor::Yellow; break;
		case EToolStatus::Success: StatusText = TEXT("Success"); StatusColor = FLinearColor::Green; break;
		case EToolStatus::Failed: StatusText = FString::Printf(TEXT("Failed: %s"), *ErrorMessage); StatusColor = FLinearColor::Red; break;
		case EToolStatus::Rejected: StatusText = TEXT("Rejected"); StatusColor = FLinearColor::Gray; break;
	}

	if (StatusTextBlock.IsValid())
	{
		StatusTextBlock->SetText(FText::FromString(StatusText));
		StatusTextBlock->SetColorAndOpacity(StatusColor);
	}

	// 触发详情区重构：此时若成功则会用结果替换参数表格
	RebuildDetailsBox();

	// 仅刷新箭头的状态
	if (ToggleButtonTextBlock.IsValid())
	{
		ToggleButtonTextBlock->SetText(FText::FromString(bIsExpanded ? TEXT("▼") : TEXT("▶")));
	}
}

FReply SMcpToolItem::OnToggleDetailsClicked()
{
	bIsExpanded = !bIsExpanded;
	if (DetailsBox.IsValid())
	{
		DetailsBox->SetVisibility(bIsExpanded ? EVisibility::Visible : EVisibility::Collapsed);
	}

	if (ToggleButtonTextBlock.IsValid())
	{
		ToggleButtonTextBlock->SetText(FText::FromString(bIsExpanded ? TEXT("▼") : TEXT("▶")));
	}
	return FReply::Handled();
}

void SMcpToolItem::RebuildDetailsBox()
{
	if (!DetailsBox.IsValid()) return;

	DetailsBox->ClearChildren();

	const bool bHasResult = (CurrentStatus == EToolStatus::Success) && !ResultString.IsEmpty();
	const FString TargetJson = bHasResult ? ResultString : ArgumentsJson;

	DetailsBox->AddSlot()
	.AutoHeight()
	[
		CreateKeyValueTable(TargetJson, bHasResult)
	];
}

TSharedRef<SWidget> SMcpToolItem::CreateKeyValueTable(const FString& InJson, bool bIsResult)
{
	TSharedPtr<FJsonObject> JsonObj;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(InJson);
	const bool bIsParsed = FJsonSerializer::Deserialize(Reader, JsonObj) && JsonObj.IsValid();

	if (bIsParsed && JsonObj->Values.Num() > 0)
	{
		TSharedRef<SGridPanel> GridPanel = SNew(SGridPanel);

		int32 RowIndex = 0;
		for (const auto& Pair : JsonObj->Values)
		{
			FString Key = JsonKeyToString(Pair.Key);
			FString Value;

			if (Pair.Value.IsValid())
			{
				if (Pair.Value->Type == EJson::Object)
				{
					TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Value);
					FJsonSerializer::Serialize(Pair.Value->AsObject().ToSharedRef(), Writer);
				}
				else if (Pair.Value->Type == EJson::Array)
				{
					TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Value);
					FJsonSerializer::Serialize(Pair.Value->AsArray(), Writer);
				}
				else
				{
					Value = Pair.Value->AsString();
				}
			}

			// Key 单元格（暗金色加粗）
			GridPanel->AddSlot(0, RowIndex)
			.Padding(4.0f, 4.0f, 16.0f, 4.0f)
			.VAlign(VAlign_Top)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Key))
				.Font(FAppStyle::Get().GetFontStyle("NormalFontBold"))
				.ColorAndOpacity(FLinearColor(0.85f, 0.70f, 0.52f))
			];

			// Value 单元格（柔白色自适应）
			GridPanel->AddSlot(1, RowIndex)
			.Padding(4.0f, 4.0f, 4.0f, 4.0f)
			.VAlign(VAlign_Top)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Value))
				.AutoWrapText(true)
				.WrapTextAt(0.0f)
				.Font(FAppStyle::Get().GetFontStyle("NormalFont"))
				.ColorAndOpacity(FLinearColor(0.9f, 0.9f, 0.95f))
			];

			RowIndex++;
		}

		return SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("DetailsView.CategoryMiddle"))
			.BorderBackgroundColor(FLinearColor(0.08f, 0.1f, 0.14f, 1.0f))
			.Padding(FMargin(10.0f, 8.0f))
			[
				GridPanel
			];
	}

	if (!bIsResult)
	{
		// 如果是参数阶段且无任何键值对（即无参数），则不渲染任何卡片，直接返回 SNullWidget::NullWidget
		return SNullWidget::NullWidget;
	}

	FString CleanString = InJson.TrimStartAndEnd();
	if (CleanString.IsEmpty())
	{
		CleanString = TEXT("(Empty result)");
	}

	// 兜底方案（带轻质边框的代码块样式）
	return SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("DetailsView.CategoryMiddle"))
		.BorderBackgroundColor(FLinearColor(0.08f, 0.1f, 0.14f, 1.0f))
		.Padding(FMargin(10.0f, 8.0f))
		[
			SNew(STextBlock)
			.Text(FText::FromString(CleanString))
			.AutoWrapText(true)
			.WrapTextAt(0.0f)
			.Font(FAppStyle::Get().GetFontStyle("SmallFont"))
			.ColorAndOpacity(FLinearColor(0.7f, 0.9f, 0.7f))
		];
}

#undef LOCTEXT_NAMESPACE
