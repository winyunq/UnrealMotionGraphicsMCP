// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#pragma once

#include "CoreMinimal.h"

#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 8)
#include "Containers/SharedString.h"
#endif

namespace UmgMcpJsonCompat
{
	inline FString KeyToString(const FString& Key)
	{
		return Key;
	}

#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 8)
	inline FString KeyToString(const UE::FSharedString& Key)
	{
		return FString(Key.ToView());
	}
#endif
}
