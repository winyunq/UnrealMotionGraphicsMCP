// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#include "Patch/UmgPatchSubsystem.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Editor.h"
#include "FileManage/UmgAttentionSubsystem.h"
#include "ScopedTransaction.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Widget/UmgSetSubsystem.h"
#include "WidgetBlueprint.h"

DEFINE_LOG_CATEGORY(LogUmgPatch);

void UUmgPatchSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	UE_LOG(LogUmgPatch, Log, TEXT("UmgPatchSubsystem initialized."));
}

bool UUmgPatchSubsystem::ParsePatchPath(const FString& Path, const FString& OpType, FPatchPathInfo& OutInfo, FString& OutError) const
{
	OutInfo = FPatchPathInfo();
	OutError.Empty();

	FString Normalized = Path;
	Normalized.TrimStartAndEndInline();
	if (!Normalized.StartsWith(TEXT("/widgets/")))
	{
		OutError = FString::Printf(TEXT("Invalid patch path (must start with /widgets/): %s"), *Path);
		return false;
	}

	Normalized = Normalized.Mid(9);
	TArray<FString> Segments;
	Normalized.ParseIntoArray(Segments, TEXT("/"), true);
	if (Segments.Num() < 1 || Segments[0].IsEmpty())
	{
		OutError = TEXT("Missing widget name in patch path.");
		return false;
	}

	OutInfo.WidgetName = Segments[0];
	OutInfo.ParentName = Segments[0];

	if (OpType == TEXT("remove"))
	{
		if (Segments.Num() != 1)
		{
			OutError = TEXT("remove op path must be exactly /widgets/{WidgetName} (elimination only).");
			return false;
		}
		return true;
	}

	if (Segments.Num() == 1)
	{
		if (OpType == TEXT("set"))
		{
			OutError = TEXT("set op path must include /properties/{PropertyName}.");
			return false;
		}
		OutError = FString::Printf(TEXT("Unsupported single-segment path for op '%s'."), *OpType);
		return false;
	}

	if (Segments[1] == TEXT("properties"))
	{
		if (OpType != TEXT("set"))
		{
			OutError = FString::Printf(TEXT("properties path is only valid for set op, not '%s'."), *OpType);
			return false;
		}
		if (Segments.Num() < 3)
		{
			OutError = TEXT("set op path must include a property name after /properties/.");
			return false;
		}

		OutInfo.PropertyPath = Segments[2];
		for (int32 Index = 3; Index < Segments.Num(); ++Index)
		{
			OutInfo.PropertyPath += TEXT(".") + Segments[Index];
		}
		return true;
	}

	if (Segments[1] == TEXT("children"))
	{
		if (OpType != TEXT("add"))
		{
			OutError = FString::Printf(TEXT("children path is only valid for add op, not '%s'."), *OpType);
			return false;
		}
		OutInfo.bIsChildAppend = Segments.Num() >= 3 && Segments[2] == TEXT("-");
		if (!OutInfo.bIsChildAppend)
		{
			OutError = TEXT("add op path must be /widgets/{Parent}/children/-.");
			return false;
		}
		return true;
	}

	OutError = FString::Printf(TEXT("Unsupported path segments in: %s"), *Path);
	return false;
}

bool UUmgPatchSubsystem::ApplySetOp(UWidgetBlueprint* WidgetBlueprint, const FPatchPathInfo& PathInfo, const TSharedPtr<FJsonValue>& Value, FString& OutError) const
{
	if (PathInfo.PropertyPath.IsEmpty() || !Value.IsValid())
	{
		OutError = TEXT("Invalid set op: missing property path or value.");
		return false;
	}

	UUmgSetSubsystem* SetSubsystem = GEditor ? GEditor->GetEditorSubsystem<UUmgSetSubsystem>() : nullptr;
	if (!SetSubsystem)
	{
		OutError = TEXT("UmgSetSubsystem unavailable.");
		return false;
	}

	TSharedPtr<FJsonObject> Props = MakeShared<FJsonObject>();
	Props->SetField(PathInfo.PropertyPath, Value);

	FString PropsJson;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&PropsJson);
	FJsonSerializer::Serialize(Props.ToSharedRef(), Writer);

	if (!SetSubsystem->SetWidgetProperties(WidgetBlueprint, PathInfo.WidgetName, PropsJson))
	{
		OutError = FString::Printf(TEXT("set failed for widget '%s' property '%s'."), *PathInfo.WidgetName, *PathInfo.PropertyPath);
		return false;
	}

	return true;
}

