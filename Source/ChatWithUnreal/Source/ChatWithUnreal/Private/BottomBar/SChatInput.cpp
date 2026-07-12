// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#include "SChatInput.h"
#include "SAttachmentList.h"
#include "BottomBar/SAtAgentMessage.h"
#include "Internationalization/Culture.h"
#include "Internationalization/Internationalization.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SMenuAnchor.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/AppStyle.h"
#include "ChatWithUnrealStyle.h"
#include "Framework/Text/TextLayout.h"

#include "ImageUtils.h"
#include "Brushes/SlateDynamicImageBrush.h"
#include "Input/DragAndDrop.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#include "Editor.h"

namespace
{
	FText GetLocText(const FString& English, const FString& Chinese)
	{
		const FString Culture = FInternationalization::Get().GetCurrentCulture()->GetName();
		return FText::FromString(Culture.StartsWith(TEXT("zh")) ? Chinese : English);
	}
}

TWeakPtr<SChatInput> SChatInput::Instance = nullptr;

void SChatInput::Construct(const FArguments& InArgs)
{
	Instance = SharedThis(this);
	bIsUpdatingText = false;
	LastText = TEXT("");

	OnSendShortcutTriggeredEvent = InArgs._OnSendShortcutTriggered;
	OnPasteShortcutTriggeredEvent = InArgs._OnPasteShortcutTriggered;
	OnFilesDroppedEvent = InArgs._OnFilesDropped;
	OnAtAgentTriggeredEvent = InArgs._OnAtAgentTriggered;

	ChildSlot
	[
		SAssignNew(InputTextBox, SMultiLineEditableTextBox)
		.AutoWrapText(true)
		.HintText(GetLocText(TEXT("Type a message... (Ctrl+Enter to Send)"), TEXT("输入消息... (Ctrl+Enter 发送)")))
		.OnKeyDownHandler(this, &SChatInput::OnInputKeyDown)
		.OnTextChanged(this, &SChatInput::HandleTextChanged)
	];
}

FText SChatInput::GetText() const
{
	if (!InputTextBox.IsValid()) return FText::GetEmpty();

	FString RawText = InputTextBox->GetText().ToString();
	FString ResultText;
	int32 ImageCounter = 0;

	for (int32 i = 0; i < RawText.Len(); ++i)
	{
		if (RawText[i] == 0x25C6) // ◆
		{
			ImageCounter++;
			ResultText += FString::Printf(TEXT("<WinyunqImageBegin>image%d<WinyunqImageEnd>"), ImageCounter);
		}
		else
		{
			ResultText.AppendChar(RawText[i]);
		}
	}
	return FText::FromString(ResultText);
}

void SChatInput::SetText(const FText& InText)
{
	if (InputTextBox.IsValid())
	{
		FString RawText = InText.ToString();
		int32 SearchPos = 0;
		while ((SearchPos = RawText.Find(TEXT("<WinyunqImageBegin>"), ESearchCase::CaseSensitive, ESearchDir::FromStart, SearchPos)) != INDEX_NONE)
		{
			int32 EndTagIdx = RawText.Find(TEXT("<WinyunqImageEnd>"), ESearchCase::CaseSensitive, ESearchDir::FromStart, SearchPos + 19);
			if (EndTagIdx != INDEX_NONE)
			{
				RawText.RemoveAt(SearchPos, (EndTagIdx + 17) - SearchPos);
				RawText.InsertAt(SearchPos, TEXT("\u25C6"));
				SearchPos += 1;
			}
			else
			{
				SearchPos += 19;
			}
		}
		
		bIsUpdatingText = true;
		InputTextBox->SetText(FText::FromString(RawText));
		LastText = RawText;
		bIsUpdatingText = false;
	}
}

void SChatInput::ClearText()
{
	if (InputTextBox.IsValid())
	{
		bIsUpdatingText = true;
		InputTextBox->SetText(FText::GetEmpty());
		LastText = TEXT("");
		bIsUpdatingText = false;
	}
}

void SChatInput::FocusInput()
{
	if (InputTextBox.IsValid())
	{
		FSlateApplication::Get().SetKeyboardFocus(InputTextBox);
	}
}

FReply SChatInput::OnInputKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	bool bIsEnter = InKeyEvent.GetKey() == EKeys::Enter;
	bool bHasCtrl = InKeyEvent.IsControlDown();
	bool bHasShift = InKeyEvent.IsShiftDown();

	if (bIsEnter && (!bHasShift || bHasCtrl))
	{
		OnSendShortcutTriggeredEvent.ExecuteIfBound();
		return FReply::Handled();
	}

	if (bHasCtrl && InKeyEvent.GetKey() == EKeys::V)
	{
		if (OnPasteShortcutTriggeredEvent.IsBound())
		{
			FReply Reply = OnPasteShortcutTriggeredEvent.Execute();
			if (Reply.IsEventHandled())
			{
				return Reply;
			}
		}
	}

	// 在输入框开头（消息头）敲击 BackSpace 时直接清除 @agent 状态
	if (InKeyEvent.GetKey() == EKeys::BackSpace && InputTextBox.IsValid())
	{
		FTextLocation CursorLoc = InputTextBox->GetCursorLocation();
		if (InputTextBox->GetText().IsEmpty() || (CursorLoc.GetLineIndex() == 0 && CursorLoc.GetOffset() == 0))
		{
			if (auto AtAgentMsg = SAtAgentMessage::Instance.Pin())
			{
				AtAgentMsg->UpdateAgent(TEXT(""), nullptr);
				return FReply::Handled();
			}
		}
	}

	return FReply::Unhandled();
}

