// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#include "SAttachmentList.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Styling/AppStyle.h"
#include "Brushes/SlateDynamicImageBrush.h"
#include "Engine/Texture2D.h"
#include "Editor.h"
#include "ImageUtils.h"
#include "Misc/Base64.h"

TWeakPtr<SAttachmentList> SAttachmentList::Instance = nullptr;

void SAttachmentList::Construct(const FArguments& InArgs)
{
	Instance = SharedThis(this);
	OnAttachmentAddedEvent = InArgs._OnAttachmentAdded;
	OnAttachmentRemovedEvent = InArgs._OnAttachmentRemoved;

	ChildSlot
	[
		SAssignNew(ListContainer, SHorizontalBox)
	];
}

SAttachmentList::~SAttachmentList()
{
	ClearAttachments();
}

TArray<FString> SAttachmentList::GetBase64Images() const
{
	TArray<FString> Res;
	for (const auto& Item : AttachedImages)
	{
		Res.Add(Item.Base64Data);
	}
	return Res;
}
void SAttachmentList::ClearAttachments()
{
	for (auto& Item : AttachedImages)
	{
		if (Item.Brush.IsValid())
		{
			if (UTexture2D* Texture = Cast<UTexture2D>(Item.Brush->GetResourceObject()))
			{
				Texture->RemoveFromRoot();
			}
			Item.Brush->ReleaseResource();
		}
	}
	AttachedImages.Empty();

	OnClearTextureCacheEvent.ExecuteIfBound();

	RefreshList();
}

void SAttachmentList::AddAttachment(const FString& InBase64, const FString& InImageId)
{
	FString ImageId = InImageId;
	if (ImageId.IsEmpty())
	{
		ImageIndexCounter++;
		ImageId = FString::Printf(TEXT("image%d"), ImageIndexCounter);
	}

	UTexture2D* Texture = nullptr;
	if (OnGetOrCreateDynamicTextureEvent.IsBound())
	{
		FString CacheKey = FString::Printf(TEXT("Attachment_%s"), *ImageId);
		Texture = OnGetOrCreateDynamicTextureEvent.Execute(InBase64, CacheKey);
	}

	// 如果没有绑定获取贴图的事件，则在本地自主进行解码，且必须调用 AddToRoot 防止被 GC 机制强制回收而产生马赛克格子
	if (!Texture)
	{
		FString CleanBase64 = InBase64;
		int32 CommaIdx = -1;
		if (CleanBase64.FindChar(TEXT(','), CommaIdx))
		{
			CleanBase64 = CleanBase64.RightChop(CommaIdx + 1);
		}

		TArray<uint8> DecodedBytes;
		if (FBase64::Decode(CleanBase64, DecodedBytes))
		{
			Texture = FImageUtils::ImportBufferAsTexture2D(DecodedBytes);
			if (Texture)
			{
				Texture->AddToRoot();
			}
		}
	}

	TSharedPtr<FSlateDynamicImageBrush> Brush = nullptr;
	if (Texture)
	{
		float W = (float)Texture->GetSizeX();
		float H = (float)Texture->GetSizeY();

		float BaseSize = 64.0f;
		float MinEdge = FMath::Min(W, H);
		float Scale = 1.0f;
		if (MinEdge > BaseSize)
		{
			Scale = 1.0f + FMath::Loge(MinEdge / BaseSize);
		}
		float TargetMinEdge = BaseSize * Scale;
		TargetMinEdge = FMath::Clamp(TargetMinEdge, BaseSize, 120.0f);

		float AspectRatio = W / H;
		FVector2D TargetSize;
		if (W <= H)
		{
			TargetSize.X = TargetMinEdge;
			TargetSize.Y = TargetMinEdge / AspectRatio;
		}
		else
		{
			TargetSize.Y = TargetMinEdge;
			TargetSize.X = TargetMinEdge * AspectRatio;
		}

		Brush = MakeShared<FSlateDynamicImageBrush>(Texture, TargetSize, NAME_None);
	}

	AttachedImages.Add({ ImageId, InBase64, Brush });
	RefreshList();

	OnAttachmentAddedEvent.ExecuteIfBound(ImageId);
}

void SAttachmentList::RemoveAttachmentById(const FString& InImageId)
{
	for (int32 i = 0; i < AttachedImages.Num(); ++i)
	{
		if (AttachedImages[i].ImageId == InImageId)
		{
			if (AttachedImages[i].Brush.IsValid())
			{
				if (UTexture2D* Texture = Cast<UTexture2D>(AttachedImages[i].Brush->GetResourceObject()))
				{
					Texture->RemoveFromRoot();
				}
				AttachedImages[i].Brush->ReleaseResource();
			}
			OnAttachmentRemovedEvent.ExecuteIfBound(InImageId);
			AttachedImages.RemoveAt(i);
			break;
		}
	}
	RefreshList();
}

void SAttachmentList::RefreshList()
{
	ListContainer->ClearChildren();

	for (int32 i = 0; i < AttachedImages.Num(); ++i)
	{
		const auto& Item = AttachedImages[i];

		TSharedRef<SWidget> ImageWidget = SNew(SImage)
			.Image(FAppStyle::Get().GetBrush("Icons.Image"));

		FVector2D BoxSize(64.0f, 64.0f);

		if (Item.Brush.IsValid())
		{
			ImageWidget = SNew(SImage)
				.Image(Item.Brush.Get());
			BoxSize = Item.Brush->ImageSize;
		}

		ListContainer->AddSlot()
		.AutoWidth()
		.VAlign(VAlign_Bottom)
		.Padding(6.0f, 4.0f)
		[
			SNew(SBox)
			.WidthOverride(BoxSize.X)
			.HeightOverride(BoxSize.Y)
			[
				SNew(SOverlay)
				
				+ SOverlay::Slot()
				[
					SNew(SBorder)
					.BorderImage(FAppStyle::GetBrush("DetailsView.CategoryTop"))
					.Padding(0.0f)
					.BorderBackgroundColor(FLinearColor(0.15f, 0.15f, 0.15f, 1.0f))
					[
						ImageWidget
					]
				]

				+ SOverlay::Slot()
				.VAlign(VAlign_Bottom)
				[
					SNew(SBorder)
					.BorderImage(FAppStyle::GetBrush("WhiteBrush"))
					.BorderBackgroundColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.6f))
					.Padding(FMargin(2.0f, 1.0f))
					.HAlign(HAlign_Center)
					[
						SNew(STextBlock)
						.Text(FText::FromString(Item.ImageId))
						.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
						.ColorAndOpacity(FLinearColor(0.9f, 0.9f, 0.95f, 1.0f))
					]
				]

				+ SOverlay::Slot()
				.HAlign(HAlign_Right)
				.VAlign(VAlign_Top)
				[
					SNew(SButton)
					.ButtonStyle(FAppStyle::Get(), "HoverHintOnly")
					.ContentPadding(0.0f)
					.OnClicked_Lambda([this, Item]() {
						this->RemoveAttachmentById(Item.ImageId);
						return FReply::Handled();
					})
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
		];
	}

	SetVisibility(AttachedImages.Num() > 0 ? EVisibility::Visible : EVisibility::Collapsed);
}
