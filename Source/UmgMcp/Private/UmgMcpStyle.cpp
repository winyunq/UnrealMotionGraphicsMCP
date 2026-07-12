// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#include "UmgMcpStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "Framework/Application/SlateApplication.h"
#include "Interfaces/IPluginManager.h"
#include "Styling/SlateStyle.h"
#include "Styling/CoreStyle.h"

TSharedPtr< FSlateStyleSet > FUmgMcpStyle::StyleInstance = nullptr;

void FUmgMcpStyle::Initialize()
{
	if (!StyleInstance.IsValid())
	{
		StyleInstance = Create();
		FSlateStyleRegistry::RegisterSlateStyle(*StyleInstance);
	}
}

void FUmgMcpStyle::Shutdown()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(*StyleInstance);
	ensure(StyleInstance.IsUnique());
	StyleInstance.Reset();
}

FName FUmgMcpStyle::GetStyleSetName()
{
	static FName StyleSetName(TEXT("UmgMcpStyle"));
	return StyleSetName;
}

const ISlateStyle& FUmgMcpStyle::Get()
{
	return *StyleInstance;
}

#define IMAGE_BRUSH( RelativePath, ... ) FSlateImageBrush( Style->RootToContentDir(RelativePath, TEXT(".png") ), __VA_ARGS__ )

TSharedRef< FSlateStyleSet > FUmgMcpStyle::Create()
{
	TSharedRef< FSlateStyleSet > Style = MakeShareable(new FSlateStyleSet(GetStyleSetName()));
	TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin("UmgMcp");
	if (Plugin.IsValid())
	{
		Style->SetContentRoot(Plugin->GetBaseDir() / TEXT("Resources"));
	}

	Style->Set("UmgMcp.PluginIcon", new IMAGE_BRUSH(TEXT("Icon128"), FVector2D(128.0f, 128.0f)));

	// --- 恢复原版富文本样式定义 ---
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

	// 代码块 (code)
	FTextBlockStyle CodeText = RichNormal;
	CodeText.SetFont(FCoreStyle::GetDefaultFontStyle("Mono", 9));
	CodeText.SetColorAndOpacity(FLinearColor(0.4f, 0.8f, 1.0f)); 
	Style->Set("code", CodeText);

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

void FUmgMcpStyle::ReloadTextures()
{
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().GetRenderer()->ReloadTextureResources();
	}
}
