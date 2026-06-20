// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "UmgPatchSubsystem.generated.h"

class UWidgetBlueprint;

DECLARE_LOG_CATEGORY_EXTERN(LogUmgPatch, Log, All);

/**
 * Orchestration layer: apply RFC6902-style patch ops inside one undo transaction.
 */
UCLASS()
class UMGMCP_API UUmgPatchSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** Apply patch array atomically; increments attention revision on success. */
	FString ApplyPatch(
		UWidgetBlueprint* WidgetBlueprint,
		const TArray<TSharedPtr<FJsonValue>>& PatchOps,
		TOptional<int32> ExpectedRevision = TOptional<int32>());

private:
	struct FPatchPathInfo
	{
		FString WidgetName;
		FString PropertyPath;
		FString ParentName;
		bool bIsChildAppend = false;
	};

	bool ParsePatchPath(const FString& Path, const FString& OpType, FPatchPathInfo& OutInfo, FString& OutError) const;
	bool ApplySetOp(UWidgetBlueprint* WidgetBlueprint, const FPatchPathInfo& PathInfo, const TSharedPtr<FJsonValue>& Value, FString& OutError) const;
	bool ApplyAddOp(UWidgetBlueprint* WidgetBlueprint, const FPatchPathInfo& PathInfo, const TSharedPtr<FJsonValue>& Value, FString& OutError) const;
	bool ApplyRemoveOp(UWidgetBlueprint* WidgetBlueprint, const FPatchPathInfo& PathInfo, FString& OutError) const;
};
