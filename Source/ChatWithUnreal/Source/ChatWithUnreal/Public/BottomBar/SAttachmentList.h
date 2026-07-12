// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

DECLARE_DELEGATE_RetVal_TwoParams(class UTexture2D*, FOnGetOrCreateDynamicTexture, const FString& /*Base64*/, const FString& /*Key*/);
DECLARE_DELEGATE_OneParam(FOnAttachmentAdded, const FString& /*ImageId*/);
DECLARE_DELEGATE_OneParam(FOnAttachmentRemoved, const FString& /*ImageId*/);

struct FAttachmentItem
{
	FString ImageId;
	FString Base64Data;
	TSharedPtr<struct FSlateDynamicImageBrush> Brush;
};

/**
 * SAttachmentList
 */
class CHATWITHUNREAL_API SAttachmentList : public SCompoundWidget
{
public:
	static TWeakPtr<SAttachmentList> Instance;

public:
	SLATE_BEGIN_ARGS(SAttachmentList) {}
		SLATE_EVENT(FOnAttachmentAdded, OnAttachmentAdded)
		SLATE_EVENT(FOnAttachmentRemoved, OnAttachmentRemoved)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SAttachmentList();

	void AddAttachment(const FString& Base64, const FString& ImageId = TEXT(""));
	void RemoveAttachmentById(const FString& ImageId);
	void ClearAttachments();

	TArray<FString> GetBase64Images() const;
	const TArray<FAttachmentItem>& GetAttachmentItems() const { return AttachedImages; }

	FOnGetOrCreateDynamicTexture OnGetOrCreateDynamicTextureEvent;
	FSimpleDelegate OnClearTextureCacheEvent;

private:
	void RefreshList();

	TArray<FAttachmentItem> AttachedImages;
	TSharedPtr<class SHorizontalBox> ListContainer;
	int32 ImageIndexCounter = 0;

	FOnAttachmentAdded OnAttachmentAddedEvent;
	FOnAttachmentRemoved OnAttachmentRemovedEvent;
};
