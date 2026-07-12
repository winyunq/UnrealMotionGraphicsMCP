// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#include "SMcpApprovalQueue.h"
#include "SMcpToolItem.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/AppStyle.h"
#include "Editor.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"
#include "Widgets/Layout/SBorder.h"

#define LOCTEXT_NAMESPACE "SMcpApprovalQueue"

void SMcpApprovalQueue::Construct(const FArguments& InArgs)
{
	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("DetailsView.CategoryMiddle"))
		.BorderBackgroundColor(FLinearColor(0.11f, 0.14f, 0.18f, 1.0f))
		.Padding(FMargin(10.0f, 8.0f))
		[
			SAssignNew(ListContainer, SVerticalBox)
		]
	];
}

void SMcpApprovalQueue::AddToolRequest(const FString& ToolName, const FString& ArgumentsJson)
{
	if (ListContainer.IsValid())
	{
		TSharedPtr<SMcpToolItem> NewItem = SNew(SMcpToolItem)
			.ToolName(ToolName)
			.ArgumentsJson(ArgumentsJson);
		
		ToolItems.Add(NewItem);
		ListContainer->AddSlot()
		.AutoHeight()
		[
			NewItem.ToSharedRef()
		];
	}
}

void SMcpApprovalQueue::PresentFirstApprovalButtons()
{
	CurrentFocusIndex = 0;
	RenderActiveButtons();
}

void SMcpApprovalQueue::RenderActiveButtons()
{
	if (!ToolItems.IsValidIndex(CurrentFocusIndex))
	{
		// 队列已全部处理完，回调
		OnMcpTaskFinished.ExecuteIfBound(CollectedResults);
		return;
	}

	// 在容器末尾插入按钮组
	ListContainer->AddSlot()
	.AutoHeight()
	.Padding(16.0f, 4.0f)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SButton)
			.Text(LOCTEXT("Accept", "Accept"))
			.ButtonStyle(FAppStyle::Get(), "PrimaryButton")
			.OnClicked(this, &SMcpApprovalQueue::OnAcceptClicked)
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(8.0f, 0.0f)
		[
			SNew(SButton)
			.Text(LOCTEXT("Reject", "Reject"))
			.OnClicked(this, &SMcpApprovalQueue::OnRejectClicked)
		]
	];
}

FReply SMcpApprovalQueue::OnAcceptClicked()
{
	if (!ToolItems.IsValidIndex(CurrentFocusIndex)) return FReply::Handled();

	TSharedPtr<SMcpToolItem> ActiveItem = ToolItems[CurrentFocusIndex];
	ActiveItem->SetStatus(SMcpToolItem::EToolStatus::Running);

	FString ResolvedToolCallId;
	FString ResultString;
	if (OnExecuteTool.IsBound())
	{
		ResultString = OnExecuteTool.Execute(ActiveItem->GetToolName(), ActiveItem->GetArgumentsJson(), ResolvedToolCallId);
	}

	ActiveItem->SetStatus(SMcpToolItem::EToolStatus::Success, TEXT(""), ResultString);

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetStringField(TEXT("role"), TEXT("tool"));
	ResultObj->SetStringField(TEXT("tool_call_id"), ResolvedToolCallId);
	ResultObj->SetStringField(TEXT("name"), ActiveItem->GetToolName());
	ResultObj->SetStringField(TEXT("content"), ResultString);
	CollectedResults.Add(ResultObj);

	AdvanceFocus();
	return FReply::Handled();
}

FReply SMcpApprovalQueue::OnRejectClicked()
{
	if (!ToolItems.IsValidIndex(CurrentFocusIndex)) return FReply::Handled();

	ToolItems[CurrentFocusIndex]->SetStatus(SMcpToolItem::EToolStatus::Rejected);
	
	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetStringField(TEXT("role"), TEXT("tool"));
	ResultObj->SetStringField(TEXT("content"), TEXT("User rejected this tool call."));
	CollectedResults.Add(ResultObj);

	OnToolRejected.ExecuteIfBound();

	AdvanceFocus();
	return FReply::Handled();
}

void SMcpApprovalQueue::AdvanceFocus()
{
	// 移除最后添加的那个按钮组 Slot
	ListContainer->RemoveSlot(ListContainer->GetChildren()->GetChildAt(ListContainer->GetChildren()->Num() - 1));
	CurrentFocusIndex++;
	RenderActiveButtons();
}

#undef LOCTEXT_NAMESPACE
