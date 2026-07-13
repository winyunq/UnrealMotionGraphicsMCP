// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#include "Blueprint/Bluecode/BluecodeMergePlanner.h"
#include "Blueprint/Bluecode/BluecodeSyntax.h"
#include "Blueprint/UmgBlueprintFunctionSubsystem.h"
#include "EdGraph/EdGraph.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "GameFramework/Actor.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_VariableSet.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/AutomationTest.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBluecodeUnionRightAnchorTest,
	"UmgMcp.Bluecode.MergePlanner.RightAnchorAndAppend",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBluecodeUnionRightAnchorTest::RunTest(const FString& Parameters)
{
	const FString Issue39Statement = TEXT("node(\"Append Tool Result to Memory\", self=ActiveLiteRtLmAgent, ToolName=\"submit_werewolf_speech\", ResultJson=\"{\\\"accepted\\\":true}\", alias=SpeechToolAck)");
	TestEqual(
		TEXT("named node arguments are not assignments"),
		FBluecodeSyntax::FindTopLevelAssignmentOperator(Issue39Statement),
		INDEX_NONE);
	TestTrue(TEXT("plain assignment is recognized"), FBluecodeSyntax::FindTopLevelAssignmentOperator(TEXT("Result = PureCall(Pin=value)")) > 0);

	const TArray<FBluecodeMergeStatement> Existing = {
		{TEXT("PrintString(\"one\")"), TEXT("printstring(\"one\")"), TEXT("NodeOne")},
		{TEXT("PrintString(\"three\")"), TEXT("printstring(\"three\")"), TEXT("NodeThree")}
	};

	const FBluecodeMergePlan AnchoredPlan = FBluecodeMergePlanner::PlanUnion(
		Existing,
		{
			{TEXT("PrintString(\"two\")"), TEXT("printstring(\"two\")"), TEXT("")},
			{TEXT("PrintString(\"three\")"), TEXT("printstring(\"three\")"), TEXT("")}
		});
	TestEqual(TEXT("anchored operation count"), AnchoredPlan.Operations.Num(), 2);
	TestEqual(
		TEXT("two inserts before the matched right anchor"),
		AnchoredPlan.Operations[0].Kind,
		EBluecodeMergeOperationKind::InsertBefore);
	TestEqual(TEXT("right anchor identity"), AnchoredPlan.Operations[0].RightAnchorNodeId, FString(TEXT("NodeThree")));
	TestEqual(TEXT("three is reused"), AnchoredPlan.Operations[1].Kind, EBluecodeMergeOperationKind::Noop);

	const FBluecodeMergePlan AppendPlan = FBluecodeMergePlanner::PlanUnion(
		Existing,
		{{TEXT("PrintString(\"two\")"), TEXT("printstring(\"two\")"), TEXT("")}});
	TestEqual(TEXT("anchorless patch appends"), AppendPlan.Operations[0].Kind, EBluecodeMergeOperationKind::Append);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBluecodeSemanticCompressionTest,
	"UmgMcp.Bluecode.Protocol.SemanticCompressionAndIssue39",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBluecodeSemanticCompressionTest::RunTest(const FString& Parameters)
{
	const FName BlueprintName = MakeUniqueObjectName(GetTransientPackage(), UBlueprint::StaticClass(), TEXT("BluecodeProtocolTest"));
	UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
		AActor::StaticClass(),
		GetTransientPackage(),
		BlueprintName,
		BPTYPE_Normal,
		TEXT("UmgMcp.Bluecode.ProtocolTest"));
	TestNotNull(TEXT("transient Blueprint"), Blueprint);
	UEdGraph* Graph = Blueprint ? FBlueprintEditorUtils::FindEventGraph(Blueprint) : nullptr;
	TestNotNull(TEXT("event graph"), Graph);
	UUmgBlueprintFunctionSubsystem* Subsystem = GEditor ? GEditor->GetEditorSubsystem<UUmgBlueprintFunctionSubsystem>() : nullptr;
	TestNotNull(TEXT("Blueprint subsystem"), Subsystem);
	if (!Blueprint || !Graph || !Subsystem)
	{
		return false;
	}

	auto Execute = [&](const FString& PayloadText)
	{
		const FString ResultText = Subsystem->HandleBlueprintGraphAction(Blueprint, TEXT("manage_blueprint_graph"), PayloadText);
		TSharedPtr<FJsonObject> Result;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResultText);
		FJsonSerializer::Deserialize(Reader, Result);
		return Result;
	};
	auto ExecuteObject = [&](const TSharedPtr<FJsonObject>& Payload)
	{
		FString PayloadText;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&PayloadText);
		FJsonSerializer::Serialize(Payload.ToSharedRef(), Writer);
		return Execute(PayloadText);
	};

	TSharedPtr<FJsonObject> SemanticBefore = Execute(TEXT("{\"subAction\":\"bluecode_read_function\",\"detail\":\"semantic\"}"));
	TestTrue(TEXT("semantic read succeeds"), SemanticBefore.IsValid() && SemanticBefore->GetBoolField(TEXT("success")));
	if (!SemanticBefore.IsValid())
	{
		return false;
	}
	TestFalse(TEXT("semantic read omits action hints"), SemanticBefore.IsValid() && SemanticBefore->HasField(TEXT("action_hints")));
	TestFalse(TEXT("semantic read omits source map"), SemanticBefore.IsValid() && SemanticBefore->HasField(TEXT("source_map")));

	FString RevisionBefore;
	FString CodeBefore;
	SemanticBefore->TryGetStringField(TEXT("base_revision"), RevisionBefore);
	SemanticBefore->TryGetStringField(TEXT("code"), CodeBefore);
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (Node)
		{
			Node->NodePosX += 917;
			Node->NodePosY -= 431;
		}
	}
	TSharedPtr<FJsonObject> SemanticAfter = Execute(TEXT("{\"subAction\":\"bluecode_read_function\",\"detail\":\"semantic\"}"));
	if (!SemanticAfter.IsValid())
	{
		AddError(TEXT("semantic read after layout change returned invalid JSON"));
		return false;
	}
	FString RevisionAfter;
	FString CodeAfter;
	SemanticAfter->TryGetStringField(TEXT("base_revision"), RevisionAfter);
	SemanticAfter->TryGetStringField(TEXT("code"), CodeAfter);
	TestEqual(TEXT("layout does not change semantic revision"), RevisionAfter, RevisionBefore);
	TestEqual(TEXT("layout does not change semantic code"), CodeAfter, CodeBefore);

	TSharedPtr<FJsonObject> Roundtrip = Execute(TEXT("{\"subAction\":\"bluecode_read_function\",\"detail\":\"roundtrip\"}"));
	TestTrue(TEXT("roundtrip includes source map"), Roundtrip.IsValid() && Roundtrip->HasField(TEXT("source_map")));

	TSharedPtr<FJsonObject> InvalidHandleResult = Execute(TEXT("{\"subAction\":\"add_function_step\",\"nodeName\":\"AppendToolResultToMemory\",\"memberClass\":\"/Script/LiteRTLMUnreal.LiteRtLmAgent\",\"action_handle\":\"bpact:invalid-test-handle\"}"));
	FString InvalidHandleErrorCode;
	bool bInvalidHandleRolledBack = false;
	if (InvalidHandleResult.IsValid())
	{
		InvalidHandleResult->TryGetStringField(TEXT("error_code"), InvalidHandleErrorCode);
		InvalidHandleResult->TryGetBoolField(TEXT("rolled_back"), bInvalidHandleRolledBack);
	}
	TestEqual(TEXT("invalid action handle returns structured failure"), InvalidHandleErrorCode, FString(TEXT("node_creation_failed")));
	TestTrue(TEXT("invalid action handle path rolls back"), bInvalidHandleRolledBack);
	TSharedPtr<FJsonObject> AfterInvalidHandle = Execute(TEXT("{\"subAction\":\"bluecode_read_function\",\"detail\":\"semantic\"}"));
	if (!AfterInvalidHandle.IsValid())
	{
		AddError(TEXT("semantic read after invalid handle returned invalid JSON"));
		return false;
	}
	FString RevisionAfterInvalidHandle;
	AfterInvalidHandle->TryGetStringField(TEXT("base_revision"), RevisionAfterInvalidHandle);
	TestEqual(TEXT("invalid action handle leaves graph unchanged"), RevisionAfterInvalidHandle, RevisionBefore);

	TSharedPtr<FJsonObject> ApplyResult = Execute(TEXT("{\"subAction\":\"bluecode_apply\",\"mode\":\"append\",\"code\":\"if true:\"}"));
	TestTrue(TEXT("deterministic branch syntax applies"), ApplyResult.IsValid() && ApplyResult->GetBoolField(TEXT("success")));
	bool bFoundBranch = false;
	bool bFoundVariableSet = false;
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		bFoundBranch |= Cast<UK2Node_IfThenElse>(Node) != nullptr;
		bFoundVariableSet |= Cast<UK2Node_VariableSet>(Node) != nullptr;
	}
	TestTrue(TEXT("Branch node was created"), bFoundBranch);
	TestFalse(TEXT("Branch statement was not misparsed as VariableSet"), bFoundVariableSet);

	TSharedPtr<FJsonObject> BranchRoundtrip = Execute(TEXT("{\"subAction\":\"bluecode_read_function\",\"detail\":\"roundtrip\"}"));
	if (!BranchRoundtrip.IsValid())
	{
		AddError(TEXT("branch roundtrip returned invalid JSON"));
		return false;
	}
	FString BranchRevision;
	const TSharedPtr<FJsonObject>* BranchSourceMap = nullptr;
	BranchRoundtrip->TryGetStringField(TEXT("base_revision"), BranchRevision);
	BranchRoundtrip->TryGetObjectField(TEXT("source_map"), BranchSourceMap);
	TestTrue(TEXT("branch roundtrip has source identity"), !BranchRevision.IsEmpty() && BranchSourceMap && BranchSourceMap->IsValid());
	if (!BranchSourceMap || !BranchSourceMap->IsValid())
	{
		return false;
	}

	TSharedPtr<FJsonObject> DryRunPayload = MakeShared<FJsonObject>();
	DryRunPayload->SetStringField(TEXT("subAction"), TEXT("bluecode_apply"));
	DryRunPayload->SetStringField(TEXT("mode"), TEXT("union"));
	DryRunPayload->SetStringField(TEXT("code"), TEXT("if false:"));
	DryRunPayload->SetStringField(TEXT("base_revision"), BranchRevision);
	DryRunPayload->SetObjectField(TEXT("source_map"), *BranchSourceMap);
	DryRunPayload->SetBoolField(TEXT("dry_run"), true);
	TSharedPtr<FJsonObject> DryRunResult = ExecuteObject(DryRunPayload);
	if (!DryRunResult.IsValid())
	{
		AddError(TEXT("dry-run returned invalid JSON"));
		return false;
	}
	const TArray<TSharedPtr<FJsonValue>>* PlannedOperations = nullptr;
	DryRunResult->TryGetArrayField(TEXT("operations"), PlannedOperations);
	FString PlannedKind;
	if (PlannedOperations && PlannedOperations->Num() == 1 && (*PlannedOperations)[0]->Type == EJson::Object)
	{
		(*PlannedOperations)[0]->AsObject()->TryGetStringField(TEXT("op"), PlannedKind);
	}
	TestEqual(TEXT("dry-run recognizes a source-mapped update"), PlannedKind, FString(TEXT("update")));

	TSharedPtr<FJsonObject> ApplyUpdatePayload = MakeShared<FJsonObject>(*DryRunPayload);
	ApplyUpdatePayload->RemoveField(TEXT("dry_run"));
	TSharedPtr<FJsonObject> UpdateResult = ExecuteObject(ApplyUpdatePayload);
	TestTrue(TEXT("source-mapped branch update succeeds"), UpdateResult.IsValid() && UpdateResult->GetBoolField(TEXT("success")));
	int32 BranchCount = 0;
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		BranchCount += Cast<UK2Node_IfThenElse>(Node) ? 1 : 0;
	}
	TestEqual(TEXT("source-mapped update does not duplicate the branch"), BranchCount, 1);

	TSharedPtr<FJsonObject> StaleResult = ExecuteObject(ApplyUpdatePayload);
	FString StaleErrorCode;
	if (StaleResult.IsValid())
	{
		StaleResult->TryGetStringField(TEXT("error_code"), StaleErrorCode);
	}
	TestEqual(TEXT("stale roundtrip is rejected"), StaleErrorCode, FString(TEXT("graph_revision_conflict")));

	TSharedPtr<FJsonObject> BeforeRollback = Execute(TEXT("{\"subAction\":\"bluecode_read_function\",\"detail\":\"semantic\"}"));
	FString BeforeRollbackRevision;
	BeforeRollback->TryGetStringField(TEXT("base_revision"), BeforeRollbackRevision);
	TSharedPtr<FJsonObject> FailingPatch = Execute(TEXT("{\"subAction\":\"bluecode_apply\",\"mode\":\"append\",\"code\":\"if true:\\nthis is not bluecode\"}"));
	bool bRolledBack = false;
	bool bRollbackVerified = false;
	if (FailingPatch.IsValid())
	{
		FailingPatch->TryGetBoolField(TEXT("rolled_back"), bRolledBack);
		FailingPatch->TryGetBoolField(TEXT("rollback_verified"), bRollbackVerified);
	}
	TestTrue(TEXT("mixed failing patch is rolled back"), bRolledBack);
	TestTrue(TEXT("rollback revision is verified"), bRollbackVerified);
	TSharedPtr<FJsonObject> AfterRollback = Execute(TEXT("{\"subAction\":\"bluecode_read_function\",\"detail\":\"semantic\"}"));
	FString AfterRollbackRevision;
	AfterRollback->TryGetStringField(TEXT("base_revision"), AfterRollbackRevision);
	TestEqual(TEXT("failed patch restores the original revision"), AfterRollbackRevision, BeforeRollbackRevision);
	return true;
}

#endif
