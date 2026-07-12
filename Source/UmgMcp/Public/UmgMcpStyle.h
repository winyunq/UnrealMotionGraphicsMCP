// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateStyle.h"

class UMGMCP_API FUmgMcpStyle
{
public:
    static void Initialize();
    static void Shutdown();
    static void ReloadTextures();
    static const ISlateStyle& Get();
    static FName GetStyleSetName();

private:
    static TSharedRef< class FSlateStyleSet > Create();

private:
    static TSharedPtr< class FSlateStyleSet > StyleInstance;
};