bool UUmgPatchSubsystem::ApplyAddOp(UWidgetBlueprint* WidgetBlueprint, const FPatchPathInfo& PathInfo, const TSharedPtr<FJsonValue>& Value, FString& OutError) const
{
	const TSharedPtr<FJsonObject>* ValueObject = nullptr;
	if (!Value.IsValid() || !Value->TryGetObject(ValueObject) || !ValueObject || !(*ValueObject).IsValid())
	{
		OutError = TEXT("add op requires an object value with type/name.");
		return false;
	}

	FString WidgetType;
	FString NewName;
	(*ValueObject)->TryGetStringField(TEXT("type"), WidgetType);
	if (WidgetType.IsEmpty())
	{
		(*ValueObject)->TryGetStringField(TEXT("widget_type"), WidgetType);
	}
	if (WidgetType.IsEmpty())
	{
		(*ValueObject)->TryGetStringField(TEXT("widget_class"), WidgetType);
	}

	(*ValueObject)->TryGetStringField(TEXT("name"), NewName);
	if (NewName.IsEmpty())
	{
		(*ValueObject)->TryGetStringField(TEXT("widget_name"), NewName);
	}

	if (WidgetType.IsEmpty() || NewName.IsEmpty())
	{
		OutError = TEXT("add op value must include type and name.");
		return false;
	}

	UUmgSetSubsystem* SetSubsystem = GEditor ? GEditor->GetEditorSubsystem<UUmgSetSubsystem>() : nullptr;
	if (!SetSubsystem)
	{
		OutError = TEXT("UmgSetSubsystem unavailable.");
		return false;
	}

	const FString Created = SetSubsystem->CreateWidget(WidgetBlueprint, PathInfo.WidgetName, WidgetType, NewName);
	if (Created.IsEmpty())
	{
		OutError = FString::Printf(TEXT("add failed: could not create '%s' under '%s'."), *NewName, *PathInfo.WidgetName);
		return false;
	}

	return true;
}

bool UUmgPatchSubsystem::ApplyRemoveOp(UWidgetBlueprint* WidgetBlueprint, const FPatchPathInfo& PathInfo, FString& OutError) const
{
	UUmgSetSubsystem* SetSubsystem = GEditor ? GEditor->GetEditorSubsystem<UUmgSetSubsystem>() : nullptr;
	if (!SetSubsystem)
	{
		OutError = TEXT("UmgSetSubsystem unavailable.");
		return false;
	}

	UE_LOG(LogUmgPatch, Log, TEXT("Patch remove: eliminating widget '%s' from tree."), *PathInfo.WidgetName);

	if (!SetSubsystem->DeleteWidget(WidgetBlueprint, PathInfo.WidgetName))
	{
		OutError = FString::Printf(TEXT("remove failed: elimination of widget '%s' did not succeed."), *PathInfo.WidgetName);
		return false;
	}

	return true;
}

