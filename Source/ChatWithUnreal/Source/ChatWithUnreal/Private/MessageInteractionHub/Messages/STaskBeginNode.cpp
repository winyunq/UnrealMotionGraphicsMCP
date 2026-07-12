// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#include "Messages/STaskBeginNode.h"
#include "SAgentAvatar.h"
#include "Styling/AppStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBox.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/Culture.h"
#include "Widgets/Input/SHyperlink.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Editor.h"

namespace 
{
	FString GetLocString(const FString& English, const FString& Chinese)
	{
		const FString Culture = FInternationalization::Get().GetCurrentCulture()->GetTwoLetterISOLanguageName();
		return Culture.StartsWith(TEXT("zh")) ? Chinese : English;
	}

	TSharedRef<SWidget> BuildAgentBadge(const FString& InAgentName)
	{
		return SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			.BorderBackgroundColor(FLinearColor(0.2f, 0.2f, 0.22f, 1.0f))
			.Padding(FMargin(4.0f, 4.0f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SAgentAvatar)
					.AgentName(InAgentName)
				]
				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.Padding(8.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock)
					.Text(FText::FromString(InAgentName))
					.ColorAndOpacity(FLinearColor(0.85f, 0.85f, 0.85f, 1.0f))
				]
			];
	}
}

void STaskBeginNode::Construct(const FArguments& InArgs)
{
	TaskId = InArgs._TaskId;
	InitiatorAgent = InArgs._InitiatorAgent;
	ReceiverAgent = InArgs._ReceiverAgent;
	TargetAsset = InArgs._TargetAsset;
	Items = InArgs._Items;
	Status = InArgs._Status;
	if (Status.IsEmpty())
	{
		Status = TEXT("Pending");
	}
	OnAccepted = InArgs._OnAccepted;
	OnRejected = InArgs._OnRejected;
	ItemFeedbacks.Init(FString(), Items.Num());

	TSharedPtr<SVerticalBox> ItemsHost;

	TSharedRef<SVerticalBox> MainLayout = SNew(SVerticalBox);

	// 1. Top Row
	TSharedRef<SHorizontalBox> TopRow = SNew(SHorizontalBox);
	TopRow->AddSlot()
		.FillWidth(1.0f)
		[
			BuildAgentBadge(InitiatorAgent)
		];

	TopRow->AddSlot()
		.FillWidth(1.0f)
		.VAlign(VAlign_Center)
		.Padding(8.0f, 0.0f, 8.0f, 0.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("→")))
				.Font(FAppStyle::Get().GetFontStyle("SmallFontBold"))
				.ColorAndOpacity(FLinearColor(0.75f, 0.75f, 0.75f, 1.0f))
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			.Padding(6.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(SHyperlink)
				.Text(this, &STaskBeginNode::GetTargetAssetHyperlinkText)
				.OnNavigate(FSimpleDelegate::CreateRaw(this, &STaskBeginNode::OnHyperlinkNavigate))
				.TextStyle(&FAppStyle::Get().GetWidgetStyle<FTextBlockStyle>("SmallText"))
			]
		];

	TopRow->AddSlot()
		.FillWidth(1.0f)
		[
			BuildAgentBadge(ReceiverAgent)
		];

	MainLayout->AddSlot()
		.AutoHeight()
		[
			TopRow
		];

	// 2. Task Begin Title
	MainLayout->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 8.0f, 0.0f, 4.0f)
		[
			SNew(STextBlock)
			.Text(FText::FromString(TEXT("Task Begin")))
			.Font(FAppStyle::Get().GetFontStyle("SmallFontBold"))
			.ColorAndOpacity(FLinearColor(0.9f, 0.78f, 0.45f, 1.0f))
		];

	// 3. Items Host (Assign ItemsHost)
	MainLayout->AddSlot()
		.AutoHeight()
		[
			SAssignNew(ItemsHost, SVerticalBox)
		];

	// 4. Bottom Row (Buttons & Status Text)
	TSharedRef<SHorizontalBox> BottomRow = SNew(SHorizontalBox);

	// Accept/Reject Buttons Slot
	BottomRow->AddSlot()
		.AutoWidth()
		[
			SNew(SHorizontalBox)
			.Visibility(this, &STaskBeginNode::GetAcceptButtonVisibility)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.0f, 0.0f, 8.0f, 0.0f)
			[
				SNew(SButton)
				.Text(FText::FromString(GetLocString(TEXT("Accept"), TEXT("同意"))))
				.ButtonStyle(FAppStyle::Get(), "PrimaryButton")
				.OnClicked(this, &STaskBeginNode::OnAcceptClicked)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.Text(FText::FromString(GetLocString(TEXT("Reject"), TEXT("拒绝"))))
				.ButtonStyle(FAppStyle::Get(), "Button")
				.OnClicked(this, &STaskBeginNode::OnRejectClicked)
			]
		];

	// Status Text Slot
	BottomRow->AddSlot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Visibility(this, &STaskBeginNode::GetStatusTextVisibility)
			.Text(this, &STaskBeginNode::GetStatusText)
			.ColorAndOpacity(this, &STaskBeginNode::GetStatusTextColor)
			.Font(FAppStyle::Get().GetFontStyle("SmallFontBold"))
		];

	MainLayout->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 10.0f, 0.0f, 0.0f)
		[
			BottomRow
		];

	// 5. Finalize ChildSlot
	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
		.BorderBackgroundColor(FLinearColor(0.15f, 0.15f, 0.15f, 1.0f))
		.Padding(FMargin(10.0f, 8.0f))
		[
			MainLayout
		]
	];

	if (ItemsHost.IsValid())
	{
		for (int32 ItemIndex = 0; ItemIndex < Items.Num(); ++ItemIndex)
		{
			TSharedPtr<SEditableTextBox> FeedbackInput;
			TSharedPtr<STextBlock> FeedbackText;
			TSharedPtr<SButton> AnnotateButton;

			ItemsHost->AddSlot()
			.AutoHeight()
			.Padding(0.0f, 4.0f, 0.0f, 0.0f)
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("ToolPanel.DarkGroupBorder"))
				.Padding(FMargin(8.0f, 6.0f))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.FillWidth(1.0f)
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Text(FText::FromString(FString::Printf(TEXT("%d. %s"), ItemIndex + 1, *FormatTaskItemSingleLine(Items[ItemIndex]))))
							.AutoWrapText(false)
							.ColorAndOpacity(FLinearColor::White)
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(6.0f, 0.0f, 0.0f, 0.0f)
						[
							SAssignNew(AnnotateButton, SButton)
							.ButtonStyle(FAppStyle::Get(), "FlatButton")
							.Text(FText::FromString(GetLocString(TEXT("Annotate"), TEXT("批注"))))
							// Expand logic
						]
					]
				]
			];
		}
	}
}

