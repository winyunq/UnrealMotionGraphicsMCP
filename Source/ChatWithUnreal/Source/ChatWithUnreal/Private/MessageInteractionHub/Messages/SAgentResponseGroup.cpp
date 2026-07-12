// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#include "SAgentResponseGroup.h"
#include "SAgentAvatar.h"
#include "ChatWithUnrealStyle.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Text/SRichTextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Images/SImage.h"
#include "Styling/AppStyle.h"
#include "SAgentStatusBar.h"
#include "HAL/PlatformApplicationMisc.h"

// Slate 文本框架头文件
#include "Framework/Text/ITextDecorator.h"
#include "Framework/Text/SlateTextRun.h"
#include "Framework/Text/IRun.h"
#include "CoreMinimal.h"

namespace 
{
	/** 
	 * 深度 Markdown 解析器
	 * 转换语法并清除原始标记，支持 H1-H5, HR, Bold, Italic, Code
	 */

	FString ConvertMarkdownToRichText(const FString& InText)
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
				// 破损标签安全处理：强制将其从富文本段中抹除，防止泄露展示
				CleanText.RemoveAt(BeginIdx, 19);
			}
		}

		// 1. 转义关键字符 (HTML 实体化)
		FString Escaped = CleanText;
		Escaped = Escaped.Replace(TEXT("&"), TEXT("&amp;"));
		Escaped = Escaped.Replace(TEXT("<"), TEXT("&lt;"));
		Escaped = Escaped.Replace(TEXT(">"), TEXT("&gt;"));

		TArray<FString> Lines;
		// 传入 false 保留空行，这样可以维持段落间距
		Escaped.ParseIntoArrayLines(Lines, false);

		// --- 闭包1：严格成对匹配的行内样式解析 ---
		auto ApplyInlineMD = [](FString& Str, const FString& MDTag, const FString& RichTag) {
			int32 StartIdx = 0;
			while (StartIdx < Str.Len())
			{
				// 找起始符号
				int32 OpenPos = Str.Find(MDTag, ESearchCase::CaseSensitive, ESearchDir::FromStart, StartIdx);
				if (OpenPos == INDEX_NONE) break;

				// 找闭合符号
				int32 ClosePos = Str.Find(MDTag, ESearchCase::CaseSensitive, ESearchDir::FromStart, OpenPos + MDTag.Len());
				if (ClosePos == INDEX_NONE)
				{
					// 没有闭合符号，说明不是合法的 Markdown 语法，跳过当前符号
					StartIdx = OpenPos + MDTag.Len();
					continue;
				}

				// 提取中间的文本内容
				FString InsideText = Str.Mid(OpenPos + MDTag.Len(), ClosePos - (OpenPos + MDTag.Len()));
				FString Replacement = FString::Printf(TEXT("<%s>%s</>"), *RichTag, *InsideText);

				// 替换原字符串
				int32 TotalLengthToRemove = ClosePos - OpenPos + MDTag.Len();
				Str.RemoveAt(OpenPos, TotalLengthToRemove);
				Str.InsertAt(OpenPos, Replacement);

				// 更新搜索起始位置
				StartIdx = OpenPos + Replacement.Len();
			}
			};

		// --- 闭包2：清除行内标记（用于标题防嵌套） ---
		auto StripInlineMD = [](FString& Str) {
			Str = Str.Replace(TEXT("***"), TEXT("")).Replace(TEXT("**"), TEXT("")).Replace(TEXT("*"), TEXT(""));
			Str = Str.Replace(TEXT("~~"), TEXT("")).Replace(TEXT("`"), TEXT("")).Replace(TEXT("__"), TEXT(""));
			};

		// 逐行解析
		for (FString& Line : Lines)
		{
			FString TrimmedLine = Line.TrimEnd(); // 保留左侧空格（应对缩进）
			if (TrimmedLine.IsEmpty()) continue;

			FString FinalLine;

			// --- 步骤一：处理块级样式 (Block) ---

			// 1. 分割线 (Horizontal Rule)
			if (TrimmedLine.StartsWith(TEXT("---")) || TrimmedLine.StartsWith(TEXT("***")))
			{
				// 验证这一行是不是只有 - 或 *
				FString Temp = TrimmedLine.Replace(TEXT("-"), TEXT("")).Replace(TEXT("*"), TEXT("")).Replace(TEXT(" "), TEXT(""));
				if (Temp.IsEmpty() && TrimmedLine.Len() >= 3)
				{
					// 【优化】：使用一个不可见字符(零宽空格 \u200B)撑起高度，并赋予专门的 hr 样式
					FinalLine = TEXT("<hr style=\"hr\">\u200B</>");
					// 如果你配置了 URichTextImageDecorator，也可以换成：FinalLine = TEXT("<img id=\"hr\"/>");
					Line = FinalLine;
					continue;
				}
			}

			// 2. 标题 (Headers)
			int32 HeaderLevel = 0;
			while (HeaderLevel < TrimmedLine.Len() && TrimmedLine[HeaderLevel] == '#' && HeaderLevel < 6)
			{
				HeaderLevel++;
			}
			if (HeaderLevel > 0 && TrimmedLine.Len() > HeaderLevel && TrimmedLine[HeaderLevel] == ' ')
			{
				FString HeaderText = TrimmedLine.Mid(HeaderLevel + 1).TrimStartAndEnd();

				// 【核心修复防嵌套】：去除标题内部的 ** 等符号，强制全部使用标题 DataTable 的字体
				StripInlineMD(HeaderText);

				FinalLine = FString::Printf(TEXT("<h%d style=\"h%d\">%s</>"), HeaderLevel, HeaderLevel, *HeaderText);
				Line = FinalLine;
				continue;
			}

			// 3. 引用 (Blockquote)
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

			// 4. 无序列表 (Unordered Lists)
			if (TrimmedLine.StartsWith(TEXT("- ")) || TrimmedLine.StartsWith(TEXT("* ")))
			{
				FString ListText = TrimmedLine.Mid(2).TrimStartAndEnd();
				ApplyInlineMD(ListText, TEXT("***"), TEXT("bi"));
				ApplyInlineMD(ListText, TEXT("**"), TEXT("b"));
				ApplyInlineMD(ListText, TEXT("*"), TEXT("i"));
				ApplyInlineMD(ListText, TEXT("`"), TEXT("code"));
				// 添加项目符号 (圆点)，避免嵌套 <li> 标签，实现物理渲染自愈
				FinalLine = FString::Printf(TEXT("\u2022 %s"), *ListText);
				Line = FinalLine;
				continue;
			}

			// --- 步骤二：处理普通段落的行内样式 (Inline) ---
			ApplyInlineMD(TrimmedLine, TEXT("***"), TEXT("bi"));
			ApplyInlineMD(TrimmedLine, TEXT("**"), TEXT("b"));
			ApplyInlineMD(TrimmedLine, TEXT("__"), TEXT("b")); // 支持 __加粗__
			ApplyInlineMD(TrimmedLine, TEXT("*"), TEXT("i"));
			ApplyInlineMD(TrimmedLine, TEXT("~~"), TEXT("s")); // 删除线
			ApplyInlineMD(TrimmedLine, TEXT("`"), TEXT("code"));

			Line = TrimmedLine;
		}

		return FString::Join(Lines, TEXT("\n"));
	}

	/**
	 * 简易富文本装饰器
	 */
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

