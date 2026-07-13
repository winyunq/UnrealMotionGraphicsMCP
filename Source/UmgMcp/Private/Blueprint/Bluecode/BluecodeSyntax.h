// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#pragma once

#include "CoreMinimal.h"

/** Quote- and nesting-aware lexical helpers shared by the BlueCode parser. */
class FBluecodeSyntax
{
public:
	/** Returns the first assignment '=' at document depth zero, or INDEX_NONE. */
	static int32 FindTopLevelAssignmentOperator(const FString& Statement);

	/** BlueCode assignment targets are deliberately limited to plain identifiers. */
	static bool IsIdentifier(const FString& Value);
};