FString UUmgPatchSubsystem::ApplyPatch(
	UWidgetBlueprint* WidgetBlueprint,
	const TArray<TSharedPtr<FJsonValue>>& PatchOps,
	TOptional<int32> ExpectedRevision)
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

	if (!WidgetBlueprint)
	{
		Result->SetBoolField(TEXT("success"), false);
		Result->SetStringField(TEXT("error"), TEXT("No active WidgetBlueprint."));
	}
	else if (PatchOps.Num() == 0)
	{
		Result->SetBoolField(TEXT("success"), false);
		Result->SetStringField(TEXT("error"), TEXT("Patch array is empty."));
	}
	else
	{
		UUmgAttentionSubsystem* Attention = GEditor ? GEditor->GetEditorSubsystem<UUmgAttentionSubsystem>() : nullptr;
		bool bRevisionOk = true;
		if (ExpectedRevision.IsSet())
		{
			const int32 CurrentRevision = Attention ? Attention->GetPatchRevision() : 0;
			bRevisionOk = Attention && CurrentRevision == ExpectedRevision.GetValue();
			if (!bRevisionOk)
			{
				Result->SetBoolField(TEXT("success"), false);
				Result->SetStringField(TEXT("error"), TEXT("Patch revision mismatch."));
				Result->SetNumberField(TEXT("expected_revision"), ExpectedRevision.GetValue());
				Result->SetNumberField(TEXT("current_revision"), CurrentRevision);
			}
		}

		if (bRevisionOk)
		{
		FScopedTransaction Transaction(NSLOCTEXT("UmgMcp", "ApplyPatchTransaction", "Apply UMG Patch"));
		WidgetBlueprint->Modify();

		TArray<TSharedPtr<FJsonValue>> AppliedOps;
		bool bAllSucceeded = true;
		FString FirstError;

		for (int32 OpIndex = 0; OpIndex < PatchOps.Num(); ++OpIndex)
		{
			const TSharedPtr<FJsonObject>* OpObject = nullptr;
			if (!PatchOps[OpIndex].IsValid() || !PatchOps[OpIndex]->TryGetObject(OpObject) || !OpObject || !(*OpObject).IsValid())
			{
				bAllSucceeded = false;
				FirstError = FString::Printf(TEXT("Op %d is not a JSON object."), OpIndex);
				break;
			}

			FString OpType;
			FString Path;
			(*OpObject)->TryGetStringField(TEXT("op"), OpType);
			(*OpObject)->TryGetStringField(TEXT("path"), Path);

			FPatchPathInfo PathInfo;
			FString PathError;
			if (!ParsePatchPath(Path, OpType, PathInfo, PathError))
			{
				bAllSucceeded = false;
				FirstError = FString::Printf(TEXT("Op %d has invalid path: %s"), OpIndex, *PathError);
				break;
			}

			FString OpError;
			bool bOpOk = false;
			const TSharedPtr<FJsonValue> ValueField = (*OpObject)->TryGetField(TEXT("value"));

			if (OpType == TEXT("set"))
			{
				bOpOk = ApplySetOp(WidgetBlueprint, PathInfo, ValueField, OpError);
			}
			else if (OpType == TEXT("add"))
			{
				bOpOk = ApplyAddOp(WidgetBlueprint, PathInfo, ValueField, OpError);
			}
			else if (OpType == TEXT("remove"))
			{
				bOpOk = ApplyRemoveOp(WidgetBlueprint, PathInfo, OpError);
			}
			else
			{
				OpError = FString::Printf(TEXT("Unsupported op '%s'."), *OpType);
			}

			if (!bOpOk)
			{
				bAllSucceeded = false;
				FirstError = FString::Printf(TEXT("Op %d (%s): %s"), OpIndex, *OpType, *OpError);
				break;
			}

			AppliedOps.Add(PatchOps[OpIndex]);
		}

		if (bAllSucceeded)
		{
			if (Attention)
			{
				const int32 NewRevision = Attention->IncrementPatchRevision();
				Result->SetNumberField(TEXT("revision"), NewRevision);
			}

			Result->SetBoolField(TEXT("success"), true);
			Result->SetArrayField(TEXT("applied"), AppliedOps);
			Result->SetNumberField(TEXT("op_count"), AppliedOps.Num());
		}
		else
		{
			Transaction.Cancel();
			Result->SetBoolField(TEXT("success"), false);
			Result->SetStringField(TEXT("error"), FirstError);
			Result->SetStringField(TEXT("rollback"), TEXT("Transaction cancelled; no partial patch applied."));
		}
		}
	}

	FString Out;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
	FJsonSerializer::Serialize(Result.ToSharedRef(), Writer);
	return Out;
}
