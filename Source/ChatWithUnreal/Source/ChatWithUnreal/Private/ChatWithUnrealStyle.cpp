// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#include "ChatWithUnrealStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "Framework/Application/SlateApplication.h"
#include "Interfaces/IPluginManager.h"
#include "Styling/SlateStyle.h"
#include "Styling/CoreStyle.h"

TSharedPtr< FSlateStyleSet > FChatWithUnrealStyle::StyleInstance = nullptr;

void FChatWithUnrealStyle::Initialize()
{
	if (!StyleInstance.IsValid())
	{
		StyleInstance = Realize();
		FSlateStyleRegistry::RegisterSlateStyle(*StyleInstance);
	}
}

void FChatWithUnrealStyle::Shutdown()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(*StyleInstance);
	ensure(StyleInstance.IsUnique());
	StyleInstance.Reset();
}

FName FChatWithUnrealStyle::GetStyleSetName()
{
	static FName StyleSetName(TEXT("ChatWithUnrealStyle"));
	return StyleSetName;
}

const ISlateStyle& FChatWithUnrealStyle::Get()
{
	return *StyleInstance;
}

#define IMAGE_BRUSH( RelativePath, ... ) FSlateImageBrush( Style->RootToContentDir(RelativePath, TEXT(".png") ), __VA_ARGS__ )

TSharedRef< FSlateStyleSet > FChatWithUnrealStyle::Realize()
{
	TSharedRef< FSlateStyleSet > Style = MakeShareable(new FSlateStyleSet(GetStyleSetName()));
	
	// --- 标准 UE 插件路径获取方式 ---
	TSharedPtr<IPlugin> MyPlugin = IPluginManager::Get().FindPlugin(TEXT("UmgMcp"));
	
	if (MyPlugin.IsValid())
	{
		// 物理根目录：D:\UE5Project\FabUMGMCP\plugins\FabUmgMcp\Resources
		Style->SetContentRoot(MyPlugin->GetBaseDir() / TEXT("Resources"));
	}

	// 统一映射：资源路径自愈
	Style->Set("ChatWithUnreal.PluginIcon", new IMAGE_BRUSH(TEXT("Icon128"), FVector2D(128.0f, 128.0f)));
	Style->Set("ChatWithUnreal.ChatAvatar", new IMAGE_BRUSH(TEXT("Icon/Agent"), FVector2D(40.0f, 40.0f)));
	Style->Set("ChatWithUnreal.Agent.Layout", new IMAGE_BRUSH(TEXT("Icon/layout"), FVector2D(40.0f, 40.0f)));
	Style->Set("ChatWithUnreal.Agent.Material", new IMAGE_BRUSH(TEXT("Icon/material"), FVector2D(40.0f, 40.0f)));
	Style->Set("ChatWithUnreal.Agent.Sequence", new IMAGE_BRUSH(TEXT("Icon/sequence"), FVector2D(40.0f, 40.0f)));
	Style->Set("ChatWithUnreal.Agent.Widget", new IMAGE_BRUSH(TEXT("Icon/widget"), FVector2D(40.0f, 40.0f)));
	Style->Set("ChatWithUnreal.Agent.Agent", new IMAGE_BRUSH(TEXT("Icon/Agent"), FVector2D(40.0f, 40.0f)));

		// --- 统一富文本样式注册 (FChatWithUnrealStyle 独有) ---
	const FTextBlockStyle& NormalText = FAppStyle::Get().GetWidgetStyle<FTextBlockStyle>("NormalText");
    
	// 基础正文
	FTextBlockStyle RichNormal = NormalText;
	RichNormal.SetFontSize(10);
	RichNormal.SetColorAndOpacity(FLinearColor(0.82f, 0.82f, 0.85f, 1.0f)); // 雅致微灰柔和正文
	Style->Set("default", RichNormal); 

	// 加粗 (b)
	FTextBlockStyle BoldText = RichNormal;
	BoldText.SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 10));
	BoldText.SetColorAndOpacity(FLinearColor::White); // 耀眼纯白，拉开明暗视觉对比
	Style->Set("b", BoldText);

	// 斜体 (i)
	FTextBlockStyle ItalicText = RichNormal;
	ItalicText.SetFont(FCoreStyle::GetDefaultFontStyle("Italic", 10));
	Style->Set("i", ItalicText);

	// 粗斜体 (bi)
	FTextBlockStyle BoldItalicText = RichNormal;
	BoldItalicText.SetFont(FCoreStyle::GetDefaultFontStyle("BoldItalic", 10));
	BoldItalicText.SetColorAndOpacity(FLinearColor::White);
	Style->Set("bi", BoldItalicText);

	// 代码块 (code)
	FTextBlockStyle CodeText = RichNormal;
	CodeText.SetFont(FCoreStyle::GetDefaultFontStyle("Mono", 9));
	CodeText.SetColorAndOpacity(FLinearColor(0.4f, 0.8f, 1.0f)); 
	Style->Set("code", CodeText);

	// 引用 (quote)
	FTextBlockStyle QuoteTextStyle = ItalicText;
	QuoteTextStyle.SetColorAndOpacity(FLinearColor(0.55f, 0.55f, 0.6f, 1.0f));
	Style->Set("quote", QuoteTextStyle);

	// 列表项 (li)
	Style->Set("li", RichNormal);

	// 标题 (h1, h2, h3, h4, h5)
	FTextBlockStyle H1 = BoldText;
	H1.SetFontSize(14);
	H1.SetColorAndOpacity(FLinearColor(1.0f, 0.7f, 0.1f));
	Style->Set("h1", H1);

	FTextBlockStyle H2 = BoldText;
	H2.SetFontSize(12);
	H2.SetColorAndOpacity(FLinearColor(1.0f, 0.8f, 0.2f));
	Style->Set("h2", H2);

	FTextBlockStyle H3 = BoldText;
	H3.SetFontSize(11);
	H3.SetColorAndOpacity(FLinearColor(1.0f, 0.9f, 0.3f));
	Style->Set("h3", H3);

	FTextBlockStyle H4 = BoldText;
	H4.SetFontSize(10);
	H4.SetColorAndOpacity(FLinearColor(0.8f, 0.9f, 0.4f));
	Style->Set("h4", H4);

	FTextBlockStyle H5 = BoldText;
	H5.SetFontSize(9);
	H5.SetColorAndOpacity(FLinearColor(0.7f, 0.8f, 0.5f));
	Style->Set("h5", H5);

	// 分割线样式
	FTextBlockStyle HR = RichNormal;
	HR.SetFontSize(8);
	HR.SetColorAndOpacity(FLinearColor(0.3f, 0.3f, 0.3f));
	Style->Set("hr", HR);

	return Style;
}

#undef IMAGE_BRUSH

void FChatWithUnrealStyle::ReloadTextures()
{
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().GetRenderer()->ReloadTextureResources();
	}
}
