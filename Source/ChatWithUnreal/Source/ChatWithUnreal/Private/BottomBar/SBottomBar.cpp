// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#include "SBottomBar.h"
#include "SChatInput.h"
#include "BottomBar/SAtAgentMessage.h"
#include "SChatSendButton.h"
#include "SAttachmentList.h"
#include "SInteractionModeSelector.h"
#include "SToolModeSelector.h"
#include "SAbilitiesSelector.h"
#include "SQuotaBar.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SMenuAnchor.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"
#include "Styling/AppStyle.h"
#include "ChatWithUnrealStyle.h"
#include "Misc/Base64.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformApplicationMisc.h"
#include "DesktopPlatformModule.h"
#include "IDesktopPlatform.h"

#if PLATFORM_WINDOWS
#include "FabWindowsClipboard.h"
#endif

TWeakPtr<SBottomBar> SBottomBar::Instance = nullptr;

void SBottomBar::Construct(const FArguments& InArgs)
{
	Instance = SharedThis(this);
	OnSendClickedEvent = InArgs._OnSendClicked;
	OnInterruptClickedEvent = InArgs._OnInterruptClicked;
	OnInteractionModeChangedEvent = InArgs._OnInteractionModeChanged;
	OnToolModeChangedEvent = InArgs._OnToolModeChanged;

	ChildSlot
	[
		SNew(SVerticalBox)
		
		// 0. 附件列表
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 5.0f)
		[
			SAssignNew(AttachmentList, SAttachmentList)
			.OnAttachmentAdded_Lambda([this](const FString& ImageId) {
				if (ChatInput.IsValid())
				{
					ChatInput->InsertImageTag(ImageId);
				}
			})
			.OnAttachmentRemoved_Lambda([this](const FString& ImageId) {
				if (ChatInput.IsValid() && AttachmentList.IsValid())
				{
					const auto& Items = AttachmentList->GetAttachmentItems();
					int32 TargetIndex = INDEX_NONE;
					for (int32 i = 0; i < Items.Num(); ++i)
					{
						if (Items[i].ImageId == ImageId)
						{
							TargetIndex = i;
							break;
						}
					}

					if (TargetIndex != INDEX_NONE)
					{
						ChatInput->RemoveImageTag(TargetIndex);
					}
				}
			})
		]

		// 1. 输入框
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SBorder)
			.Padding(FMargin(4.0f, 2.0f, 4.0f, 2.0f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SAssignNew(AtAgentMessage, SAtAgentMessage)
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.VAlign(VAlign_Center)
				[
					SAssignNew(ChatInput, SChatInput)
					.OnSendShortcutTriggered(FSimpleDelegate::CreateSP(this, &SBottomBar::OnChatInputSendRequested))
					.OnPasteShortcutTriggered(FOnPasteShortcutTriggered::CreateSP(this, &SBottomBar::OnPasteImageFromClipboard))
					.OnFilesDropped(FOnFilesDropped::CreateSP(this, &SBottomBar::OnFilesDropped))
				]
			]
		]

		// 2. 控制工具条 (原子控件拼装)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 5.0f, 0.0f, 0.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SAssignNew(InteractionModeSelector, SInteractionModeSelector)
				.OnInteractionModeChanged(this, &SBottomBar::OnInteractionModeChanged)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(5.0f, 0.0f, 0.0f, 0.0f)
			[
				SAssignNew(AbilitiesSelector, SAbilitiesSelector)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(5.0f, 0.0f, 0.0f, 0.0f)
			[
				SAssignNew(ToolModeSelector, SToolModeSelector)
				.OnGetInteractionMode(this, &SBottomBar::GetInteractionMode)
				.OnToolModeChanged(this, &SBottomBar::OnToolModeChanged)
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNew(SSpacer)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(5.0f, 0.0f)
			[
				SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "HoverHintOnly")
				.ContentPadding(FMargin(5.0f))
				.ToolTipText(FText::FromString(TEXT("Add Image Attachment")))
				.OnClicked(this, &SBottomBar::OnAddAttachmentClicked)
				[
					SNew(SImage)
					.Image(FAppStyle::Get().GetBrush("Icons.Plus"))
					.DesiredSizeOverride(FVector2D(16.0f, 16.0f))
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SAssignNew(SendButton, SChatSendButton)
				.OnSendClicked(FSimpleDelegate::CreateSP(this, &SBottomBar::OnChatInputSendRequested))
				.OnInterruptClicked(OnInterruptClickedEvent)
			]
		]

		// 3. 配额进度条
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 5.0f, 0.0f, 0.0f)
		[
			SAssignNew(QuotaBar, SQuotaBar)
			.Percent(0.5f)
			.Visibility(EVisibility::Collapsed) 
		]
	];

	if (ChatInput.IsValid() && AttachmentList.IsValid())
	{
		ChatInput->SetAttachmentList(AttachmentList);
	}
}

