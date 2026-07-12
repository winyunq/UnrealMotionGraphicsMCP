// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#include "Messages/SUserMessageWidget.h"
#include "SUserAvatar.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Text/SRichTextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Images/SImage.h"
#include "Styling/AppStyle.h"
#include "ChatWithUnrealStyle.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Brushes/SlateDynamicImageBrush.h"
#include "Engine/Texture2D.h"
#include "Editor.h"
#include "ImageUtils.h"

// Slate 富文本及排版框架
#include "Framework/Text/ITextDecorator.h"
#include "Framework/Text/SlateTextRun.h"
#include "Framework/Text/IRun.h"

namespace
{
	static FString CleanImageTags(const FString& InText)
	{
		FString Result = InText;
		int32 StartIdx = 0;
		while ((StartIdx = Result.Find(TEXT("<WinyunqImageBegin>"))) != INDEX_NONE)
		{
			int32 EndIdx = Result.Find(TEXT("<WinyunqImageEnd>"), ESearchCase::CaseSensitive, ESearchDir::FromStart, StartIdx);
			if (EndIdx != INDEX_NONE)
			{
				Result.RemoveAt(StartIdx, (EndIdx + 17) - StartIdx);
			}
			else
			{
				Result.RemoveAt(StartIdx, 19);
			}
		}
		return Result;
	}

	static FString ConvertMarkdownToRichText(const FString& InText)
	{
		if (InText.IsEmpty()) return FString();

		FString CleanText = InText;
		int32 BeginIdx = 0;
		while ((BeginIdx = CleanText.Find(TEXT("<WinyunqImageBegin>"))) != INDEX_NONE)
		{
			int32 EndIdx = CleanText.Find(TEXT("<WinyunqImageEnd>"), ESearchCase::CaseSensitive, ESearchDir::FromStart, BeginIdx);
			if (EndIdx != INDEX_NONE)
			{
				FString ImageId = CleanText.Mid(BeginIdx + 19, EndIdx - (BeginIdx + 19));
				CleanText.RemoveAt(BeginIdx, EndIdx + 17 - BeginIdx);
				CleanText.InsertAt(BeginIdx, FString::Printf(TEXT(" [📷 %s] "), *ImageId));
			}
			else
			{
				CleanText.RemoveAt(BeginIdx, 19);
			}
		}

		// 1. 转义关键字符 (HTML 实体化)
		FString Escaped = CleanText;
		Escaped = Escaped.Replace(TEXT("&"), TEXT("&amp;"));
		Escaped = Escaped.Replace(TEXT("<"), TEXT("&lt;"));
		Escaped = Escaped.Replace(TEXT(">"), TEXT("&gt;"));

		TArray<FString> Lines;
		Escaped.ParseIntoArrayLines(Lines, false);

		auto ApplyInlineMD = [](FString& Str, const FString& MDTag, const FString& RichTag) {
			int32 StartIdx = 0;
			while (StartIdx < Str.Len())
			{
				int32 OpenPos = Str.Find(MDTag, ESearchCase::CaseSensitive, ESearchDir::FromStart, StartIdx);
				if (OpenPos == INDEX_NONE) break;

				int32 ClosePos = Str.Find(MDTag, ESearchCase::CaseSensitive, ESearchDir::FromStart, OpenPos + MDTag.Len());
				if (ClosePos == INDEX_NONE)
				{
					StartIdx = OpenPos + MDTag.Len();
					continue;
				}

				FString InsideText = Str.Mid(OpenPos + MDTag.Len(), ClosePos - (OpenPos + MDTag.Len()));
				FString Replacement = FString::Printf(TEXT("<%s>%s</>"), *RichTag, *InsideText);

				int32 TotalLengthToRemove = ClosePos - OpenPos + MDTag.Len();
				Str.RemoveAt(OpenPos, TotalLengthToRemove);
				Str.InsertAt(OpenPos, Replacement);

				StartIdx = OpenPos + Replacement.Len();
			}
			};

		auto StripInlineMD = [](FString& Str) {
			Str = Str.Replace(TEXT("***"), TEXT("")).Replace(TEXT("**"), TEXT("")).Replace(TEXT("*"), TEXT(""));
			Str = Str.Replace(TEXT("~~"), TEXT("")).Replace(TEXT("`"), TEXT("")).Replace(TEXT("__"), TEXT(""));
			};

		for (FString& Line : Lines)
		{
			FString TrimmedLine = Line.TrimEnd();
			if (TrimmedLine.IsEmpty()) continue;

			FString FinalLine;

			if (TrimmedLine.StartsWith(TEXT("---")) || TrimmedLine.StartsWith(TEXT("***")))
			{
				FString Temp = TrimmedLine.Replace(TEXT("-"), TEXT("")).Replace(TEXT("*"), TEXT("")).Replace(TEXT(" "), TEXT(""));
				if (Temp.IsEmpty() && TrimmedLine.Len() >= 3)
				{
					FinalLine = TEXT("<hr style=\"hr\">\u200B</>");
					Line = FinalLine;
					continue;
				}
			}

			int32 HeaderLevel = 0;
			while (HeaderLevel < TrimmedLine.Len() && TrimmedLine[HeaderLevel] == '#' && HeaderLevel < 6)
			{
				HeaderLevel++;
			}
			if (HeaderLevel > 0 && TrimmedLine.Len() > HeaderLevel && TrimmedLine[HeaderLevel] == ' ')
			{
				FString HeaderText = TrimmedLine.Mid(HeaderLevel + 1).TrimStartAndEnd();
				StripInlineMD(HeaderText);
				FinalLine = FString::Printf(TEXT("<h%d style=\"h%d\">%s</>"), HeaderLevel, HeaderLevel, *HeaderText);
				Line = FinalLine;
				continue;
			}

			if (TrimmedLine.StartsWith(TEXT("> ")))
			{
				FString QuoteText = TrimmedLine.Mid(2).TrimStartAndEnd();
				bool bHasInlineStyles = QuoteText.Contains(TEXT("*")) || QuoteText.Contains(TEXT("_")) || QuoteText.Contains(TEXT("`"));
				ApplyInlineMD(QuoteText, TEXT("***"), TEXT("bi"));
				ApplyInlineMD(QuoteText, TEXT("**"), TEXT("b"));
				ApplyInlineMD(QuoteText, TEXT("*"), TEXT("i"));
				ApplyInlineMD(QuoteText, TEXT("`"), TEXT("code"));
				
				if (bHasInlineStyles)
				{
					FinalLine = FString::Printf(TEXT("▎ %s"), *QuoteText);
				}
				else
				{
					FinalLine = FString::Printf(TEXT("<quote style=\"quote\">%s</>"), *QuoteText);
				}
				Line = FinalLine;
				continue;
			}

			ApplyInlineMD(Line, TEXT("***"), TEXT("bi"));
			ApplyInlineMD(Line, TEXT("**"), TEXT("b"));
			ApplyInlineMD(Line, TEXT("*"), TEXT("i"));
			ApplyInlineMD(Line, TEXT("`"), TEXT("code"));
		}

		return FString::Join(Lines, TEXT("\n"));
	}