void SAgentResponseGroup::Construct(const FArguments& InArgs)
{
	AgentName = InArgs._AgentName;
	MessageText = InArgs._MessageText;

	ChildSlot
	[
		SNew(SHorizontalBox)
		// 1. 左侧头像 - 直接注入组件，由组件自身管理尺寸
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Top)
		.Padding(FMargin(0.0f, 0.0f, 12.0f, 0.0f))
		[
			SNew(SAgentAvatar)
			.AgentName(AgentName)
		]

		// 2. 消息内容区 - 核心：使用 FillWidth 配合 WrapTextAt(0.0f) 实现自动折行
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		[
			SNew(SVerticalBox)
			// 顶部：名字+状态区
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 4.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SAssignNew(AgentNameTextBlock, STextBlock)
					.Text(FText::FromString(AgentName))
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
					.ColorAndOpacity(FLinearColor(0.7f, 0.7f, 0.75f, 1.0f))
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(8.0f, 0.0f, 0.0f, 0.0f)
				[
					SAssignNew(AgentStatusBarWidget, SAgentStatusBar)
					.StatusText("")
					.bShowSpinner(false)
				]
			]
			// 消息气泡展示区
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("DetailsView.CategoryMiddle"))
				.Padding(FMargin(14.0f, 10.0f))
				.BorderBackgroundColor(FLinearColor(0.15f, 0.15f, 0.15f, 1.0f))
				[
					SAssignNew(TurnContentBox, SVerticalBox)
				]
			]
			
			// 底部操作区 (复制按钮)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Left)
			.Padding(0.0f, 6.0f, 0.0f, 0.0f)
			[
				SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "SimpleButton")
				.ContentPadding(FMargin(4.0f, 2.0f))
				.OnClicked_Lambda([this]() {
					FPlatformApplicationMisc::ClipboardCopy(*MessageText);
					return FReply::Handled();
				})
				.ToolTipText(FText::FromString(TEXT("Copy message")))
				[
					SNew(SImage)
					.Image(FAppStyle::GetBrush("GenericCommands.Copy"))
					.DesiredSizeOverride(FVector2D(12, 12))
					.ColorAndOpacity(FLinearColor(0.4f, 0.4f, 0.4f, 0.8f))
				]
			]
		]
	];

	if (!MessageText.IsEmpty())
	{
		AddTextOutputBlock(MessageText);
	}
}