void SBottomBar::OnInteractionModeChanged(const FString& NewMode)
{
	if (ToolModeSelector.IsValid())
	{
		ToolModeSelector->SyncWithInteractionMode(NewMode);
	}
	OnInteractionModeChangedEvent.ExecuteIfBound(NewMode);
}

void SBottomBar::OnToolModeChanged(const FString& NewTool)
{
	OnToolModeChangedEvent.ExecuteIfBound(NewTool);
}

FString SBottomBar::GetInteractionMode() const
{
	return InteractionModeSelector.IsValid() ? InteractionModeSelector->GetCurrentMode() : TEXT("Chat");
}

void SBottomBar::OnChatInputSendRequested()
{
	OnSendClickedEvent.ExecuteIfBound();
}

FReply SBottomBar::OnPasteImageFromClipboard()
{
#if PLATFORM_WINDOWS
	int32 Width, Height;
	TArray<uint8> PendingImageData;
	if (FFabWindowsClipboard::GetBitmapFromClipboard(PendingImageData, Width, Height))
	{
		FString Base64Str = FBase64::Encode(PendingImageData);
		if (AttachmentList.IsValid())
		{
			AttachmentList->AddAttachment(Base64Str);
		}
		return FReply::Handled();
	}
#endif
	return FReply::Unhandled();
}

FReply SBottomBar::OnAddAttachmentClicked()
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (!DesktopPlatform) return FReply::Handled();

	TArray<FString> OutFiles;
	void* ParentWindowHandle = FSlateApplication::Get().GetActiveTopLevelWindow()->GetNativeWindow()->GetOSWindowHandle();

	if (DesktopPlatform->OpenFileDialog(
		ParentWindowHandle,
		TEXT("Select Image Attachment"),
		TEXT(""),
		TEXT(""),
		TEXT("Image Files (*.png;*.jpg;*.jpeg)|*.png;*.jpg;*.jpeg"),
		EFileDialogFlags::None,
		OutFiles
	))
	{
		for (const FString& FilePath : OutFiles)
		{
			TArray<uint8> FileData;
			if (FFileHelper::LoadFileToArray(FileData, *FilePath))
			{
				FString Base64Str = FBase64::Encode(FileData);
				if (AttachmentList.IsValid())
				{
					AttachmentList->AddAttachment(Base64Str);
				}
			}
		}
	}

	return FReply::Handled();
}

void SBottomBar::OnFilesDropped(const TArray<FString>& Files)
{
	for (const FString& FilePath : Files)
	{
		FString Ext = FPaths::GetExtension(FilePath).ToLower();
		if (Ext == TEXT("png") || Ext == TEXT("jpg") || Ext == TEXT("jpeg") || Ext == TEXT("bmp") || Ext == TEXT("wav") || Ext == TEXT("mp3"))
		{
			TArray<uint8> FileData;
			if (FFileHelper::LoadFileToArray(FileData, *FilePath))
			{
				FString Base64Str = FBase64::Encode(FileData);
				if (AttachmentList.IsValid())
				{
					AttachmentList->AddAttachment(Base64Str);
				}
			}
		}
	}
}