	class FSimpleRichTextDecorator : public ITextDecorator
	{
	public:
		static TSharedRef<FSimpleRichTextDecorator> Create(const ISlateStyle* InStyleSet)
		{
			return MakeShareable(new FSimpleRichTextDecorator(InStyleSet));
		}

		virtual bool Supports(const FTextRunParseResults& RunParseResults, const FString& Text) const override
		{
			return StyleSet && StyleSet->HasWidgetStyle<FTextBlockStyle>(FName(*RunParseResults.Name));
		}

		virtual TSharedRef<ISlateRun> Create(const TSharedRef<class FTextLayout>& TextLayout, const FTextRunParseResults& RunParseResults, const FString& OriginalText, const TSharedRef<FString>& InOutModelText, const ISlateStyle* InStyleSet) override
		{
			FTextRange ModelRange;
			ModelRange.BeginIndex = InOutModelText->Len();

			FString RunContent = OriginalText.Mid(RunParseResults.ContentRange.BeginIndex, RunParseResults.ContentRange.EndIndex - RunParseResults.ContentRange.BeginIndex);
			*InOutModelText += RunContent;

			ModelRange.EndIndex = InOutModelText->Len();

			// 物理直接从指定的 InStyleSet 中获取对应 Tag 名字的富文本样式
			FTextBlockStyle Style = InStyleSet->GetWidgetStyle<FTextBlockStyle>(FName(*RunParseResults.Name));

			FRunInfo RunInfo;
			RunInfo.Name = RunParseResults.Name;
			for (const auto& MetaPair : RunParseResults.MetaData)
			{
				const FTextRange& Range = MetaPair.Value;
				FString MetaValue = OriginalText.Mid(Range.BeginIndex, Range.EndIndex - Range.BeginIndex);
				RunInfo.MetaData.Add(MetaPair.Key, MetaValue);
			}

			TSharedRef<const FString> ConstModelText = InOutModelText;
			return FSlateTextRun::Create(RunInfo, ConstModelText, Style, ModelRange);
		}

