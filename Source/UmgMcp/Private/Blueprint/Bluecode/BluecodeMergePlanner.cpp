// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#include "Blueprint/Bluecode/BluecodeMergePlanner.h"

FBluecodeMergePlan FBluecodeMergePlanner::PlanUnion(
	const TArray<FBluecodeMergeStatement>& Existing,
	const TArray<FBluecodeMergeStatement>& Patch)
{
	FBluecodeMergePlan Plan;
	const int32 ExistingCount = Existing.Num();
	const int32 PatchCount = Patch.Num();

	// LCS gives duplicate statements a stable neighborhood-aware alignment instead
	// of treating a normalized string as a globally unique node identity.
	TArray<int32> Scores;
	Scores.Init(0, (ExistingCount + 1) * (PatchCount + 1));
	auto At = [&](const int32 ExistingIndex, const int32 PatchIndex) -> int32&
	{
		return Scores[ExistingIndex * (PatchCount + 1) + PatchIndex];
	};
	for (int32 ExistingIndex = ExistingCount - 1; ExistingIndex >= 0; --ExistingIndex)
	{
		for (int32 PatchIndex = PatchCount - 1; PatchIndex >= 0; --PatchIndex)
		{
			if (!Existing[ExistingIndex].Normalized.IsEmpty() &&
				Existing[ExistingIndex].Normalized == Patch[PatchIndex].Normalized)
			{
				At(ExistingIndex, PatchIndex) = 1 + At(ExistingIndex + 1, PatchIndex + 1);
			}
			else
			{
				At(ExistingIndex, PatchIndex) = FMath::Max(
					At(ExistingIndex + 1, PatchIndex),
					At(ExistingIndex, PatchIndex + 1));
			}
		}
	}

	TMap<int32, int32> PatchToExisting;
	int32 ExistingIndex = 0;
	int32 PatchIndex = 0;
	while (ExistingIndex < ExistingCount && PatchIndex < PatchCount)
	{
		if (!Existing[ExistingIndex].Normalized.IsEmpty() &&
			Existing[ExistingIndex].Normalized == Patch[PatchIndex].Normalized)
		{
			PatchToExisting.Add(PatchIndex, ExistingIndex);
			++ExistingIndex;
			++PatchIndex;
		}
		else if (At(ExistingIndex + 1, PatchIndex) >= At(ExistingIndex, PatchIndex + 1))
		{
			++ExistingIndex;
		}
		else
		{
			++PatchIndex;
		}
	}

	for (PatchIndex = 0; PatchIndex < PatchCount; ++PatchIndex)
	{
		FBluecodeMergeOperation Operation;
		Operation.PatchIndex = PatchIndex;
		Operation.Statement = Patch[PatchIndex].Statement;

		if (const int32* MatchedExistingIndex = PatchToExisting.Find(PatchIndex))
		{
			Operation.Kind = EBluecodeMergeOperationKind::Noop;
			Operation.MatchedNodeId = Existing[*MatchedExistingIndex].NodeId;
			Plan.Operations.Add(MoveTemp(Operation));
			continue;
		}

		for (int32 RightPatchIndex = PatchIndex + 1; RightPatchIndex < PatchCount; ++RightPatchIndex)
		{
			const int32* RightExistingIndex = PatchToExisting.Find(RightPatchIndex);
			if (!RightExistingIndex)
			{
				continue;
			}
			Operation.RightAnchorNodeId = Existing[*RightExistingIndex].NodeId;
			break;
		}

		if (!Operation.RightAnchorNodeId.IsEmpty())
		{
			Operation.Kind = EBluecodeMergeOperationKind::InsertBefore;
		}
		else
		{
			Operation.Kind = EBluecodeMergeOperationKind::Append;
		}
		Plan.Operations.Add(MoveTemp(Operation));
	}

	return Plan;
}