FString STaskBeginNode::FormatTaskItemSingleLine(const FString& InText)
{
	const FString Flattened = InText.Replace(TEXT("\n"), TEXT(" ")).Replace(TEXT("\t"), TEXT(" "));
	FString Out;
	Out.Reserve(Flattened.Len());
	bool bPrevSpace = false;
	for (TCHAR Ch : Flattened)
	{
		const bool bIsSpace = FChar::IsWhitespace(Ch);
		if (bIsSpace)
		{
			if (!bPrevSpace) Out.AppendChar(TEXT(' '));
			bPrevSpace = true;
			continue;
		}
		Out.AppendChar(Ch);
		bPrevSpace = false;
	}
	return Out.TrimStartAndEnd();
}

FReply STaskBeginNode::OnAcceptClicked()
{
	Status = TEXT("Accepted");
	OnAccepted.ExecuteIfBound();
	return FReply::Handled();
}

FReply STaskBeginNode::OnRejectClicked()
{
	Status = TEXT("Rejected");
	OnRejected.ExecuteIfBound();
	return FReply::Handled();
}

void STaskBeginNode::OnHyperlinkNavigate()
{
	FString CleanAssetPath = TargetAsset;
	if (TargetAsset.Contains(TEXT("AssetTarget=")))
	{
		FString Left, Right;
		if (TargetAsset.Split(TEXT(";"), &Left, &Right))
		{
			Left.Split(TEXT("="), nullptr, &CleanAssetPath);
		}
		else
		{
			TargetAsset.Split(TEXT("="), nullptr, &CleanAssetPath);
		}
		CleanAssetPath = CleanAssetPath.TrimStartAndEnd();
	}

	if (!CleanAssetPath.IsEmpty() && CleanAssetPath != TEXT("(None)"))
	{
		UObject* AssetObj = StaticLoadObject(UObject::StaticClass(), nullptr, *CleanAssetPath);
		if (AssetObj && GEditor)
		{
			if (UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>())
			{
				AssetEditorSubsystem->OpenEditorForAsset(AssetObj);
			}
		}
	}
}

EVisibility STaskBeginNode::GetAcceptButtonVisibility() const
{
	return Status.Equals(TEXT("Pending"), ESearchCase::IgnoreCase) ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility STaskBeginNode::GetStatusTextVisibility() const
{
	return !Status.Equals(TEXT("Pending"), ESearchCase::IgnoreCase) ? EVisibility::Visible : EVisibility::Collapsed;
}

FText STaskBeginNode::GetStatusText() const
{
	if (Status.Equals(TEXT("Accepted"), ESearchCase::IgnoreCase))
	{
		return FText::FromString(GetLocString(TEXT("Accepted (Running)"), TEXT("已同意 (执行中)")));
	}
	else if (Status.Equals(TEXT("Rejected"), ESearchCase::IgnoreCase))
	{
		return FText::FromString(GetLocString(TEXT("Rejected"), TEXT("已拒绝")));
	}
	else if (Status.Equals(TEXT("Ended"), ESearchCase::IgnoreCase))
	{
		return FText::FromString(GetLocString(TEXT("Accepted (Completed)"), TEXT("已同意 (已完成)")));
	}
	return FText::GetEmpty();
}

FSlateColor STaskBeginNode::GetStatusTextColor() const
{
	if (Status.Equals(TEXT("Accepted"), ESearchCase::IgnoreCase)) return FSlateColor(FLinearColor(0.2f, 0.8f, 0.2f, 1.0f));
	if (Status.Equals(TEXT("Rejected"), ESearchCase::IgnoreCase)) return FSlateColor(FLinearColor(0.8f, 0.2f, 0.2f, 1.0f));
	return FSlateColor(FLinearColor(0.5f, 0.5f, 0.5f, 1.0f));
}

void STaskBeginNode::SetTargetAsset(const FString& InTargetAsset)
{
	TargetAsset = InTargetAsset;
}

FText STaskBeginNode::GetTargetAssetHyperlinkText() const
{
	return FText::FromString(FString::Printf(TEXT("Target: %s"), TargetAsset.IsEmpty() ? TEXT("(None)") : *TargetAsset));
}

