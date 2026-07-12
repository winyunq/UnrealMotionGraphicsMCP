// Copyright (c) 2025-2026 Winyunq. All rights reserved.

#include "ChatWithUnreal.h"
#include "SChatWindow.h"
#include "ChatWithUnrealStyle.h"

void FChatWithUnrealModule::StartupModule()
{
	FChatWithUnrealStyle::Initialize();
}

void FChatWithUnrealModule::ShutdownModule()
{
	FChatWithUnrealStyle::Shutdown();
}

IMPLEMENT_MODULE(FChatWithUnrealModule, ChatWithUnreal)