FReply SChatInput::OnDragOver(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	TSharedPtr<FExternalDragOperation> ExternalDrag = DragDropEvent.GetOperationAs<FExternalDragOperation>();
	if (ExternalDrag.IsValid() && ExternalDrag->HasFiles())
	{
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

FReply SChatInput::OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	TSharedPtr<FExternalDragOperation> ExternalDrag = DragDropEvent.GetOperationAs<FExternalDragOperation>();
	if (ExternalDrag.IsValid() && ExternalDrag->HasFiles())
	{
		if (OnFilesDroppedEvent.IsBound())
		{
			OnFilesDroppedEvent.Execute(ExternalDrag->GetFiles());
			return FReply::Handled();
		}
	}
	return FReply::Unhandled();
}

void SChatInput::RemoveImageTag(int32 TargetIndex)
{
	if (!InputTextBox.IsValid()) return;
	
	FString CurrentText = InputTextBox->GetText().ToString();
	int32 FoundCount = -1;
	for (int32 i = 0; i < CurrentText.Len(); ++i)
	{
		if (CurrentText[i] == 0x25C6) // ◆
		{
			FoundCount++;
			if (FoundCount == TargetIndex)
			{
				CurrentText.RemoveAt(i, 1);
				break;
			}
		}
	}
	
	bIsUpdatingText = true;
	InputTextBox->SetText(FText::FromString(CurrentText));
	LastText = CurrentText;
	bIsUpdatingText = false;
}

void SChatInput::InsertImageTag(const FString& InImageId)
{
	if (InputTextBox.IsValid())
	{
		bIsUpdatingText = true;
		InputTextBox->InsertTextAtCursor(FText::FromString(TEXT("\u25C6")));
		LastText = InputTextBox->GetText().ToString();
		bIsUpdatingText = false;
	}
}

void SChatInput::SetAttachmentList(TSharedPtr<SAttachmentList> InList)
{
	AttachmentListWidget = InList;
}

void SChatInput::HandleTextChanged(const FText& NewText)
{
	if (bIsUpdatingText) return;

	FString NewTextStr = NewText.ToString();

	// 触发 AtAgent 联想检测
	int32 AtIndex = INDEX_NONE;
	if (NewTextStr.FindLastChar(TEXT('@'), AtIndex))
	{
		FString FilterText = NewTextStr.Mid(AtIndex + 1);
		if (!FilterText.Contains(TEXT(" ")))
		{
			if (OnAtAgentTriggeredEvent.IsBound())
			{
				OnAtAgentTriggeredEvent.Execute(FilterText);
			}
		}
	}

	TSharedPtr<SAttachmentList> AttList = AttachmentListWidget.Pin();
	if (AttList.IsValid())
	{
		TArray<int32> OldTokenIndices;
		TArray<int32> NewTokenIndices;

		for (int32 i = 0; i < LastText.Len(); ++i)
		{
			if (LastText[i] == 0x25C6)
			{
				OldTokenIndices.Add(i);
			}
		}

		for (int32 i = 0; i < NewTextStr.Len(); ++i)
		{
			if (NewTextStr[i] == 0x25C6)
			{
				NewTokenIndices.Add(i);
			}
		}

		const TArray<FAttachmentItem>& Items = AttList->GetAttachmentItems();

		if (NewTokenIndices.Num() < OldTokenIndices.Num() && Items.Num() > 0)
		{
			if (NewTokenIndices.Num() == 0)
			{
				bIsUpdatingText = true;
				AttList->ClearAttachments();
				bIsUpdatingText = false;
			}
			else
			{
				int32 ErasedIndex = -1;
				for (int32 i = 0; i < NewTokenIndices.Num(); ++i)
				{
					FString NewPrefix = NewTextStr.Left(NewTokenIndices[i]).Replace(TEXT("◆"), TEXT(""));
					FString OldPrefix = LastText.Left(OldTokenIndices[i]).Replace(TEXT("◆"), TEXT(""));

					if (NewPrefix != OldPrefix)
					{
						ErasedIndex = i;
						break;
					}
				}

				if (ErasedIndex == -1)
				{
					ErasedIndex = OldTokenIndices.Num() - 1;
				}

				if (ErasedIndex >= 0 && ErasedIndex < Items.Num())
				{
					bIsUpdatingText = true;
					AttList->RemoveAttachmentById(Items[ErasedIndex].ImageId);
					bIsUpdatingText = false;
				}
			}
		}
	}

	LastText = NewTextStr;
}