	private:
		FSimpleRichTextDecorator(const ISlateStyle* InStyleSet) : StyleSet(InStyleSet) {}
		const ISlateStyle* StyleSet;
	};
}

void SUserMessageWidget::Construct(const FArguments& InArgs)
{
	MessageText = InArgs._MessageText;
	Base64Images = InArgs._Base64Images;

	TArray<TSharedRef<class ITextDecorator>> Decorators;
	Decorators.Add(FSimpleRichTextDecorator::Create(&FChatWithUnrealStyle::Get()));

	FString DisplayName = TEXT("User");

	TSharedPtr<SVerticalBox> BubbleContentBox = SNew(SVerticalBox);

	// ---- 自然对数解析图文混排的核心算法 ----
	FString ParsingText = MessageText;
	int32 CurrentPos = 0;

	while (CurrentPos < ParsingText.Len())
	{
		int32 BeginIdx = ParsingText.Find(TEXT("<WinyunqImageBegin>"), ESearchCase::CaseSensitive, ESearchDir::FromStart, CurrentPos);
		if (BeginIdx == INDEX_NONE)
		{
			FString NormalPart = CleanImageTags(ParsingText.RightChop(CurrentPos)).TrimStartAndEnd();
			if (!NormalPart.IsEmpty())
			{
				BubbleContentBox->AddSlot()
				.AutoHeight()
				.Padding(0.0f, 2.0f)
				[
					SNew(SRichTextBlock)
					.Text(FText::FromString(ConvertMarkdownToRichText(NormalPart)))
					.TextStyle(&FChatWithUnrealStyle::Get().GetWidgetStyle<FTextBlockStyle>("default"))
					.DecoratorStyleSet(&FChatWithUnrealStyle::Get())
					.Decorators(Decorators)
					.AutoWrapText(true)
					.WrapTextAt(0.0f)
				];
			}
			break;
		}

		if (BeginIdx > CurrentPos)
		{
			FString NormalPart = CleanImageTags(ParsingText.Mid(CurrentPos, BeginIdx - CurrentPos)).TrimStartAndEnd();
			if (!NormalPart.IsEmpty())
			{
				BubbleContentBox->AddSlot()
				.AutoHeight()
				.Padding(0.0f, 2.0f)
				[
					SNew(SRichTextBlock)
					.Text(FText::FromString(ConvertMarkdownToRichText(NormalPart)))
					.TextStyle(&FChatWithUnrealStyle::Get().GetWidgetStyle<FTextBlockStyle>("default"))
					.DecoratorStyleSet(&FChatWithUnrealStyle::Get())
					.Decorators(Decorators)
					.AutoWrapText(true)
					.WrapTextAt(0.0f)
				];
			}
		}

		int32 ContentStartIdx = BeginIdx + 19; // strlen("<WinyunqImageBegin>") = 19
		int32 EndIdx = ParsingText.Find(TEXT("<WinyunqImageEnd>"), ESearchCase::CaseSensitive, ESearchDir::FromStart, ContentStartIdx);
		if (EndIdx == INDEX_NONE)
		{
			// 破损标签安全处理：跳过起始标记，避免泄露
			CurrentPos = BeginIdx + 19;
			continue;
		}

		FString ImageId = ParsingText.Mid(ContentStartIdx, EndIdx - ContentStartIdx);
		int32 ImageIndex = -1;
		if (ImageId.StartsWith(TEXT("image")))
		{
			ImageIndex = FCString::Atoi(*ImageId.RightChop(5)) - 1;
		}

		if (ImageIndex >= 0 && ImageIndex < Base64Images.Num())
		{
			const FString& Base64Str = Base64Images[ImageIndex];
			UTexture2D* Texture = nullptr;
			
			// 纯前端本地导入 Base64 为 UTexture2D 预览气泡，不经过后端 Cache Subsystem 强耦合
			FString CleanBase64 = Base64Str;
			int32 CommaIdx = -1;
			if (CleanBase64.FindChar(TEXT(','), CommaIdx))
			{
				CleanBase64 = CleanBase64.RightChop(CommaIdx + 1);
			}

			TArray<uint8> DecodedBytes;
			if (FBase64::Decode(CleanBase64, DecodedBytes))
			{
				Texture = FImageUtils::ImportBufferAsTexture2D(DecodedBytes);
			}

			if (Texture)
			{
				Texture->AddToRoot();
				float W = Texture->GetSizeX();
				float H = Texture->GetSizeY();

				// 对数比例算法计算最舒服的气泡图尺寸
				float BaseSize = 64.0f;
				float MinEdge = FMath::Min(W, H);
				float Scale = 1.0f;
				if (MinEdge > BaseSize)
				{
					Scale = 1.0f + FMath::Loge(MinEdge / BaseSize);
				}
				float TargetMinEdge = BaseSize * Scale;
				TargetMinEdge = FMath::Clamp(TargetMinEdge, BaseSize, 180.0f); // 气泡内最大边限定在 180px

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

				TSharedPtr<FSlateDynamicImageBrush> DynamicBrush = MakeShareable(new FSlateDynamicImageBrush(Texture, TargetSize, NAME_None));
				AttachedSlateBrushes.Add(DynamicBrush);

				BubbleContentBox->AddSlot()
				.AutoHeight()
				.Padding(0.0f, 6.0f)
				[
					SNew(SBox)
					.WidthOverride(TargetSize.X)
					.HeightOverride(TargetSize.Y)
					[
						SNew(SBorder)
						.BorderImage(FAppStyle::GetBrush("DetailsView.CategoryTop"))
						.Padding(0.0f)
						.BorderBackgroundColor(FLinearColor(0.1f, 0.1f, 0.1f, 0.5f))
						[
							SNew(SImage)
							.Image(DynamicBrush.Get())
						]
					]
				];
			}
		}

		CurrentPos = EndIdx + 17; // strlen("<WinyunqImageEnd>") = 17
	}

	ChildSlot
	[
		SNew(SHorizontalBox)
		// 1. 左侧占位实现右对齐 (Push everything to the right)
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		[
			SNew(SSpacer)
		]

		// 2. 消息内容列 (Bubble + Action Row)
		+ SHorizontalBox::Slot()
		.FillWidth(2.0f)
		.Padding(FMargin(60.0f, 0.0f, 12.0f, 0.0f))
		[
			SNew(SVerticalBox)
			// 顶部：名字
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Right)
			.Padding(0.0f, 0.0f, 0.0f, 4.0f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(DisplayName))
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
				.ColorAndOpacity(FLinearColor(0.7f, 0.7f, 0.75f, 1.0f))
			]

			// 气泡
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNew(SSpacer)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SBorder)
					.BorderImage(FAppStyle::GetBrush("DetailsView.CategoryTop"))
					.Padding(FMargin(12.0f, 8.0f))
					.BorderBackgroundColor(FLinearColor(0.2f, 0.4f, 0.8f, 1.0f))
					[
						BubbleContentBox.ToSharedRef()
					]
				]
			]

			// 操作条 (复制按钮)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Right)
			.Padding(0.0f, 4.0f, 0.0f, 0.0f)
			[
				SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "SimpleButton")
				.ContentPadding(FMargin(4.0f, 2.0f))
				.OnClicked_Lambda([this]() {
					FString PlainText = CleanImageTags(MessageText).TrimStartAndEnd();
					FPlatformApplicationMisc::ClipboardCopy(*PlainText);
					return FReply::Handled();
				})
				.ToolTipText(FText::FromString(TEXT("Copy text")))
				[
					SNew(SImage)
					.Image(FAppStyle::Get().GetBrush("GenericCommands.Copy"))
					.DesiredSizeOverride(FVector2D(12, 12))
					.ColorAndOpacity(FSlateColor::UseForeground())
				]
			]
		]

		// 3. 右侧头像
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Top)
		[
			SNew(SUserAvatar)
		]
	];
}

SUserMessageWidget::~SUserMessageWidget()
{
	for (auto& Brush : AttachedSlateBrushes)
	{
		if (Brush.IsValid())
		{
			if (UTexture2D* Texture = Cast<UTexture2D>(Brush->GetResourceObject()))
			{
				Texture->RemoveFromRoot();
			}
			Brush->ReleaseResource();
		}
	}
	AttachedSlateBrushes.Empty();
}
