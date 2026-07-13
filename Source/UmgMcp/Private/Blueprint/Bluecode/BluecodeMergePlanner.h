// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#pragma once

#include "CoreMinimal.h"

enum class EBluecodeMergeOperationKind : uint8
{
	Noop,
	InsertBefore,
	Append
};

struct FBluecodeMergeStatement
{
	FString Statement;
	FString Normalized;
	FString NodeId;
};

struct FBluecodeMergeOperation
{
	EBluecodeMergeOperationKind Kind = EBluecodeMergeOperationKind::Append;
	int32 PatchIndex = INDEX_NONE;
	FString Statement;
	FString MatchedNodeId;
	FString RightAnchorNodeId;
};

struct FBluecodeMergePlan
{
	TArray<FBluecodeMergeOperation> Operations;
	TArray<FString> Conflicts;
};

/**
 * Pure sequence planner for conservative BlueCode union writes.
 *
 * Existing and patch statements are aligned by semantic-normalized equality using
 * LCS. Unmatched patch statements use the next matched statement as a right anchor;
 * when no right anchor exists they append. The planner never emits deletion.
 */
class FBluecodeMergePlanner
{
public:
	static FBluecodeMergePlan PlanUnion(
		const TArray<FBluecodeMergeStatement>& Existing,
		const TArray<FBluecodeMergeStatement>& Patch);
};