void SAgentResponseGroup::AddTextOutputBlock(const FString& NewText)
{
	MessageText = NewText;
	if (TurnContentBox.IsValid() && !NewText.IsEmpty())
	{
		ActiveTextBlock.Reset();

		TArray<TSharedRef<class ITextDecorator>> Decorators;
		Decorators.Add(FSimpleRichTextDecorator::Create(&FChatWithUnrealStyle::Get()));

		TurnContentBox->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 2.0f, 0.0f, 6.0f)
		[
			SAssignNew(ActiveTextBlock, SRichTextBlock)
			.Text(FText::FromString(ConvertMarkdownToRichText(NewText)))
			.TextStyle(&FChatWithUnrealStyle::Get().GetWidgetStyle<FTextBlockStyle>("default"))
			.DecoratorStyleSet(&FChatWithUnrealStyle::Get())
			.Decorators(Decorators)
			.AutoWrapText(true)
			.WrapTextAt(0.0f)
		];
	}
}

void SAgentResponseGroup::AppendToCurrentTextOutputBlock(const FString& PartialText)
{
	if (PartialText.IsEmpty()) return;
	MessageText += PartialText;

	if (ActiveTextBlock.IsValid())
	{
		ActiveTextBlock->SetText(FText::FromString(ConvertMarkdownToRichText(MessageText)));
	}
	else
	{
		AddTextOutputBlock(PartialText);
	}
}

void SAgentResponseGroup::RemoveWidget(TSharedRef<SWidget> WidgetToRemove)
{
	if (TurnContentBox.IsValid())
	{
		TurnContentBox->RemoveSlot(WidgetToRemove);
	}
}

void SAgentResponseGroup::SetAgentStatus(const FText& StatusText, bool bShowSpinner, const FLinearColor& StatusColor)
{
	if (AgentStatusBarWidget.IsValid())
	{
		AgentStatusBarWidget->SetStatus(StatusText.ToString(), bShowSpinner, StatusColor);
	}
}

void SAgentResponseGroup::SetAgentName(const FString& InName)
{
	AgentName = InName;
	if (AgentNameTextBlock.IsValid())
	{
		AgentNameTextBlock->SetText(FText::FromString(InName));
	}
}

void SAgentResponseGroup::AddToolExecutionBlock(const FString& ToolName, const FText& Status, bool bIsError)
{
	if (TurnContentBox.IsValid())
	{
		ActiveTextBlock.Reset();
		TurnContentBox->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 2.0f)
		[
			SNew(STextBlock)
			.Text(FText::Format(FText::FromString(TEXT("[{0}] {1}")), FText::FromString(ToolName), Status))
			.ColorAndOpacity(bIsError ? FLinearColor::Red : FLinearColor::Yellow)
		];
	}
}

void SAgentResponseGroup::AddToolExecutionWidget(TSharedRef<SWidget> ToolWidget)
{
	if (TurnContentBox.IsValid())
	{
		ActiveTextBlock.Reset();
		TurnContentBox->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 4.0f)
		[
			ToolWidget
		];
	}
}

bool SAgentResponseGroup::IsEmpty() const
{
	return MessageText.IsEmpty() && (!TurnContentBox.IsValid() || TurnContentBox->NumSlots() == 0);
}

FString SAgentResponseGroup::GetAgentName() const
{
	return AgentNameTextBlock.IsValid() ? AgentNameTextBlock->GetText().ToString() : TEXT("");
}
