// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateStyle.h"

/**
 * FChatWithUnrealStyle：前端模块的资产具象化实现。
 * 遵循自适应路径原则，不依赖硬编码的插件名称。
 */
class CHATWITHUNREAL_API FChatWithUnrealStyle
{
public:
    static void Initialize();
    static void Shutdown();
    static void ReloadTextures();
    static const ISlateStyle& Get();
    static FName GetStyleSetName();

private:
    static TSharedRef< class FSlateStyleSet > Realize();

private:
    static TSharedPtr< class FSlateStyleSet > StyleInstance;
};
