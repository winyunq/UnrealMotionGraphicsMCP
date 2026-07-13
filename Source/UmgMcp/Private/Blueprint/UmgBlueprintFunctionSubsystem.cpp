// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#include "Blueprint/UmgBlueprintFunctionSubsystem.h"
#include "Blueprint/Bluecode/BluecodeMergePlanner.h"
#include "Blueprint/Bluecode/BluecodeSyntax.h"
#include "WidgetBlueprint.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Logging/LogMacros.h"
#include "FileManage/UmgAttentionSubsystem.h"
#include "Bridge/UmgMcpJsonCompat.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"

// Editor includes
#if WITH_EDITOR
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraph/EdGraphSchema.h"
#include "EdGraphUtilities.h"
#include "Blueprint/WidgetTree.h"
#include "Components/PanelWidget.h"
#include "Components/Widget.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "BlueprintActionMenuBuilder.h"
#include "BlueprintActionMenuItem.h"
#include "BlueprintActionMenuUtils.h"
#include "BlueprintNodeSpawner.h"
#include "Misc/SecureHash.h"

// Nodes
#include "K2Node_CallFunction.h"
#include "K2Node_Event.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "K2Node_DynamicCast.h"
#include "K2Node_ComponentBoundEvent.h"
#include "K2Node_FunctionEntry.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_Event.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "K2Node_DynamicCast.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_AddPinInterface.h"
#include "K2Node_ExecutionSequence.h"
#include "K2Node_FormatText.h"
#include "K2Node_MakeArray.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogUmgBlueprint, Log, All);

#if WITH_EDITOR
namespace
{
	UBlueprint* ResolveBlueprintFromGraph(UEdGraph* Graph)
	{
		return Graph ? FBlueprintEditorUtils::FindBlueprintForGraph(Graph) : nullptr;
	}

	bool NodeHasExecPin(const UEdGraphNode* Node)
	{
		if (!Node) return false;
		for (const UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
			{
				return true;
			}
		}
		return false;
	}

	TSharedPtr<FJsonObject> MakePinCounts(const UEdGraphNode* Node)
	{
		TSharedPtr<FJsonObject> Counts = MakeShared<FJsonObject>();
		int32 InputPins = 0;
		int32 OutputPins = 0;
		int32 ExecInputs = 0;
		int32 ExecOutputs = 0;
		int32 DataInputs = 0;
		int32 DataOutputs = 0;

		if (Node)
		{
			for (const UEdGraphPin* Pin : Node->Pins)
			{
				if (!Pin || Pin->bHidden)
				{
					continue;
				}

				const bool bExec = Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec;
				if (Pin->Direction == EGPD_Input)
				{
					++InputPins;
					bExec ? ++ExecInputs : ++DataInputs;
				}
				else
				{
					++OutputPins;
					bExec ? ++ExecOutputs : ++DataOutputs;
				}
			}
		}

		Counts->SetNumberField(TEXT("inputs"), InputPins);
		Counts->SetNumberField(TEXT("outputs"), OutputPins);
		Counts->SetNumberField(TEXT("exec_inputs"), ExecInputs);
		Counts->SetNumberField(TEXT("exec_outputs"), ExecOutputs);
		Counts->SetNumberField(TEXT("data_inputs"), DataInputs);
		Counts->SetNumberField(TEXT("data_outputs"), DataOutputs);
		return Counts;
	}

	TSharedPtr<FJsonObject> TemplatePinToJson(const UEdGraphPin* Pin)
	{
		TSharedPtr<FJsonObject> PinObj = MakeShared<FJsonObject>();
		if (!Pin)
		{
			return PinObj;
		}

		PinObj->SetStringField(TEXT("name"), Pin->PinName.ToString());
		const FString DisplayName = Pin->GetDisplayName().ToString();
		if (!DisplayName.IsEmpty() && !DisplayName.Equals(Pin->PinName.ToString(), ESearchCase::CaseSensitive))
		{
			PinObj->SetStringField(TEXT("display_name"), DisplayName);
		}
		if (!Pin->PinFriendlyName.IsEmpty())
		{
			PinObj->SetStringField(TEXT("friendly_name"), Pin->PinFriendlyName.ToString());
		}
		PinObj->SetStringField(TEXT("direction"), Pin->Direction == EGPD_Input ? TEXT("input") : TEXT("output"));
		PinObj->SetStringField(TEXT("category"), Pin->PinType.PinCategory.ToString());
		PinObj->SetBoolField(TEXT("is_exec"), Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec);
		PinObj->SetBoolField(TEXT("hidden"), Pin->bHidden);
		if (!Pin->PinType.PinSubCategory.ToString().IsEmpty())
		{
			PinObj->SetStringField(TEXT("sub_category"), Pin->PinType.PinSubCategory.ToString());
		}
		if (!Pin->DefaultValue.IsEmpty())
		{
			PinObj->SetStringField(TEXT("default"), Pin->DefaultValue);
		}
		if (!Pin->DefaultTextValue.IsEmpty())
		{
			PinObj->SetStringField(TEXT("default_text"), Pin->DefaultTextValue.ToString());
		}
		if (Pin->DefaultObject)
		{
			PinObj->SetStringField(TEXT("default_object"), Pin->DefaultObject->GetPathName());
		}
		if (Pin->PinType.PinSubCategoryObject.IsValid())
		{
			PinObj->SetStringField(TEXT("subType"), Pin->PinType.PinSubCategoryObject->GetName());
			PinObj->SetStringField(TEXT("subTypePath"), Pin->PinType.PinSubCategoryObject->GetPathName());
			PinObj->SetStringField(TEXT("sub_type"), Pin->PinType.PinSubCategoryObject->GetName());
			PinObj->SetStringField(TEXT("sub_type_path"), Pin->PinType.PinSubCategoryObject->GetPathName());
		}
		return PinObj;
	}

	TSharedPtr<FJsonObject> TemplateNodeToJson(const UEdGraphNode* Node)
	{
		TSharedPtr<FJsonObject> NodeObj = MakeShared<FJsonObject>();
		if (!Node)
		{
			return NodeObj;
		}

		NodeObj->SetStringField(TEXT("name"), Node->GetName());
		NodeObj->SetStringField(TEXT("title"), Node->GetNodeTitle(ENodeTitleType::ListView).ToString());
		NodeObj->SetStringField(TEXT("class"), Node->GetClass()->GetName());
		NodeObj->SetBoolField(TEXT("isExec"), NodeHasExecPin(Node));
		NodeObj->SetBoolField(TEXT("is_exec"), NodeHasExecPin(Node));
		NodeObj->SetObjectField(TEXT("pin_counts"), MakePinCounts(Node));

		TArray<TSharedPtr<FJsonValue>> Inputs;
		TArray<TSharedPtr<FJsonValue>> Outputs;
		for (const UEdGraphPin* Pin : Node->Pins)
		{
			if (!Pin || Pin->bHidden)
			{
				continue;
			}
			if (Pin->Direction == EGPD_Input)
			{
				Inputs.Add(MakeShared<FJsonValueObject>(TemplatePinToJson(Pin)));
			}
			else
			{
				Outputs.Add(MakeShared<FJsonValueObject>(TemplatePinToJson(Pin)));
			}
		}
		NodeObj->SetArrayField(TEXT("inputs"), Inputs);
		NodeObj->SetArrayField(TEXT("outputs"), Outputs);
		return NodeObj;
	}

	FString MakeActionIdentity(const TSharedPtr<FEdGraphSchemaAction>& Action)
	{
		if (!Action.IsValid())
		{
			return TEXT("");
		}

		if (Action->GetTypeId() == FBlueprintActionMenuItem::StaticGetTypeId())
		{
			const TSharedPtr<FBlueprintActionMenuItem> MenuItem = StaticCastSharedPtr<FBlueprintActionMenuItem>(Action);
			if (MenuItem.IsValid())
			{
				const UBlueprintNodeSpawner* Spawner = MenuItem->GetRawAction();
				if (Spawner)
				{
					return FString::Printf(
						TEXT("%s|%s|%s|%s"),
						Spawner->NodeClass ? *Spawner->NodeClass->GetPathName() : TEXT("NoNodeClass"),
						*Spawner->GetSpawnerSignature().ToString(),
						*Action->GetMenuDescription().ToString(),
						*Action->GetCategory().ToString());
				}
			}
		}

		return FString::Printf(
			TEXT("%s|%s|%s"),
			*Action->GetCategory().ToString(),
			*Action->GetMenuDescription().ToString(),
			*Action->GetFullSearchText());
	}

	FString MakeActionHandle(const TSharedPtr<FEdGraphSchemaAction>& Action)
	{
		const FString Identity = MakeActionIdentity(Action);
		FTCHARToUTF8 Utf8Identity(*Identity);
		FMD5 Md5;
		Md5.Update(reinterpret_cast<const uint8*>(Utf8Identity.Get()), Utf8Identity.Length());

		uint8 Digest[16];
		Md5.Final(Digest);
		return TEXT("bpact:") + BytesToHex(Digest, UE_ARRAY_COUNT(Digest)).ToLower();
	}

	bool MatchesTextFilter(const FString& Haystack, const FString& Needle, const bool bExact)
	{
		if (Needle.IsEmpty())
		{
			return true;
		}
		return bExact
			? Haystack.Equals(Needle, ESearchCase::IgnoreCase)
			: Haystack.Contains(Needle, ESearchCase::IgnoreCase);
	}

	FString NormalizeBluecodeStatement(const FString& Statement)
	{
		FString Normalized = Statement;
		Normalized.ReplaceInline(TEXT(" "), TEXT(""));
		Normalized.ReplaceInline(TEXT("\t"), TEXT(""));
		Normalized.ReplaceInline(TEXT("\r"), TEXT(""));
		Normalized.ReplaceInline(TEXT("\n"), TEXT(""));
		return Normalized.ToLower();
	}

	FString NormalizeBluecodePinName(const FString& PinName)
	{
		FString Normalized = NormalizeBluecodeStatement(PinName);
		Normalized.ReplaceInline(TEXT("["), TEXT(""));
		Normalized.ReplaceInline(TEXT("]"), TEXT(""));
		return Normalized;
	}

	FString HashBluecodeIdentity(const FString& Identity, const FString& Prefix)
	{
		FTCHARToUTF8 Utf8Identity(*Identity);
		FMD5 Md5;
		Md5.Update(reinterpret_cast<const uint8*>(Utf8Identity.Get()), Utf8Identity.Length());
		uint8 Digest[16];
		Md5.Final(Digest);
		return Prefix + BytesToHex(Digest, UE_ARRAY_COUNT(Digest)).ToLower();
	}

	FString MakeBluecodeSemanticKey(const UEdGraphNode* Node)
	{
		if (!Node)
		{
			return TEXT("invalid");
		}
		if (const UK2Node_CallFunction* CallNode = Cast<UK2Node_CallFunction>(Node))
		{
			const UFunction* Function = CallNode->GetTargetFunction();
			const FString Owner = Function && Function->GetOwnerClass()
				? Function->GetOwnerClass()->GetPathName()
				: TEXT("self");
			return FString::Printf(
				TEXT("call:%s::%s"),
				*Owner,
				*CallNode->FunctionReference.GetMemberName().ToString());
		}
		if (const UK2Node_VariableSet* SetNode = Cast<UK2Node_VariableSet>(Node))
		{
			return TEXT("set:") + SetNode->VariableReference.GetMemberName().ToString();
		}
		if (const UK2Node_VariableGet* GetNode = Cast<UK2Node_VariableGet>(Node))
		{
			return TEXT("get:") + GetNode->VariableReference.GetMemberName().ToString();
		}
		if (Cast<UK2Node_IfThenElse>(Node))
		{
			return TEXT("control:branch");
		}
		return FString::Printf(
			TEXT("node:%s:%s"),
			Node->GetClass() ? *Node->GetClass()->GetPathName() : TEXT("unknown"),
			*NormalizeBluecodeStatement(Node->GetNodeTitle(ENodeTitleType::ListView).ToString()));
	}

	bool IsBluecodeSemanticKeyCompatible(
		const FString& SemanticKey,
		const FString& CallName,
		const FString& ResolvedNodeName)
	{
		if (SemanticKey.IsEmpty())
		{
			return false;
		}
		const FString NormalizedResolved = NormalizeBluecodeStatement(ResolvedNodeName);
		const FString NormalizedCall = NormalizeBluecodeStatement(CallName);
		if (SemanticKey.StartsWith(TEXT("call:")))
		{
			FString Left;
			FString MemberName;
			return SemanticKey.Split(TEXT("::"), &Left, &MemberName, ESearchCase::CaseSensitive, ESearchDir::FromEnd) &&
				NormalizeBluecodeStatement(MemberName) == NormalizedResolved;
		}
		if (SemanticKey.StartsWith(TEXT("set:")))
		{
			return NormalizeBluecodeStatement(SemanticKey.Mid(4)) == NormalizedResolved;
		}
		if (SemanticKey == TEXT("control:branch"))
		{
			return NormalizedCall == TEXT("if") || NormalizedResolved == TEXT("branch");
		}
		if (SemanticKey.StartsWith(TEXT("node:")))
		{
			FString Left;
			FString Title;
			return SemanticKey.Split(TEXT(":"), &Left, &Title, ESearchCase::CaseSensitive, ESearchDir::FromEnd) &&
				NormalizeBluecodeStatement(Title) == NormalizedResolved;
		}
		return false;
	}

	FString MakeBluecodeGraphRevision(const UEdGraph* Graph)
	{
		if (!Graph)
		{
			return TEXT("bcrev:invalid");
		}

		TArray<const UEdGraphNode*> Nodes;
		for (const UEdGraphNode* Node : Graph->Nodes)
		{
			if (Node)
			{
				Nodes.Add(Node);
			}
		}
		Nodes.Sort([](const UEdGraphNode& A, const UEdGraphNode& B)
		{
			return A.NodeGuid.ToString() < B.NodeGuid.ToString();
		});

		TArray<FString> Parts;
		Parts.Reserve(Nodes.Num() * 4);
		Parts.Add(TEXT("graph:") + Graph->GetName());
		for (const UEdGraphNode* Node : Nodes)
		{
			Parts.Add(FString::Printf(
				TEXT("node:%s|%s|%s"),
				*Node->NodeGuid.ToString(),
				Node->GetClass() ? *Node->GetClass()->GetPathName() : TEXT("unknown"),
				*MakeBluecodeSemanticKey(Node)));

			// Preserve arbitrary UK2Node/plugin UObject properties in the semantic
			// revision while deliberately removing editor-only layout coordinates.
			TSet<UObject*> NodeToExport;
			NodeToExport.Add(const_cast<UEdGraphNode*>(Node));
			FString ExportedNodeText;
			FEdGraphUtilities::ExportNodesToText(NodeToExport, ExportedNodeText);
			TArray<FString> ExportedLines;
			ExportedNodeText.ParseIntoArrayLines(ExportedLines, false);
			TArray<FString> SemanticPropertyLines;
			for (FString Line : ExportedLines)
			{
				Line.TrimStartAndEndInline();
				if (Line.StartsWith(TEXT("Begin Object")) ||
					Line == TEXT("End Object") ||
					Line.StartsWith(TEXT("CustomProperties Pin")) ||
					Line.StartsWith(TEXT("NodePosX=")) ||
					Line.StartsWith(TEXT("NodePosY=")) ||
					Line.StartsWith(TEXT("NodeWidth=")) ||
					Line.StartsWith(TEXT("NodeHeight=")))
				{
					continue;
				}
				SemanticPropertyLines.Add(Line);
			}
			Parts.Add(TEXT("properties:") + FString::Join(SemanticPropertyLines, TEXT("\n")));

			TArray<const UEdGraphPin*> Pins;
			for (const UEdGraphPin* Pin : Node->Pins)
			{
				if (Pin)
				{
					Pins.Add(Pin);
				}
			}
			Pins.Sort([](const UEdGraphPin& A, const UEdGraphPin& B)
			{
				const FString AKey = FString::Printf(TEXT("%d:%s"), static_cast<int32>(A.Direction), *A.PinName.ToString());
				const FString BKey = FString::Printf(TEXT("%d:%s"), static_cast<int32>(B.Direction), *B.PinName.ToString());
				return AKey < BKey;
			});

			for (const UEdGraphPin* Pin : Pins)
			{
				TArray<FString> Links;
				for (const UEdGraphPin* LinkedPin : Pin->LinkedTo)
				{
					const UEdGraphNode* LinkedNode = LinkedPin ? LinkedPin->GetOwningNode() : nullptr;
					if (LinkedNode && LinkedPin)
					{
						Links.Add(LinkedNode->NodeGuid.ToString() + TEXT(":") + LinkedPin->PinName.ToString());
					}
				}
				Links.Sort();
				const FString DefaultObjectPath = Pin->DefaultObject ? Pin->DefaultObject->GetPathName() : TEXT("");
				Parts.Add(FString::Printf(
					TEXT("pin:%s|%d|%s|%s|%s|%s|%s|%s"),
					*Pin->PinName.ToString(),
					static_cast<int32>(Pin->Direction),
					*Pin->PinType.PinCategory.ToString(),
					*Pin->PinType.PinSubCategory.ToString(),
					*Pin->DefaultValue,
					*Pin->DefaultTextValue.ToString(),
					*DefaultObjectPath,
					*FString::Join(Links, TEXT(","))));
			}
		}

		// Deliberately excludes NodePosX/Y: layout-only edits must not invalidate a
		// semantic read/plan/apply cycle.
		return HashBluecodeIdentity(FString::Join(Parts, TEXT("\n")), TEXT("bcrev:"));
	}

	FString StripBluecodeQuotes(FString Value);

	bool IsBluecodeDataPin(const UEdGraphPin* Pin)
	{
		return Pin &&
			Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec &&
			!Pin->bHidden;
	}

	bool IsBluecodeSelfPinAlias(const FString& PinName)
	{
		const FString Normalized = NormalizeBluecodePinName(PinName);
		return Normalized == TEXT("self") ||
			Normalized == TEXT("target") ||
			Normalized == TEXT("this");
	}

	bool IsBluecodeSelfPin(const UEdGraphPin* Pin)
	{
		return Pin &&
			(Pin->PinName == UEdGraphSchema_K2::PN_Self ||
			 IsBluecodeSelfPinAlias(Pin->PinName.ToString()) ||
			 IsBluecodeSelfPinAlias(Pin->GetDisplayName().ToString()));
	}

	bool IsBluecodeReadableInputPin(const UEdGraphPin* Pin)
	{
		if (!Pin ||
			Pin->Direction != EGPD_Input ||
			Pin->bHidden ||
			Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
		{
			return false;
		}

		if (IsBluecodeSelfPin(Pin))
		{
			return Pin->LinkedTo.Num() > 0 ||
				Pin->DefaultObject ||
				!Pin->DefaultValue.IsEmpty() ||
				!Pin->DefaultTextValue.IsEmpty();
		}

		return true;
	}

	bool BluecodePinNameMatches(const UEdGraphPin* Pin, const FString& Query)
	{
		if (!Pin)
		{
			return false;
		}

		const FString TrimmedQuery = Query.TrimStartAndEnd();
		if (TrimmedQuery.IsEmpty())
		{
			return false;
		}

		const FString NormalizedQuery = NormalizeBluecodePinName(TrimmedQuery);
		const FString PinName = Pin->PinName.ToString();
		if (PinName.Equals(TrimmedQuery, ESearchCase::IgnoreCase) ||
			NormalizeBluecodePinName(PinName) == NormalizedQuery)
		{
			return true;
		}

		const FString FriendlyName = Pin->PinFriendlyName.ToString();
		if (!FriendlyName.IsEmpty() &&
			(FriendlyName.Equals(TrimmedQuery, ESearchCase::IgnoreCase) ||
			 NormalizeBluecodePinName(FriendlyName) == NormalizedQuery))
		{
			return true;
		}

		const FString DisplayName = Pin->GetDisplayName().ToString();
		return !DisplayName.IsEmpty() &&
			(DisplayName.Equals(TrimmedQuery, ESearchCase::IgnoreCase) ||
			 NormalizeBluecodePinName(DisplayName) == NormalizedQuery);
	}

	bool IsBluecodeRoleAlias(const FString& PinName, const bool bSource)
	{
		const FString Normalized = NormalizeBluecodePinName(PinName);
		if (bSource)
		{
			return Normalized.IsEmpty() ||
				Normalized == TEXT("returnvalue") ||
				Normalized == TEXT("result") ||
				Normalized == TEXT("output") ||
				Normalized == TEXT("value");
		}
		return Normalized.IsEmpty() ||
			Normalized == TEXT("inpin") ||
			Normalized == TEXT("input") ||
			Normalized == TEXT("value");
	}

	FString BluecodeJsonValueToString(const TSharedPtr<FJsonValue>& Value)
	{
		if (!Value.IsValid())
		{
			return TEXT("");
		}
		if (Value->Type == EJson::String)
		{
			return Value->AsString();
		}
		if (Value->Type == EJson::Number)
		{
			return FString::SanitizeFloat(Value->AsNumber());
		}
		if (Value->Type == EJson::Boolean)
		{
			return Value->AsBool() ? TEXT("true") : TEXT("false");
		}
		if (Value->Type == EJson::Object)
		{
			TSharedPtr<FJsonObject> Obj = Value->AsObject();
			if (Obj.IsValid())
			{
				const TCHAR* Fields[] = {TEXT("value"), TEXT("path"), TEXT("object"), TEXT("class"), TEXT("text"), TEXT("literal")};
				for (const TCHAR* Field : Fields)
				{
					FString TextValue;
					if (Obj->TryGetStringField(Field, TextValue))
					{
						return TextValue;
					}
				}
			}
		}
		return TEXT("");
	}

	FString BluecodeJsonValueToImportText(const TSharedPtr<FJsonValue>& Value)
	{
		const FString SimpleValue = BluecodeJsonValueToString(Value);
		if (!SimpleValue.IsEmpty())
		{
			return SimpleValue;
		}
		if (Value.IsValid() && Value->Type == EJson::Object)
		{
			FString JsonText;
			TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonText);
			FJsonSerializer::Serialize(Value->AsObject().ToSharedRef(), Writer);
			return JsonText;
		}
		return TEXT("");
	}

	FProperty* FindBluecodeObjectProperty(UClass* Class, const FString& Query, TArray<FString>* OutCandidates = nullptr)
	{
		if (!Class || Query.IsEmpty())
		{
			return nullptr;
		}

		if (FProperty* ExactProperty = FindFProperty<FProperty>(Class, FName(*Query)))
		{
			return ExactProperty;
		}

		const FString NormalizedQuery = NormalizeBluecodePinName(Query);
		for (TFieldIterator<FProperty> It(Class); It; ++It)
		{
			FProperty* Property = *It;
			if (!Property)
			{
				continue;
			}

			const FString PropertyName = Property->GetName();
			const FString DisplayName = Property->GetMetaData(TEXT("DisplayName"));
			const bool bMatches =
				PropertyName.Equals(Query, ESearchCase::IgnoreCase) ||
				NormalizeBluecodePinName(PropertyName) == NormalizedQuery ||
				(!DisplayName.IsEmpty() &&
					(DisplayName.Equals(Query, ESearchCase::IgnoreCase) ||
					 NormalizeBluecodePinName(DisplayName) == NormalizedQuery));
			if (bMatches)
			{
				return Property;
			}

			if (OutCandidates &&
				(PropertyName.Contains(Query, ESearchCase::IgnoreCase) ||
				 (!DisplayName.IsEmpty() && DisplayName.Contains(Query, ESearchCase::IgnoreCase))))
			{
				OutCandidates->Add(PropertyName);
			}
		}
		return nullptr;
	}

	bool TrySetBluecodeObjectProperty(UObject* Object, FProperty* Property, const TSharedPtr<FJsonValue>& JsonValue, FString& OutError)
	{
		if (!Object || !Property)
		{
			OutError = TEXT("Missing object or property.");
			return false;
		}

		void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Object);
		if (!ValuePtr)
		{
			OutError = TEXT("Property value pointer is unavailable.");
			return false;
		}

		const FString TextValue = BluecodeJsonValueToImportText(JsonValue);

		if (FBoolProperty* BoolProperty = CastField<FBoolProperty>(Property))
		{
			const bool bValue = JsonValue.IsValid() && JsonValue->Type == EJson::Boolean
				? JsonValue->AsBool()
				: TextValue.Equals(TEXT("true"), ESearchCase::IgnoreCase) || TextValue == TEXT("1");
			BoolProperty->SetPropertyValue(ValuePtr, bValue);
			return true;
		}

		if (FNumericProperty* NumericProperty = CastField<FNumericProperty>(Property))
		{
			if (NumericProperty->IsInteger())
			{
				const int64 IntValue = JsonValue.IsValid() && JsonValue->Type == EJson::Number
					? static_cast<int64>(JsonValue->AsNumber())
					: FCString::Atoi64(*TextValue);
				NumericProperty->SetIntPropertyValue(ValuePtr, IntValue);
			}
			else
			{
				const double FloatValue = JsonValue.IsValid() && JsonValue->Type == EJson::Number
					? JsonValue->AsNumber()
					: FCString::Atod(*TextValue);
				NumericProperty->SetFloatingPointPropertyValue(ValuePtr, FloatValue);
			}
			return true;
		}

		if (FStrProperty* StringProperty = CastField<FStrProperty>(Property))
		{
			StringProperty->SetPropertyValue(ValuePtr, TextValue);
			return true;
		}

		if (FNameProperty* NameProperty = CastField<FNameProperty>(Property))
		{
			NameProperty->SetPropertyValue(ValuePtr, FName(*TextValue));
			return true;
		}

		if (FTextProperty* TextProperty = CastField<FTextProperty>(Property))
		{
			TextProperty->SetPropertyValue(ValuePtr, FText::FromString(TextValue));
			return true;
		}

		if (FEnumProperty* EnumProperty = CastField<FEnumProperty>(Property))
		{
			UEnum* Enum = EnumProperty->GetEnum();
			const int64 EnumValue = Enum ? Enum->GetValueByNameString(TextValue) : INDEX_NONE;
			if (EnumValue == INDEX_NONE)
			{
				OutError = FString::Printf(TEXT("Enum value '%s' is not valid for property '%s'."), *TextValue, *Property->GetName());
				return false;
			}
			EnumProperty->GetUnderlyingProperty()->SetIntPropertyValue(ValuePtr, EnumValue);
			return true;
		}

		if (FByteProperty* ByteProperty = CastField<FByteProperty>(Property))
		{
			if (ByteProperty->Enum)
			{
				const int64 EnumValue = ByteProperty->Enum->GetValueByNameString(TextValue);
				if (EnumValue == INDEX_NONE)
				{
					OutError = FString::Printf(TEXT("Enum value '%s' is not valid for byte property '%s'."), *TextValue, *Property->GetName());
					return false;
				}
				ByteProperty->SetIntPropertyValue(ValuePtr, EnumValue);
			}
			else
			{
				ByteProperty->SetIntPropertyValue(ValuePtr, static_cast<int64>(FCString::Atoi(*TextValue)));
			}
			return true;
		}

		if (FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(Property))
		{
			UObject* ObjectValue = nullptr;
			if (JsonValue.IsValid() && JsonValue->Type != EJson::Null && !TextValue.IsEmpty())
			{
				ObjectValue = StaticLoadObject(ObjectProperty->PropertyClass, nullptr, *TextValue);
				if (!ObjectValue)
				{
					ObjectValue = FindObject<UObject>(nullptr, *TextValue);
				}
				if (!ObjectValue)
				{
					OutError = FString::Printf(TEXT("Object '%s' could not be loaded for property '%s'."), *TextValue, *Property->GetName());
					return false;
				}
			}
			ObjectProperty->SetObjectPropertyValue(ValuePtr, ObjectValue);
			return true;
		}

		if (!TextValue.IsEmpty() && Property->ImportText_Direct(*TextValue, ValuePtr, Object, PPF_None) != nullptr)
		{
			return true;
		}

		OutError = FString::Printf(TEXT("Could not import value '%s' for property '%s'."), *TextValue, *Property->GetName());
		return false;
	}

	TSharedPtr<FJsonObject> ApplyBluecodeNodeProperties(UEdGraphNode* Node, const TSharedPtr<FJsonObject>& Payload)
	{
		TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
		Result->SetBoolField(TEXT("success"), true);
		Result->SetNumberField(TEXT("requested_count"), 0);
		Result->SetNumberField(TEXT("applied_count"), 0);

		if (!Node || !Payload.IsValid())
		{
			return Result;
		}

		const TSharedPtr<FJsonObject>* PropertiesObj = nullptr;
		if (!Payload->TryGetObjectField(TEXT("nodeProperties"), PropertiesObj) || !PropertiesObj || !PropertiesObj->IsValid())
		{
			return Result;
		}

		TArray<TSharedPtr<FJsonValue>> Operations;
		TArray<TSharedPtr<FJsonValue>> Failures;
		int32 AppliedCount = 0;
		Node->Modify();

		for (const auto& Pair : (*PropertiesObj)->Values)
		{
			const FString PropertyName = UmgMcpJsonCompat::KeyToString(Pair.Key);
			TSharedPtr<FJsonObject> Operation = MakeShared<FJsonObject>();
			Operation->SetStringField(TEXT("property"), PropertyName);

			TArray<FString> Candidates;
			FProperty* Property = FindBluecodeObjectProperty(Node->GetClass(), PropertyName, &Candidates);
			if (!Property)
			{
				Operation->SetBoolField(TEXT("success"), false);
				Operation->SetStringField(TEXT("error"), FString::Printf(TEXT("Node property '%s' was not found on %s."), *PropertyName, *Node->GetClass()->GetName()));
				if (Candidates.Num() > 0)
				{
					TArray<TSharedPtr<FJsonValue>> CandidateValues;
					for (const FString& Candidate : Candidates)
					{
						CandidateValues.Add(MakeShared<FJsonValueString>(Candidate));
					}
					Operation->SetArrayField(TEXT("candidates"), CandidateValues);
				}
				Failures.Add(MakeShared<FJsonValueObject>(Operation));
				Operations.Add(MakeShared<FJsonValueObject>(Operation));
				continue;
			}

			FString ErrorMessage;
			if (TrySetBluecodeObjectProperty(Node, Property, Pair.Value, ErrorMessage))
			{
				Operation->SetBoolField(TEXT("success"), true);
				Operation->SetStringField(TEXT("property_type"), Property->GetClass()->GetName());
				++AppliedCount;
			}
			else
			{
				Operation->SetBoolField(TEXT("success"), false);
				Operation->SetStringField(TEXT("property_type"), Property->GetClass()->GetName());
				Operation->SetStringField(TEXT("error"), ErrorMessage);
				Failures.Add(MakeShared<FJsonValueObject>(Operation));
			}
			Operations.Add(MakeShared<FJsonValueObject>(Operation));
		}

		if (AppliedCount > 0)
		{
			Node->ReconstructNode();
			if (UEdGraph* Graph = Node->GetGraph())
			{
				Graph->NotifyNodeChanged(Node);
			}
			Node->PostEditChange();
		}

		Result->SetBoolField(TEXT("success"), Failures.Num() == 0);
		Result->SetNumberField(TEXT("requested_count"), (*PropertiesObj)->Values.Num());
		Result->SetNumberField(TEXT("applied_count"), AppliedCount);
		Result->SetArrayField(TEXT("operations"), Operations);
		Result->SetArrayField(TEXT("failures"), Failures);
		return Result;
	}

	bool BluecodeDefaultObjectMatches(const UObject* Object, const FString& Value)
	{
		if (!Object)
		{
			return false;
		}
		return Object->GetPathName().Equals(Value, ESearchCase::IgnoreCase) ||
			Object->GetName().Equals(Value, ESearchCase::IgnoreCase);
	}

	bool TrySetBluecodePinDefault(UEdGraphPin* Pin, const FString& RawValue, FString& OutError)
	{
		if (!Pin || !Pin->GetSchema())
		{
			OutError = TEXT("Missing pin or schema.");
			return false;
		}

		const FString Value = StripBluecodeQuotes(RawValue);
		const bool bTextualPin =
			Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_String ||
			Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Text ||
			Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Name;
		if (Value.IsEmpty() && !bTextualPin)
		{
			OutError = TEXT("Empty literal value.");
			return false;
		}

		const FString OldDefaultValue = Pin->DefaultValue;
		UObject* OldDefaultObject = Pin->DefaultObject;
		const FText OldDefaultText = Pin->DefaultTextValue;

		if (Pin->DefaultValue.Equals(Value, ESearchCase::CaseSensitive) ||
			BluecodeDefaultObjectMatches(Pin->DefaultObject, Value) ||
			(!Pin->DefaultTextValue.IsEmpty() && Pin->DefaultTextValue.ToString().Equals(Value, ESearchCase::CaseSensitive)))
		{
			return true;
		}

		if (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Text)
		{
			Pin->GetSchema()->TrySetDefaultText(*Pin, FText::FromString(Value));
		}
		else
		{
			// K2 parses object/class asset paths, enum names, structs, numerics, booleans, and strings here.
			Pin->GetSchema()->TrySetDefaultValue(*Pin, Value);
		}

		const bool bChanged =
			!Pin->DefaultValue.Equals(OldDefaultValue, ESearchCase::CaseSensitive) ||
			Pin->DefaultObject != OldDefaultObject ||
			!Pin->DefaultTextValue.EqualTo(OldDefaultText);

		const bool bMatches =
			Pin->DefaultValue.Equals(Value, ESearchCase::CaseSensitive) ||
			BluecodeDefaultObjectMatches(Pin->DefaultObject, Value) ||
			(!Pin->DefaultTextValue.IsEmpty() && Pin->DefaultTextValue.ToString().Equals(Value, ESearchCase::CaseSensitive));

		if (bChanged || bMatches)
		{
			return true;
		}

		OutError = FString::Printf(TEXT("Could not set pin '%s' default from '%s'."), *Pin->PinName.ToString(), *RawValue);
		return false;
	}

	bool TryGetBluecodeInputWireValue(const TSharedPtr<FJsonObject>& InputWires, const UEdGraphPin* Pin, FString& OutValue)
	{
		if (!InputWires.IsValid() || !Pin)
		{
			return false;
		}

		const FString NormalizedPin = NormalizeBluecodePinName(Pin->PinName.ToString());
		for (const auto& Pair : InputWires->Values)
		{
			const FString Key = UmgMcpJsonCompat::KeyToString(Pair.Key);
			if (BluecodePinNameMatches(Pin, Key) ||
				NormalizeBluecodePinName(Key) == NormalizedPin)
			{
				OutValue = BluecodeJsonValueToString(Pair.Value);
				return true;
			}
		}
		return false;
	}

	bool IsBluecodeDynamicInputKey(const FString& Key)
	{
		FString Normalized = NormalizeBluecodePinName(Key);
		if (Normalized.IsEmpty())
		{
			return false;
		}

		if (Normalized.IsNumeric())
		{
			return true;
		}

		if (Normalized.StartsWith(TEXT("option")))
		{
			const FString Suffix = Normalized.Mid(6);
			return !Suffix.IsEmpty() && Suffix.IsNumeric();
		}

		if (Normalized.StartsWith(TEXT("then")))
		{
			const FString Suffix = Normalized.Mid(4);
			return !Suffix.IsEmpty() && Suffix.IsNumeric();
		}

		return false;
	}

	UEdGraphPin* FindBluecodePin(UEdGraphNode* Node, const FString& PinName, const EEdGraphPinDirection Direction, TArray<FString>* OutCandidates = nullptr)
	{
		if (!Node)
		{
			return nullptr;
		}

		TArray<UEdGraphPin*> DirectionPins;
		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin && Pin->Direction == Direction && IsBluecodeDataPin(Pin))
			{
				DirectionPins.Add(Pin);
			}
		}

		for (UEdGraphPin* Pin : DirectionPins)
		{
			if (BluecodePinNameMatches(Pin, PinName))
			{
				return Pin;
			}
		}

		const bool bSource = Direction == EGPD_Output;
		if (IsBluecodeRoleAlias(PinName, bSource) && DirectionPins.Num() == 1)
		{
			return DirectionPins[0];
		}

		if (OutCandidates)
		{
			for (UEdGraphPin* Pin : DirectionPins)
			{
				OutCandidates->Add(Pin->PinName.ToString());
			}
		}
		return nullptr;
	}

	FString StripBluecodeQuotes(FString Value)
	{
		Value.TrimStartAndEndInline();
		if (Value.Len() >= 2)
		{
			const TCHAR First = Value[0];
			const TCHAR Last = Value[Value.Len() - 1];
			if ((First == TEXT('"') && Last == TEXT('"')) || (First == TEXT('\'') && Last == TEXT('\'')))
			{
				return Value.Mid(1, Value.Len() - 2);
			}
		}
		return Value;
	}

	TArray<FString> SplitBluecodeArgs(const FString& ArgText)
	{
		TArray<FString> Args;
		FString Current;
		TCHAR Quote = 0;
		int32 Depth = 0;

		for (int32 Index = 0; Index < ArgText.Len(); ++Index)
		{
			const TCHAR Ch = ArgText[Index];
			if (Quote != 0)
			{
				Current.AppendChar(Ch);
				if (Ch == Quote)
				{
					Quote = 0;
				}
				continue;
			}
			if (Ch == TEXT('"') || Ch == TEXT('\''))
			{
				Quote = Ch;
				Current.AppendChar(Ch);
				continue;
			}
			if (Ch == TEXT('(') || Ch == TEXT('[') || Ch == TEXT('{'))
			{
				++Depth;
			}
			else if (Ch == TEXT(')') || Ch == TEXT(']') || Ch == TEXT('}'))
			{
				Depth = FMath::Max(0, Depth - 1);
			}
			if (Ch == TEXT(',') && Depth == 0)
			{
				Current.TrimStartAndEndInline();
				if (!Current.IsEmpty())
				{
					Args.Add(StripBluecodeQuotes(Current));
				}
				Current.Reset();
				continue;
			}
			Current.AppendChar(Ch);
		}

		Current.TrimStartAndEndInline();
		if (!Current.IsEmpty())
		{
			Args.Add(StripBluecodeQuotes(Current));
		}
		return Args;
	}

	TArray<FString> ParseBluecodeStatements(const FString& Code)
	{
		TArray<FString> Lines;
		Code.ParseIntoArrayLines(Lines, false);

		TArray<FString> Statements;
		for (FString Line : Lines)
		{
			Line.TrimStartAndEndInline();
			Line.RemoveFromEnd(TEXT(";"));
			Line.TrimStartAndEndInline();
			if (Line.IsEmpty() || Line.StartsWith(TEXT("#")) || Line.StartsWith(TEXT("//")) || Line.StartsWith(TEXT("```")))
			{
				continue;
			}
			if (Line.Equals(TEXT("main()"), ESearchCase::IgnoreCase) ||
				Line.Equals(TEXT("main:"), ESearchCase::IgnoreCase) ||
				Line.Equals(TEXT("end"), ESearchCase::IgnoreCase) ||
				Line.Equals(TEXT("return"), ESearchCase::IgnoreCase))
			{
				continue;
			}
			Statements.Add(Line);
		}
		return Statements;
	}

	bool TryParseBluecodeCall(const FString& Statement, FString& OutName, TArray<FString>& OutArgs)
	{
		const int32 OpenIndex = Statement.Find(TEXT("("));
		const int32 CloseIndex = Statement.Find(TEXT(")"), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
		if (OpenIndex <= 0 || CloseIndex <= OpenIndex)
		{
			return false;
		}

		OutName = Statement.Left(OpenIndex);
		OutName.TrimStartAndEndInline();
		FString ArgText = Statement.Mid(OpenIndex + 1, CloseIndex - OpenIndex - 1);
		OutArgs = SplitBluecodeArgs(ArgText);
		return !OutName.IsEmpty();
	}

	bool TryParseBluecodeAssignment(const FString& Statement, FString& OutName, FString& OutValue)
	{
		if (Statement.StartsWith(TEXT("if "), ESearchCase::IgnoreCase))
		{
			return false;
		}

		// Assignment is only valid for a top-level '='. Named call arguments such as
		// node("Action", Pin=value) must remain calls; the old first-'=' split turned
		// them into bogus K2Node_VariableSet nodes.
		const int32 AssignmentIndex = FBluecodeSyntax::FindTopLevelAssignmentOperator(Statement);
		if (AssignmentIndex == INDEX_NONE)
		{
			return false;
		}

		FString Left = Statement.Left(AssignmentIndex);
		FString Right = Statement.Mid(AssignmentIndex + 1);
		Left.TrimStartAndEndInline();
		Right.TrimStartAndEndInline();
		if (Left.IsEmpty() || Right.IsEmpty())
		{
			return false;
		}
		if (!FBluecodeSyntax::IsIdentifier(Left))
		{
			return false;
		}

		OutName = Left;
		OutValue = StripBluecodeQuotes(Right);
		return true;
	}

	bool IsBluecodeIdentifier(const FString& Value)
	{
		return FBluecodeSyntax::IsIdentifier(Value);
	}

	bool IsBluecodeNamedPinArgName(const FString& Value)
	{
		FString Text = StripBluecodeQuotes(Value);
		Text.TrimStartAndEndInline();
		if (Text.IsEmpty())
		{
			return false;
		}
		return !Text.Contains(TEXT("=")) &&
			!Text.Contains(TEXT("->")) &&
			!Text.Contains(TEXT("\n")) &&
			!Text.Contains(TEXT("\r"));
	}

	bool IsBluecodeNumber(const FString& Value)
	{
		FString Text = Value;
		Text.TrimStartAndEndInline();
		if (Text.IsEmpty())
		{
			return false;
		}

		bool bHasDigit = false;
		bool bHasDot = false;
		for (int32 Index = 0; Index < Text.Len(); ++Index)
		{
			const TCHAR Ch = Text[Index];
			if ((Ch == TEXT('+') || Ch == TEXT('-')) && Index == 0)
			{
				continue;
			}
			if (Ch == TEXT('.') && !bHasDot)
			{
				bHasDot = true;
				continue;
			}
			if (!FChar::IsDigit(Ch))
			{
				return false;
			}
			bHasDigit = true;
		}
		return bHasDigit;
	}

	bool IsBluecodeMathOperand(const FString& Value)
	{
		return IsBluecodeIdentifier(Value) || IsBluecodeNumber(Value);
	}

	bool TryParseBluecodeBinaryExpression(const FString& Value, FString& OutLeft, FString& OutOp, FString& OutRight)
	{
		FString Text = Value;
		Text.TrimStartAndEndInline();

		TCHAR Quote = 0;
		int32 Depth = 0;
		for (int32 Index = 0; Index < Text.Len(); ++Index)
		{
			const TCHAR Ch = Text[Index];
			if (Quote != 0)
			{
				if (Ch == Quote && (Index == 0 || Text[Index - 1] != TEXT('\\')))
				{
					Quote = 0;
				}
				continue;
			}
			if (Ch == TEXT('"') || Ch == TEXT('\''))
			{
				Quote = Ch;
				continue;
			}
			if (Ch == TEXT('(') || Ch == TEXT('[') || Ch == TEXT('{'))
			{
				++Depth;
				continue;
			}
			if (Ch == TEXT(')') || Ch == TEXT(']') || Ch == TEXT('}'))
			{
				Depth = FMath::Max(0, Depth - 1);
				continue;
			}
			if (Depth == 0 && (Ch == TEXT('+') || Ch == TEXT('-') || Ch == TEXT('*') || Ch == TEXT('/')))
			{
				if (Ch == TEXT('-') && (Index == 0 || FString(TEXT("+-*/(")).Contains(FString::Chr(Text[Index - 1]))))
				{
					continue;
				}

				FString Left = Text.Left(Index);
				FString Right = Text.Mid(Index + 1);
				Left.TrimStartAndEndInline();
				Right.TrimStartAndEndInline();
				if (IsBluecodeMathOperand(Left) && IsBluecodeMathOperand(Right))
				{
					OutLeft = StripBluecodeQuotes(Left);
					OutOp = FString::Chr(Ch);
					OutRight = StripBluecodeQuotes(Right);
					return true;
				}
			}
		}
		return false;
	}

	FString BluecodeVarTypeFamily(const UBlueprint* Blueprint, const FString& VarName)
	{
		if (!Blueprint)
		{
			return TEXT("");
		}
		for (const FBPVariableDescription& Var : Blueprint->NewVariables)
		{
			if (!Var.VarName.ToString().Equals(VarName, ESearchCase::CaseSensitive))
			{
				continue;
			}

			const FString Category = Var.VarType.PinCategory.ToString().ToLower();
			if (Category.Contains(TEXT("float")) || Category.Contains(TEXT("double")) || Category.Contains(TEXT("real")))
			{
				return TEXT("float");
			}
			if (Category.Contains(TEXT("int")) || Category.Contains(TEXT("byte")))
			{
				return TEXT("int");
			}
			return TEXT("");
		}
		return TEXT("");
	}

	FString BluecodeMathTypeFamily(const UBlueprint* Blueprint, const FString& TargetName, const FString& Left, const FString& Right)
	{
		FString Family = (Left.Contains(TEXT(".")) || Right.Contains(TEXT("."))) ? TEXT("float") : TEXT("int");
		const FString TargetFamily = BluecodeVarTypeFamily(Blueprint, TargetName);
		if (!TargetFamily.IsEmpty())
		{
			return TargetFamily;
		}
		const FString LeftFamily = BluecodeVarTypeFamily(Blueprint, Left);
		const FString RightFamily = BluecodeVarTypeFamily(Blueprint, Right);
		if (LeftFamily == TEXT("float") || RightFamily == TEXT("float"))
		{
			return TEXT("float");
		}
		if (LeftFamily == TEXT("int") || RightFamily == TEXT("int"))
		{
			return TEXT("int");
		}
		return Family;
	}

	FString BluecodeMathFunctionName(const FString& Op, const FString& Family)
	{
		if (Op == TEXT("+"))
		{
			return Family == TEXT("int") ? TEXT("Add_IntInt") : TEXT("Add_FloatFloat");
		}
		if (Op == TEXT("-"))
		{
			return Family == TEXT("int") ? TEXT("Subtract_IntInt") : TEXT("Subtract_FloatFloat");
		}
		if (Op == TEXT("*"))
		{
			return Family == TEXT("int") ? TEXT("Multiply_IntInt") : TEXT("Multiply_FloatFloat");
		}
		if (Op == TEXT("/"))
		{
			return Family == TEXT("int") ? TEXT("Divide_IntInt") : TEXT("Divide_FloatFloat");
		}
		return TEXT("");
	}

	FString QuoteBluecodeString(FString Value)
	{
		Value.ReplaceInline(TEXT("\\"), TEXT("\\\\"));
		Value.ReplaceInline(TEXT("\""), TEXT("\\\""));
		Value.ReplaceInline(TEXT("\r"), TEXT("\\r"));
		Value.ReplaceInline(TEXT("\n"), TEXT("\\n"));
		return FString::Printf(TEXT("\"%s\""), *Value);
	}

	FString BluecodeNodeTitle(const UEdGraphNode* Node)
	{
		return Node ? Node->GetNodeTitle(ENodeTitleType::ListView).ToString() : TEXT("");
	}

	bool IsBluecodeDataInputPin(const UEdGraphPin* Pin)
	{
		return Pin &&
			Pin->Direction == EGPD_Input &&
			!Pin->bHidden &&
			Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec &&
			Pin->PinName != UEdGraphSchema_K2::PN_Self;
	}

	FString BluecodeMathOperatorFromFunctionName(const FString& FunctionName)
	{
		if (FunctionName == TEXT("Add_IntInt") || FunctionName == TEXT("Add_FloatFloat"))
		{
			return TEXT("+");
		}
		if (FunctionName == TEXT("Subtract_IntInt") || FunctionName == TEXT("Subtract_FloatFloat"))
		{
			return TEXT("-");
		}
		if (FunctionName == TEXT("Multiply_IntInt") || FunctionName == TEXT("Multiply_FloatFloat"))
		{
			return TEXT("*");
		}
		if (FunctionName == TEXT("Divide_IntInt") || FunctionName == TEXT("Divide_FloatFloat"))
		{
			return TEXT("/");
		}
		return TEXT("");
	}

	FString BluecodeExpressionFromPin(const UEdGraphPin* Pin, TSet<const UEdGraphNode*>& Seen);
	FString JoinBluecodeCallArgs(const TArray<FString>& Args);

	TArray<FString> BluecodeInputExpressions(const UEdGraphNode* Node, TSet<const UEdGraphNode*>& Seen, const bool bNamed)
	{
		TArray<FString> Args;
		if (!Node)
		{
			return Args;
		}

		for (const UEdGraphPin* Pin : Node->Pins)
		{
			if (!IsBluecodeReadableInputPin(Pin))
			{
				continue;
			}

			const FString Value = BluecodeExpressionFromPin(Pin, Seen);
			if (!Value.IsEmpty())
			{
				Args.Add(bNamed
					? FString::Printf(TEXT("%s=%s"), *Pin->PinName.ToString(), *Value)
					: Value);
			}
		}
		return Args;
	}

	FString BluecodeExpressionFromNode(const UEdGraphNode* Node, TSet<const UEdGraphNode*>& Seen)
	{
		if (!Node)
		{
			return TEXT("");
		}
		if (Seen.Contains(Node))
		{
			return QuoteBluecodeString(BluecodeNodeTitle(Node));
		}
		Seen.Add(Node);

		if (const UK2Node_VariableGet* GetNode = Cast<UK2Node_VariableGet>(Node))
		{
			return GetNode->VariableReference.GetMemberName().ToString();
		}

		if (const UK2Node_CallFunction* CallNode = Cast<UK2Node_CallFunction>(Node))
		{
			const FString FunctionName = CallNode->FunctionReference.GetMemberName().ToString();
			TArray<FString> PositionalArgs = BluecodeInputExpressions(Node, Seen, false);
			const FString MathOp = BluecodeMathOperatorFromFunctionName(FunctionName);
			if (!MathOp.IsEmpty() && PositionalArgs.Num() >= 2)
			{
				return FString::Printf(TEXT("%s %s %s"), *PositionalArgs[0], *MathOp, *PositionalArgs[1]);
			}
			return FString::Printf(TEXT("%s(%s)"), *FunctionName, *JoinBluecodeCallArgs(PositionalArgs));
		}

		const FString Title = BluecodeNodeTitle(Node);
		if (!Title.IsEmpty())
		{
			TArray<FString> Args;
			Args.Add(QuoteBluecodeString(Title));
			TArray<FString> NamedArgs = BluecodeInputExpressions(Node, Seen, true);
			Args.Append(NamedArgs);
			return FString::Printf(TEXT("value(%s)"), *JoinBluecodeCallArgs(Args));
		}
		return TEXT("");
	}

	FString BluecodeExpressionFromPin(const UEdGraphPin* Pin, TSet<const UEdGraphNode*>& Seen)
	{
		if (!Pin)
		{
			return TEXT("");
		}

		if (Pin->LinkedTo.Num() > 0 && Pin->LinkedTo[0] && Pin->LinkedTo[0]->GetOwningNode())
		{
			const UEdGraphNode* LinkedNode = Pin->LinkedTo[0]->GetOwningNode();
			if (!NodeHasExecPin(LinkedNode))
			{
				return BluecodeExpressionFromNode(LinkedNode, Seen);
			}
			return QuoteBluecodeString(BluecodeNodeTitle(LinkedNode));
		}

		if (Pin->DefaultObject)
		{
			return QuoteBluecodeString(Pin->DefaultObject->GetPathName());
		}

		if (!Pin->DefaultTextValue.IsEmpty())
		{
			return QuoteBluecodeString(Pin->DefaultTextValue.ToString());
		}

		if (Pin->DefaultValue.IsEmpty())
		{
			return TEXT("");
		}

		const FName Category = Pin->PinType.PinCategory;
		if (Category == UEdGraphSchema_K2::PC_String ||
			Category == UEdGraphSchema_K2::PC_Text ||
			Category == UEdGraphSchema_K2::PC_Name)
		{
			return QuoteBluecodeString(Pin->DefaultValue);
		}
		return Pin->DefaultValue;
	}

	FString BluecodePinValue(const UEdGraphPin* Pin)
	{
		TSet<const UEdGraphNode*> Seen;
		return BluecodeExpressionFromPin(Pin, Seen);
	}

	FString BluecodePinEndpoint(const UEdGraphPin* Pin)
	{
		if (!Pin || !Pin->GetOwningNode())
		{
			return TEXT("");
		}
		return FString::Printf(TEXT("%s:%s"),
			*Pin->GetOwningNode()->NodeGuid.ToString(),
			*Pin->PinName.ToString());
	}

	TSharedPtr<FJsonObject> MakeBluecodeLinkJson(const UEdGraphPin* SourcePin, const UEdGraphPin* TargetPin)
	{
		TSharedPtr<FJsonObject> LinkObj = MakeShared<FJsonObject>();
		const UEdGraphNode* SourceNode = SourcePin ? SourcePin->GetOwningNode() : nullptr;
		const UEdGraphNode* TargetNode = TargetPin ? TargetPin->GetOwningNode() : nullptr;

		LinkObj->SetStringField(TEXT("source"), BluecodePinEndpoint(SourcePin));
		LinkObj->SetStringField(TEXT("target"), BluecodePinEndpoint(TargetPin));
		LinkObj->SetStringField(TEXT("kind"),
			(SourcePin && SourcePin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec) ? TEXT("exec") : TEXT("data"));
		if (SourceNode)
		{
			LinkObj->SetStringField(TEXT("source_node_id"), SourceNode->NodeGuid.ToString());
			LinkObj->SetStringField(TEXT("source_title"), BluecodeNodeTitle(SourceNode));
			LinkObj->SetStringField(TEXT("source_class"), SourceNode->GetClass() ? SourceNode->GetClass()->GetName() : TEXT(""));
		}
		if (SourcePin)
		{
			LinkObj->SetStringField(TEXT("source_pin"), SourcePin->PinName.ToString());
		}
		if (TargetNode)
		{
			LinkObj->SetStringField(TEXT("target_node_id"), TargetNode->NodeGuid.ToString());
			LinkObj->SetStringField(TEXT("target_title"), BluecodeNodeTitle(TargetNode));
			LinkObj->SetStringField(TEXT("target_class"), TargetNode->GetClass() ? TargetNode->GetClass()->GetName() : TEXT(""));
		}
		if (TargetPin)
		{
			LinkObj->SetStringField(TEXT("target_pin"), TargetPin->PinName.ToString());
		}
		LinkObj->SetStringField(TEXT("connect"), FString::Printf(TEXT("%s -> %s"), *BluecodePinEndpoint(SourcePin), *BluecodePinEndpoint(TargetPin)));

		TSharedPtr<FJsonObject> DeleteObj = MakeShared<FJsonObject>();
		DeleteObj->SetStringField(TEXT("kind"), TEXT("connection"));
		DeleteObj->SetStringField(TEXT("source"), BluecodePinEndpoint(SourcePin));
		DeleteObj->SetStringField(TEXT("target"), BluecodePinEndpoint(TargetPin));
		LinkObj->SetObjectField(TEXT("delete_target"), DeleteObj);
		return LinkObj;
	}

	TSharedPtr<FJsonObject> MakeBluecodePinHintJson(const UEdGraphPin* Pin)
	{
		TSharedPtr<FJsonObject> PinObj = MakeShared<FJsonObject>();
		if (!Pin)
		{
			return PinObj;
		}

		PinObj->SetStringField(TEXT("name"), Pin->PinName.ToString());
		const FString DisplayName = Pin->GetDisplayName().ToString();
		if (!DisplayName.IsEmpty() && !DisplayName.Equals(Pin->PinName.ToString(), ESearchCase::CaseSensitive))
		{
			PinObj->SetStringField(TEXT("display_name"), DisplayName);
		}
		if (!Pin->PinFriendlyName.IsEmpty())
		{
			PinObj->SetStringField(TEXT("friendly_name"), Pin->PinFriendlyName.ToString());
		}
		PinObj->SetStringField(TEXT("direction"), Pin->Direction == EGPD_Input ? TEXT("input") : TEXT("output"));
		PinObj->SetStringField(TEXT("category"), Pin->PinType.PinCategory.ToString());
		PinObj->SetBoolField(TEXT("is_exec"), Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec);
		PinObj->SetBoolField(TEXT("hidden"), Pin->bHidden);
		PinObj->SetStringField(TEXT("endpoint"), BluecodePinEndpoint(Pin));
		if (!Pin->PinType.PinSubCategory.ToString().IsEmpty())
		{
			PinObj->SetStringField(TEXT("sub_category"), Pin->PinType.PinSubCategory.ToString());
		}
		if (Pin->PinType.PinSubCategoryObject.IsValid())
		{
			PinObj->SetStringField(TEXT("sub_type"), Pin->PinType.PinSubCategoryObject->GetName());
			PinObj->SetStringField(TEXT("sub_type_path"), Pin->PinType.PinSubCategoryObject->GetPathName());
		}

		if (Pin->Direction == EGPD_Input && Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec)
		{
			const FString Expression = BluecodePinValue(Pin);
			if (!Expression.IsEmpty())
			{
				PinObj->SetStringField(TEXT("expression"), Expression);
			}
			if (!Pin->DefaultValue.IsEmpty())
			{
				PinObj->SetStringField(TEXT("default"), Pin->DefaultValue);
			}
			if (!Pin->DefaultTextValue.IsEmpty())
			{
				PinObj->SetStringField(TEXT("default_text"), Pin->DefaultTextValue.ToString());
			}
			if (Pin->DefaultObject)
			{
				PinObj->SetStringField(TEXT("default_object"), Pin->DefaultObject->GetPathName());
			}
		}

		TArray<TSharedPtr<FJsonValue>> LinksArray;
		for (const UEdGraphPin* LinkedPin : Pin->LinkedTo)
		{
			if (!LinkedPin)
			{
				continue;
			}
			const UEdGraphPin* SourcePin = Pin->Direction == EGPD_Output ? Pin : LinkedPin;
			const UEdGraphPin* TargetPin = Pin->Direction == EGPD_Output ? LinkedPin : Pin;
			LinksArray.Add(MakeShared<FJsonValueObject>(MakeBluecodeLinkJson(SourcePin, TargetPin)));
		}
		if (LinksArray.Num() > 0)
		{
			PinObj->SetArrayField(TEXT("links"), LinksArray);
		}
		return PinObj;
	}

	void AddBluecodePinHintArrays(const UEdGraphNode* Node, const TSharedPtr<FJsonObject>& OutObj)
	{
		if (!Node || !OutObj.IsValid())
		{
			return;
		}

		TArray<TSharedPtr<FJsonValue>> Inputs;
		TArray<TSharedPtr<FJsonValue>> Outputs;
		for (const UEdGraphPin* Pin : Node->Pins)
		{
			if (!Pin || Pin->bHidden)
			{
				continue;
			}
			if (Pin->Direction == EGPD_Input)
			{
				Inputs.Add(MakeShared<FJsonValueObject>(MakeBluecodePinHintJson(Pin)));
			}
			else
			{
				Outputs.Add(MakeShared<FJsonValueObject>(MakeBluecodePinHintJson(Pin)));
			}
		}
		OutObj->SetArrayField(TEXT("inputs"), Inputs);
		OutObj->SetArrayField(TEXT("outputs"), Outputs);
	}

	void AddBluecodeNodeResultFields(const UEdGraphNode* Node, const TSharedPtr<FJsonObject>& OutObj)
	{
		if (!Node || !OutObj.IsValid())
		{
			return;
		}

		OutObj->SetStringField(TEXT("node_id"), Node->NodeGuid.ToString());
		OutObj->SetStringField(TEXT("title"), BluecodeNodeTitle(Node));
		OutObj->SetStringField(TEXT("node_class"), Node->GetClass() ? Node->GetClass()->GetName() : TEXT(""));
		OutObj->SetStringField(TEXT("node_class_path"), Node->GetClass() ? Node->GetClass()->GetPathName() : TEXT(""));
		OutObj->SetBoolField(TEXT("is_exec"), NodeHasExecPin(Node));
		OutObj->SetObjectField(TEXT("pin_counts"), MakePinCounts(Node));
		AddBluecodePinHintArrays(Node, OutObj);
	}

	TArray<TSharedPtr<FJsonValue>> CollectBluecodeConnections(UEdGraph* Graph)
	{
		TArray<TSharedPtr<FJsonValue>> Connections;
		TSet<FString> Seen;
		if (!Graph)
		{
			return Connections;
		}

		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (!Node)
			{
				continue;
			}
			for (UEdGraphPin* Pin : Node->Pins)
			{
				if (!Pin || Pin->bHidden || Pin->Direction != EGPD_Output)
				{
					continue;
				}
				for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
				{
					if (!LinkedPin || LinkedPin->bHidden)
					{
						continue;
					}
					const FString Key = BluecodePinEndpoint(Pin) + TEXT("->") + BluecodePinEndpoint(LinkedPin);
					if (Seen.Contains(Key))
					{
						continue;
					}
					Seen.Add(Key);
					Connections.Add(MakeShared<FJsonValueObject>(MakeBluecodeLinkJson(Pin, LinkedPin)));
				}
			}
		}
		return Connections;
	}

	void CollectBluecodeDependencyNodes(const UEdGraphNode* Node, TSet<const UEdGraphNode*>& OutDependencyNodes)
	{
		if (!Node)
		{
			return;
		}

		for (const UEdGraphPin* Pin : Node->Pins)
		{
			if (!IsBluecodeReadableInputPin(Pin))
			{
				continue;
			}
			for (const UEdGraphPin* LinkedPin : Pin->LinkedTo)
			{
				const UEdGraphNode* LinkedNode = LinkedPin ? LinkedPin->GetOwningNode() : nullptr;
				if (LinkedNode && !NodeHasExecPin(LinkedNode) && !OutDependencyNodes.Contains(LinkedNode))
				{
					OutDependencyNodes.Add(LinkedNode);
					CollectBluecodeDependencyNodes(LinkedNode, OutDependencyNodes);
				}
			}
		}
	}

	bool IsBluecodeEntryNode(const UEdGraphNode* Node)
	{
		return Cast<UK2Node_Event>(Node) ||
			Cast<UK2Node_ComponentBoundEvent>(Node) ||
			Cast<UK2Node_CustomEvent>(Node) ||
			Cast<UK2Node_FunctionEntry>(Node);
	}

	bool HasBluecodeExecInputLink(const UEdGraphNode* Node)
	{
		if (!Node)
		{
			return false;
		}
		for (const UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin &&
				Pin->Direction == EGPD_Input &&
				Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec &&
				Pin->LinkedTo.Num() > 0)
			{
				return true;
			}
		}
		return false;
	}

	void VisitBluecodeExecFlowNode(UEdGraphNode* Node, TSet<UEdGraphNode*>& Visited, TArray<UEdGraphNode*>& Ordered)
	{
		if (!Node || Visited.Contains(Node))
		{
			return;
		}

		Visited.Add(Node);
		Ordered.Add(Node);

		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (!Pin ||
				Pin->Direction != EGPD_Output ||
				Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec)
			{
				continue;
			}

			for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
			{
				if (LinkedPin && LinkedPin->GetOwningNode())
				{
					VisitBluecodeExecFlowNode(LinkedPin->GetOwningNode(), Visited, Ordered);
				}
			}
		}
	}

	TArray<UEdGraphNode*> BuildBluecodeReadOrder(const TArray<UEdGraphNode*>& SortedNodes)
	{
		TArray<UEdGraphNode*> Ordered;
		TSet<UEdGraphNode*> Visited;

		for (UEdGraphNode* Node : SortedNodes)
		{
			if (IsBluecodeEntryNode(Node))
			{
				VisitBluecodeExecFlowNode(Node, Visited, Ordered);
			}
		}

		for (UEdGraphNode* Node : SortedNodes)
		{
			if (Node &&
				!Visited.Contains(Node) &&
				NodeHasExecPin(Node) &&
				!HasBluecodeExecInputLink(Node))
			{
				VisitBluecodeExecFlowNode(Node, Visited, Ordered);
			}
		}

		for (UEdGraphNode* Node : SortedNodes)
		{
			if (Node && !Visited.Contains(Node))
			{
				Visited.Add(Node);
				Ordered.Add(Node);
			}
		}

		return Ordered;
	}

	void AddBluecodeInputArgs(const UEdGraphNode* Node, TArray<FString>& Args)
	{
		if (!Node)
		{
			return;
		}

		for (const UEdGraphPin* Pin : Node->Pins)
		{
			if (!IsBluecodeDataInputPin(Pin))
			{
				continue;
			}

			const FString Value = BluecodePinValue(Pin);
			if (!Value.IsEmpty())
			{
				Args.Add(FString::Printf(TEXT("%s=%s"), *Pin->PinName.ToString(), *Value));
			}
		}
	}

	FString JoinBluecodeCallArgs(const TArray<FString>& Args)
	{
		return FString::Join(Args, TEXT(", "));
	}

	bool TrySplitBluecodeNamedArg(const FString& Arg, FString& OutName, FString& OutValue)
	{
		TCHAR Quote = 0;
		int32 Depth = 0;
		for (int32 Index = 0; Index < Arg.Len(); ++Index)
		{
			const TCHAR Ch = Arg[Index];
			if (Quote != 0)
			{
				if (Ch == Quote && (Index == 0 || Arg[Index - 1] != TEXT('\\')))
				{
					Quote = 0;
				}
				continue;
			}
			if (Ch == TEXT('"') || Ch == TEXT('\''))
			{
				Quote = Ch;
				continue;
			}
			if (Ch == TEXT('(') || Ch == TEXT('[') || Ch == TEXT('{'))
			{
				++Depth;
				continue;
			}
			if (Ch == TEXT(')') || Ch == TEXT(']') || Ch == TEXT('}'))
			{
				Depth = FMath::Max(0, Depth - 1);
				continue;
			}
			if (Depth == 0 && Ch == TEXT('='))
			{
				const TCHAR Prev = Index > 0 ? Arg[Index - 1] : 0;
				const TCHAR Next = Index + 1 < Arg.Len() ? Arg[Index + 1] : 0;
				if (Prev == TEXT('=') || Prev == TEXT('!') || Prev == TEXT('<') || Prev == TEXT('>') || Next == TEXT('='))
				{
					continue;
				}

				FString Name = Arg.Left(Index);
				FString Value = Arg.Mid(Index + 1);
				Name.TrimStartAndEndInline();
				Value.TrimStartAndEndInline();
				if (IsBluecodeNamedPinArgName(Name) && !Value.IsEmpty())
				{
					OutName = StripBluecodeQuotes(Name);
					OutValue = Value;
					return true;
				}
				return false;
			}
		}
		return false;
	}

	void ApplyBluecodeArgsToPayload(
		const TArray<FString>& Args,
		const int32 StartIndex,
		const TSharedPtr<FJsonObject>& Payload)
	{
		if (!Payload.IsValid())
		{
			return;
		}

		TArray<TSharedPtr<FJsonValue>> PositionalArgs;
		TSharedPtr<FJsonObject> InputWires = MakeShared<FJsonObject>();
		bool bHasNamedArgs = false;

		for (int32 Index = StartIndex; Index < Args.Num(); ++Index)
		{
			FString Arg = Args[Index];
			Arg.TrimStartAndEndInline();
			FString Left;
			FString Right;
			if (TrySplitBluecodeNamedArg(Arg, Left, Right))
			{
				InputWires->SetStringField(Left, StripBluecodeQuotes(Right));
				bHasNamedArgs = true;
				continue;
			}
			if (!Arg.IsEmpty())
			{
				PositionalArgs.Add(MakeShared<FJsonValueString>(StripBluecodeQuotes(Arg)));
			}
		}

		if (PositionalArgs.Num() > 0)
		{
			Payload->SetArrayField(TEXT("extraArgs"), PositionalArgs);
		}
		if (bHasNamedArgs)
		{
			Payload->SetObjectField(TEXT("inputWires"), InputWires);
		}
	}

	bool ExtractBluecodeAliasArg(TArray<FString>& Args, const int32 StartIndex, FString& OutAlias)
	{
		for (int32 Index = Args.Num() - 1; Index >= StartIndex; --Index)
		{
			FString Name;
			FString Value;
			if (!TrySplitBluecodeNamedArg(Args[Index], Name, Value))
			{
				continue;
			}
			if (Name.Equals(TEXT("alias"), ESearchCase::IgnoreCase) ||
				Name.Equals(TEXT("_alias"), ESearchCase::IgnoreCase) ||
				Name.Equals(TEXT("as"), ESearchCase::IgnoreCase))
			{
				OutAlias = StripBluecodeQuotes(Value);
				Args.RemoveAt(Index);
				return !OutAlias.IsEmpty();
			}
		}
		return false;
	}

	void ApplyBluecodeAliasHints(
		const FString& Statement,
		const FString& CallName,
		const FString& ResolvedNodeName,
		const TSharedPtr<FJsonObject>& Params,
		FString& InOutAlias)
	{
		if (!Params.IsValid() || !InOutAlias.IsEmpty())
		{
			return;
		}

		const TArray<FString> Keys = {
			Statement,
			NormalizeBluecodeStatement(Statement),
			CallName,
			NormalizeBluecodeStatement(CallName),
			ResolvedNodeName,
			NormalizeBluecodeStatement(ResolvedNodeName)
		};

		const TSharedPtr<FJsonObject>* AliasObj = nullptr;
		if (!Params->TryGetObjectField(TEXT("node_aliases"), AliasObj) || !AliasObj || !AliasObj->IsValid())
		{
			Params->TryGetObjectField(TEXT("aliases"), AliasObj);
		}
		if (!AliasObj || !AliasObj->IsValid())
		{
			return;
		}
		for (const FString& Key : Keys)
		{
			FString Alias;
			if ((*AliasObj)->TryGetStringField(Key, Alias) && !Alias.IsEmpty())
			{
				InOutAlias = Alias;
				return;
			}
		}
	}

	bool TryParseBluecodeBranch(const FString& Statement, FString& OutCondition)
	{
		if (!Statement.StartsWith(TEXT("if "), ESearchCase::IgnoreCase))
		{
			return false;
		}

		OutCondition = Statement.Mid(3);
		OutCondition.TrimStartAndEndInline();
		OutCondition.RemoveFromEnd(TEXT(":"));
		OutCondition.TrimStartAndEndInline();
		return !OutCondition.IsEmpty();
	}

	void CopyBluecodeHintContext(
		const TSharedPtr<FJsonObject>& Source,
		const TSharedPtr<FJsonObject>& Target)
	{
		if (!Source.IsValid() || !Target.IsValid())
		{
			return;
		}

		const TCHAR* ObjectFields[] = {
			TEXT("action_handles"),
			TEXT("actionHints"),
			TEXT("action_hints"),
			TEXT("expressionHints"),
			TEXT("expression_hints"),
			TEXT("node_properties"),
			TEXT("member_classes"),
			TEXT("memberClasses")
		};
		for (const TCHAR* Field : ObjectFields)
		{
			const TSharedPtr<FJsonObject>* Obj = nullptr;
			if (Source->TryGetObjectField(Field, Obj) && Obj && Obj->IsValid())
			{
				Target->SetObjectField(Field, *Obj);
			}
		}

		const TCHAR* ArrayFields[] = {
			TEXT("action_hints_by_line"),
			TEXT("actionHintsByLine"),
			TEXT("action_hints_sequence")
		};
		for (const TCHAR* Field : ArrayFields)
		{
			const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
			if (Source->TryGetArrayField(Field, Values) && Values)
			{
				Target->SetArrayField(Field, *Values);
			}
		}
	}

	void ApplyBluecodeActionHints(
		const FString& Statement,
		const FString& CallName,
		const FString& ResolvedNodeName,
		const TSharedPtr<FJsonObject>& Params,
		const TSharedPtr<FJsonObject>& Payload,
		const int32 StatementOrdinal = INDEX_NONE,
		TSet<int32>* UsedLineHintIndices = nullptr)
	{
		if (!Params.IsValid() || !Payload.IsValid())
		{
			return;
		}

		const TArray<FString> Keys = {
			Statement,
			NormalizeBluecodeStatement(Statement),
			CallName,
			NormalizeBluecodeStatement(CallName),
			ResolvedNodeName,
			NormalizeBluecodeStatement(ResolvedNodeName)
		};

		auto ApplyHintObject = [&](const TSharedPtr<FJsonObject>& HintObj)
		{
			if (!HintObj.IsValid())
			{
				return false;
			}

			TSharedPtr<FJsonObject> EffectiveHint = HintObj;
			const TSharedPtr<FJsonObject>* WrappedHint = nullptr;
			if (HintObj->TryGetObjectField(TEXT("hint"), WrappedHint) && WrappedHint && WrappedHint->IsValid())
			{
				EffectiveHint = *WrappedHint;
			}

			const TArray<TSharedPtr<FJsonValue>>* Actions = nullptr;
			if (EffectiveHint->TryGetArrayField(TEXT("actions"), Actions) &&
				Actions &&
				Actions->Num() == 1 &&
				(*Actions)[0].IsValid() &&
				(*Actions)[0]->Type == EJson::Object)
			{
				EffectiveHint = (*Actions)[0]->AsObject();
			}

			const TCHAR* HandleFields[] = {
				TEXT("action_handle"),
				TEXT("handle"),
				TEXT("actionHandle"),
				TEXT("node_handle"),
				TEXT("nodeHandle")
			};
			for (const TCHAR* Field : HandleFields)
			{
				FString Value;
				if (EffectiveHint->TryGetStringField(Field, Value) && !Value.IsEmpty())
				{
					Payload->SetStringField(TEXT("action_handle"), Value);
					break;
				}
			}

			const TCHAR* NodeIdFields[] = {
				TEXT("target_node_id"),
				TEXT("node_id"),
				TEXT("nodeId"),
				TEXT("id")
			};
			for (const TCHAR* Field : NodeIdFields)
			{
				FString Value;
				if (EffectiveHint->TryGetStringField(Field, Value) && !Value.IsEmpty())
				{
					Payload->SetStringField(TEXT("target_node_id"), Value);
					break;
				}
			}

			FString Category;
			if (EffectiveHint->TryGetStringField(TEXT("category"), Category) && !Category.IsEmpty())
			{
				Payload->SetStringField(TEXT("category"), Category);
			}

			FString NodeClass;
			if (EffectiveHint->TryGetStringField(TEXT("node_class"), NodeClass) && !NodeClass.IsEmpty())
			{
				Payload->SetStringField(TEXT("node_class"), NodeClass);
			}
			else if (EffectiveHint->TryGetStringField(TEXT("nodeClass"), NodeClass) && !NodeClass.IsEmpty())
			{
				Payload->SetStringField(TEXT("node_class"), NodeClass);
			}

			FString NodeClassPath;
			if (EffectiveHint->TryGetStringField(TEXT("node_class_path"), NodeClassPath) && !NodeClassPath.IsEmpty())
			{
				Payload->SetStringField(TEXT("node_class_path"), NodeClassPath);
			}
			else if (EffectiveHint->TryGetStringField(TEXT("nodeClassPath"), NodeClassPath) && !NodeClassPath.IsEmpty())
			{
				Payload->SetStringField(TEXT("node_class_path"), NodeClassPath);
			}

			FString Signature;
			if (EffectiveHint->TryGetStringField(TEXT("signature"), Signature) && !Signature.IsEmpty())
			{
				Payload->SetStringField(TEXT("signature"), Signature);
			}
			else if (EffectiveHint->TryGetStringField(TEXT("spawner_signature"), Signature) && !Signature.IsEmpty())
			{
				Payload->SetStringField(TEXT("signature"), Signature);
			}

			FString MemberClass;
			if (EffectiveHint->TryGetStringField(TEXT("member_class"), MemberClass) && !MemberClass.IsEmpty())
			{
				Payload->SetStringField(TEXT("memberClass"), MemberClass);
			}
			else if (EffectiveHint->TryGetStringField(TEXT("memberClass"), MemberClass) && !MemberClass.IsEmpty())
			{
				Payload->SetStringField(TEXT("memberClass"), MemberClass);
			}
			bool bContextSensitive = true;
			if (EffectiveHint->TryGetBoolField(TEXT("context_sensitive"), bContextSensitive))
			{
				Payload->SetBoolField(TEXT("context_sensitive"), bContextSensitive);
			}
			else if (EffectiveHint->TryGetBoolField(TEXT("contextSensitive"), bContextSensitive))
			{
				Payload->SetBoolField(TEXT("context_sensitive"), bContextSensitive);
			}

			bool bUseActionMenu = true;
			if (EffectiveHint->TryGetBoolField(TEXT("useActionMenu"), bUseActionMenu))
			{
				Payload->SetBoolField(TEXT("useActionMenu"), bUseActionMenu);
			}
			else if (EffectiveHint->TryGetBoolField(TEXT("use_action_menu"), bUseActionMenu))
			{
				Payload->SetBoolField(TEXT("useActionMenu"), bUseActionMenu);
			}

			const TSharedPtr<FJsonObject>* InputWires = nullptr;
			if (EffectiveHint->TryGetObjectField(TEXT("inputWires"), InputWires) && InputWires && InputWires->IsValid())
			{
				Payload->SetObjectField(TEXT("inputWires"), *InputWires);
			}
			else if (EffectiveHint->TryGetObjectField(TEXT("input_wires"), InputWires) && InputWires && InputWires->IsValid())
			{
				Payload->SetObjectField(TEXT("inputWires"), *InputWires);
			}

			const TArray<TSharedPtr<FJsonValue>>* ExtraArgs = nullptr;
			if (EffectiveHint->TryGetArrayField(TEXT("extraArgs"), ExtraArgs) && ExtraArgs)
			{
				Payload->SetArrayField(TEXT("extraArgs"), *ExtraArgs);
			}
			else if (EffectiveHint->TryGetArrayField(TEXT("extra_args"), ExtraArgs) && ExtraArgs)
			{
				Payload->SetArrayField(TEXT("extraArgs"), *ExtraArgs);
			}

			const TSharedPtr<FJsonObject>* NodeProperties = nullptr;
			if (EffectiveHint->TryGetObjectField(TEXT("node_properties"), NodeProperties) && NodeProperties && NodeProperties->IsValid())
			{
				Payload->SetObjectField(TEXT("nodeProperties"), *NodeProperties);
			}
			else if (EffectiveHint->TryGetObjectField(TEXT("nodeProperties"), NodeProperties) && NodeProperties && NodeProperties->IsValid())
			{
				Payload->SetObjectField(TEXT("nodeProperties"), *NodeProperties);
			}
			else if (EffectiveHint->TryGetObjectField(TEXT("properties"), NodeProperties) && NodeProperties && NodeProperties->IsValid())
			{
				Payload->SetObjectField(TEXT("nodeProperties"), *NodeProperties);
			}
			return true;
		};

		auto TryApplyOrderedLineHint = [&]()
		{
			if (StatementOrdinal == INDEX_NONE)
			{
				return false;
			}

			const TArray<TSharedPtr<FJsonValue>>* LineHints = nullptr;
			if ((!Params->TryGetArrayField(TEXT("action_hints_by_line"), LineHints) || !LineHints) &&
				(!Params->TryGetArrayField(TEXT("actionHintsByLine"), LineHints) || !LineHints) &&
				(!Params->TryGetArrayField(TEXT("action_hints_sequence"), LineHints) || !LineHints))
			{
				return false;
			}

			auto TryApplyLineHint = [&](const int32 Index, const bool bRequireStatementMatch)
			{
				if (UsedLineHintIndices && UsedLineHintIndices->Contains(Index))
				{
					return false;
				}
				if (!LineHints || !LineHints->IsValidIndex(Index) || !(*LineHints)[Index].IsValid() || (*LineHints)[Index]->Type != EJson::Object)
				{
					return false;
				}

				TSharedPtr<FJsonObject> LineHint = (*LineHints)[Index]->AsObject();
				if (!LineHint.IsValid())
				{
					return false;
				}

				FString HintStatement;
				if (bRequireStatementMatch &&
					LineHint->TryGetStringField(TEXT("statement"), HintStatement) &&
					!HintStatement.IsEmpty() &&
					NormalizeBluecodeStatement(HintStatement) != NormalizeBluecodeStatement(Statement))
				{
					return false;
				}

				const bool bApplied = ApplyHintObject(LineHint);
				if (bApplied && UsedLineHintIndices)
				{
					UsedLineHintIndices->Add(Index);
				}
				return bApplied;
			};

			if (TryApplyLineHint(StatementOrdinal, false))
			{
				return true;
			}

			for (int32 Index = 0; LineHints && Index < LineHints->Num(); ++Index)
			{
				if (Index != StatementOrdinal && TryApplyLineHint(Index, true))
				{
					return true;
				}
			}
			return false;
		};

		auto TryApplySourceMap = [&]()
		{
			if (StatementOrdinal == INDEX_NONE)
			{
				return false;
			}
			const TSharedPtr<FJsonObject>* SourceMap = nullptr;
			if (!Params->TryGetObjectField(TEXT("source_map"), SourceMap) || !SourceMap || !SourceMap->IsValid())
			{
				return false;
			}

			auto IsCompatibleSource = [&](const TSharedPtr<FJsonObject>& Source)
			{
				FString SemanticKey;
				if (!Source.IsValid() || !Source->TryGetStringField(TEXT("semantic_key"), SemanticKey) || SemanticKey.IsEmpty())
				{
					return false;
				}
				return IsBluecodeSemanticKeyCompatible(SemanticKey, CallName, ResolvedNodeName);
			};

			FString PatchCode;
			Params->TryGetStringField(TEXT("code"), PatchCode);
			const bool bShapePreserved = ParseBluecodeStatements(PatchCode).Num() == (*SourceMap)->Values.Num();
			if (bShapePreserved)
			{
				const FString Path = FString::Printf(TEXT("/body/%d"), StatementOrdinal);
				const TSharedPtr<FJsonObject>* Source = nullptr;
				if ((*SourceMap)->TryGetObjectField(Path, Source) && Source && Source->IsValid() && IsCompatibleSource(*Source))
				{
					return ApplyHintObject(*Source);
				}
			}

			// If lines were inserted or removed, do not trust ordinal paths. Exact
			// unchanged statements can still retain their source identity safely.
			for (const auto& Pair : (*SourceMap)->Values)
			{
				if (!Pair.Value.IsValid() || Pair.Value->Type != EJson::Object)
				{
					continue;
				}
				const TSharedPtr<FJsonObject> Source = Pair.Value->AsObject();
				FString SourceStatement;
				if (Source.IsValid() &&
					IsCompatibleSource(Source) &&
					Source->TryGetStringField(TEXT("statement"), SourceStatement) &&
					NormalizeBluecodeStatement(SourceStatement) == NormalizeBluecodeStatement(Statement))
				{
					return ApplyHintObject(Source);
				}
			}
			return false;
		};

		const TSharedPtr<FJsonObject>* ProvidedSourceMap = nullptr;
		const bool bHasSourceMap =
			Params->TryGetObjectField(TEXT("source_map"), ProvidedSourceMap) &&
			ProvidedSourceMap &&
			ProvidedSourceMap->IsValid();
		if (TryApplySourceMap() || (!bHasSourceMap && TryApplyOrderedLineHint()))
		{
			return;
		}

		bool bAppliedHint = false;

		const TSharedPtr<FJsonObject>* HandlesObj = nullptr;
		if (Params->TryGetObjectField(TEXT("action_handles"), HandlesObj) && HandlesObj && HandlesObj->IsValid())
		{
			for (const FString& Key : Keys)
			{
				FString Handle;
				if ((*HandlesObj)->TryGetStringField(Key, Handle) && !Handle.IsEmpty())
				{
					Payload->SetStringField(TEXT("action_handle"), Handle);
					bAppliedHint = true;
					break;
				}
			}
		}

		const TSharedPtr<FJsonObject>* HintsObj = nullptr;
		if (Params->TryGetObjectField(TEXT("action_hints"), HintsObj) && HintsObj && HintsObj->IsValid())
		{
			for (const FString& Key : Keys)
			{
				FString StringHint;
				if ((*HintsObj)->TryGetStringField(Key, StringHint) && !StringHint.IsEmpty())
				{
					Payload->SetStringField(TEXT("action_handle"), StringHint);
					bAppliedHint = true;
					break;
				}

				const TSharedPtr<FJsonObject>* HintObj = nullptr;
				if ((*HintsObj)->TryGetObjectField(Key, HintObj) && HintObj && HintObj->IsValid())
				{
					bAppliedHint = ApplyHintObject(*HintObj);
					break;
				}
			}
		}

		const TSharedPtr<FJsonObject>* ExpressionHintsObj = nullptr;
		if (!bAppliedHint &&
			((Params->TryGetObjectField(TEXT("expression_hints"), ExpressionHintsObj) && ExpressionHintsObj && ExpressionHintsObj->IsValid()) ||
			 (Params->TryGetObjectField(TEXT("expressionHints"), ExpressionHintsObj) && ExpressionHintsObj && ExpressionHintsObj->IsValid())))
		{
			for (const FString& Key : Keys)
			{
				FString StringHint;
				if ((*ExpressionHintsObj)->TryGetStringField(Key, StringHint) && !StringHint.IsEmpty())
				{
					Payload->SetStringField(TEXT("action_handle"), StringHint);
					bAppliedHint = true;
					break;
				}

				const TSharedPtr<FJsonObject>* HintObj = nullptr;
				if ((*ExpressionHintsObj)->TryGetObjectField(Key, HintObj) && HintObj && HintObj->IsValid())
				{
					bAppliedHint = ApplyHintObject(*HintObj);
					break;
				}
			}
		}

		auto TryApplyNodePropertiesMap = [&](const TCHAR* FieldName)
		{
			const TSharedPtr<FJsonObject>* PropertiesMap = nullptr;
			if (!Params->TryGetObjectField(FieldName, PropertiesMap) || !PropertiesMap || !PropertiesMap->IsValid())
			{
				return false;
			}
			for (const FString& Key : Keys)
			{
				const TSharedPtr<FJsonObject>* PropertiesObj = nullptr;
				if ((*PropertiesMap)->TryGetObjectField(Key, PropertiesObj) && PropertiesObj && PropertiesObj->IsValid())
				{
					Payload->SetObjectField(TEXT("nodeProperties"), *PropertiesObj);
					return true;
				}
			}
			return false;
		};
		TryApplyNodePropertiesMap(TEXT("node_properties")) || TryApplyNodePropertiesMap(TEXT("nodeProperties"));
	}

	bool TryGetBluecodeMemberClassHint(
		const TSharedPtr<FJsonObject>& Params,
		const TArray<FString>& CandidateKeys,
		FString& OutMemberClass)
	{
		if (!Params.IsValid())
		{
			return false;
		}

		const TSharedPtr<FJsonObject>* MemberClasses = nullptr;
		if ((!Params->TryGetObjectField(TEXT("member_classes"), MemberClasses) || !MemberClasses || !MemberClasses->IsValid()) &&
			(!Params->TryGetObjectField(TEXT("memberClasses"), MemberClasses) || !MemberClasses || !MemberClasses->IsValid()))
		{
			return false;
		}

		for (const FString& CandidateKey : CandidateKeys)
		{
			if (CandidateKey.IsEmpty())
			{
				continue;
			}

			FString Value;
			if ((*MemberClasses)->TryGetStringField(CandidateKey, Value) && !Value.IsEmpty())
			{
				OutMemberClass = Value;
				return true;
			}
		}

		for (const auto& Pair : (*MemberClasses)->Values)
		{
			const FString MapKey = UmgMcpJsonCompat::KeyToString(Pair.Key);
			const FString NormalizedMapKey = NormalizeBluecodeStatement(MapKey);
			for (const FString& CandidateKey : CandidateKeys)
			{
				if (!CandidateKey.IsEmpty() && NormalizedMapKey == NormalizeBluecodeStatement(CandidateKey))
				{
					OutMemberClass = BluecodeJsonValueToString(Pair.Value);
					return !OutMemberClass.IsEmpty();
				}
			}
		}

		return false;
	}

	void ResolveBluecodeCallName(const FString& CallName, const TSharedPtr<FJsonObject>& Params, FString& OutNodeName, FString& OutMemberClass)
	{
		OutNodeName = CallName;
		OutMemberClass.Reset();

		FString ClassAlias;
		FString MemberName;
		if (CallName.Split(TEXT("."), &ClassAlias, &MemberName, ESearchCase::CaseSensitive, ESearchDir::FromEnd))
		{
			OutNodeName = MemberName;
			if (TryGetBluecodeMemberClassHint(Params, {CallName, MemberName, ClassAlias}, OutMemberClass))
			{
				return;
			}

			if (ClassAlias.Equals(TEXT("WidgetSwitcher"), ESearchCase::IgnoreCase))
			{
				OutMemberClass = TEXT("/Script/UMG.WidgetSwitcher");
			}
			return;
		}

		if (TryGetBluecodeMemberClassHint(Params, {CallName}, OutMemberClass))
		{
			return;
		}
	}

	void BuildBlueprintActionMenu(UBlueprint* Blueprint, UEdGraph* Graph, const bool bContextSensitive, FBlueprintActionMenuBuilder& Menu)
	{
		FBlueprintActionContext Context;
		if (Blueprint)
		{
			Context.Blueprints.Add(Blueprint);
		}
		if (Graph)
		{
			Context.Graphs.Add(Graph);
		}

		const uint32 TargetMask =
			EContextTargetFlags::TARGET_Blueprint |
			EContextTargetFlags::TARGET_SubComponents |
			EContextTargetFlags::TARGET_NodeTarget |
			EContextTargetFlags::TARGET_PinObject |
			EContextTargetFlags::TARGET_SiblingPinObjects |
			EContextTargetFlags::TARGET_BlueprintLibraries |
			EContextTargetFlags::TARGET_NonImportedTypes;

		FBlueprintActionMenuUtils::MakeContextMenu(Context, bContextSensitive, TargetMask, Menu);
		while (Menu.GetNumPendingActions() > 0)
		{
			if (!Menu.ProcessPendingActions())
			{
				break;
			}
		}
	}

	TSharedPtr<FJsonObject> ActionToJson(const TSharedPtr<FEdGraphSchemaAction>& Action, UEdGraph* TargetGraph, const bool bIncludePins)
	{
		TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
		if (!Action.IsValid())
		{
			return Obj;
		}

		const FString ActionHandle = MakeActionHandle(Action);
		Obj->SetStringField(TEXT("handle"), ActionHandle);
		Obj->SetStringField(TEXT("action_handle"), ActionHandle);
		Obj->SetStringField(TEXT("name"), Action->GetMenuDescription().ToString());
		Obj->SetStringField(TEXT("category"), Action->GetCategory().ToString());
		Obj->SetStringField(TEXT("tooltip"), Action->GetTooltipDescription().ToString());
		Obj->SetStringField(TEXT("keywords"), Action->GetKeywords().ToString());
		Obj->SetStringField(TEXT("search_text"), Action->GetFullSearchText());

		if (Action->GetTypeId() == FBlueprintActionMenuItem::StaticGetTypeId())
		{
			const TSharedPtr<FBlueprintActionMenuItem> MenuItem = StaticCastSharedPtr<FBlueprintActionMenuItem>(Action);
			if (MenuItem.IsValid())
			{
				const UBlueprintNodeSpawner* Spawner = MenuItem->GetRawAction();
				if (Spawner)
				{
					Obj->SetStringField(TEXT("node_class"), Spawner->NodeClass ? Spawner->NodeClass->GetName() : TEXT(""));
					Obj->SetStringField(TEXT("node_class_path"), Spawner->NodeClass ? Spawner->NodeClass->GetPathName() : TEXT(""));
					Obj->SetStringField(TEXT("signature"), Spawner->GetSpawnerSignature().ToString());
					if (UEdGraphNode* TemplateNode = Spawner->GetTemplateNode(TargetGraph))
					{
						const bool bIsExec = NodeHasExecPin(TemplateNode);
						Obj->SetBoolField(TEXT("isExec"), bIsExec);
						Obj->SetBoolField(TEXT("is_exec"), bIsExec);
						Obj->SetObjectField(TEXT("pin_counts"), MakePinCounts(TemplateNode));
						if (bIncludePins)
						{
							Obj->SetObjectField(TEXT("template"), TemplateNodeToJson(TemplateNode));
						}
					}
				}
			}
		}
		return Obj;
	}

	TSharedPtr<FJsonObject> MakeBluecodeRoundtripHint(UEdGraph* TargetGraph, const UEdGraphNode* Node, const FString& Statement)
	{
		TSharedPtr<FJsonObject> Hint = MakeShared<FJsonObject>();
		if (!Node)
		{
			return Hint;
		}

		const FString Title = BluecodeNodeTitle(Node);
		Hint->SetStringField(TEXT("statement"), Statement);
		Hint->SetStringField(TEXT("node_id"), Node->NodeGuid.ToString());
		Hint->SetStringField(TEXT("title"), Title);
		Hint->SetStringField(TEXT("node_class"), Node->GetClass() ? Node->GetClass()->GetName() : TEXT(""));
		Hint->SetStringField(TEXT("node_class_path"), Node->GetClass() ? Node->GetClass()->GetPathName() : TEXT(""));
		Hint->SetBoolField(TEXT("useActionMenu"), true);
		AddBluecodePinHintArrays(Node, Hint);

		FString Query = Title;
		if (const UK2Node_CallFunction* CallNode = Cast<UK2Node_CallFunction>(Node))
		{
			const FString FunctionName = CallNode->FunctionReference.GetMemberName().ToString();
			if (!FunctionName.IsEmpty())
			{
				Query = FunctionName;
				Hint->SetStringField(TEXT("function_name"), FunctionName);
			}
		}

		FBlueprintActionMenuBuilder Menu;
		BuildBlueprintActionMenu(ResolveBlueprintFromGraph(TargetGraph), TargetGraph, true, Menu);

		struct FHintMatch
		{
			TSharedPtr<FEdGraphSchemaAction> Action;
			int32 Score = 0;
		};

		TArray<FHintMatch> Matches;
		const FString NodeClass = Node->GetClass() ? Node->GetClass()->GetName() : TEXT("");
		const FString NormalizedQuery = NormalizeBluecodeStatement(Query);
		const FString NormalizedTitle = NormalizeBluecodeStatement(Title);

		for (int32 Index = 0; Index < Menu.GetNumActions(); ++Index)
		{
			TSharedPtr<FEdGraphSchemaAction> Action = Menu.GetSchemaAction(Index);
			if (!Action.IsValid() || Action->GetTypeId() != FBlueprintActionMenuItem::StaticGetTypeId())
			{
				continue;
			}

			const TSharedPtr<FBlueprintActionMenuItem> MenuItem = StaticCastSharedPtr<FBlueprintActionMenuItem>(Action);
			const UBlueprintNodeSpawner* Spawner = MenuItem.IsValid() ? MenuItem->GetRawAction() : nullptr;
			const FString ActionNodeClass = (Spawner && Spawner->NodeClass) ? Spawner->NodeClass->GetName() : TEXT("");
			const FString Name = Action->GetMenuDescription().ToString();
			const FString SearchText = Action->GetFullSearchText();
			const FString Signature = Spawner ? Spawner->GetSpawnerSignature().ToString() : TEXT("");
			const FString NormalizedName = NormalizeBluecodeStatement(Name);
			const FString NormalizedSearch = NormalizeBluecodeStatement(SearchText);
			const FString NormalizedSignature = NormalizeBluecodeStatement(Signature);

			int32 Score = 0;
			if (!NodeClass.IsEmpty() && ActionNodeClass.Equals(NodeClass, ESearchCase::IgnoreCase))
			{
				Score += 500;
			}
			if (!NormalizedQuery.IsEmpty() &&
				(NormalizedName == NormalizedQuery ||
				 NormalizedSearch.Contains(NormalizedQuery) ||
				 NormalizedSignature.Contains(NormalizedQuery)))
			{
				Score += 300;
			}
			if (!NormalizedTitle.IsEmpty() &&
				(NormalizedName == NormalizedTitle || NormalizedSearch.Contains(NormalizedTitle)))
			{
				Score += 200;
			}
			if (Score > 0)
			{
				Matches.Add({Action, Score});
			}
		}

		if (Matches.Num() > 0)
		{
			Matches.Sort([](const FHintMatch& A, const FHintMatch& B) { return A.Score > B.Score; });
			const int32 BestScore = Matches[0].Score;
			int32 BestCount = 0;
			for (const FHintMatch& Match : Matches)
			{
				if (Match.Score == BestScore)
				{
					++BestCount;
				}
			}

			if (BestCount == 1)
			{
				TSharedPtr<FJsonObject> ActionObj = ActionToJson(Matches[0].Action, TargetGraph, false);
				FString Handle;
				if (ActionObj.IsValid() && ActionObj->TryGetStringField(TEXT("handle"), Handle) && !Handle.IsEmpty())
				{
					Hint->SetStringField(TEXT("handle"), Handle);
					Hint->SetStringField(TEXT("action_handle"), Handle);
				}
				FString Category;
				if (ActionObj.IsValid() && ActionObj->TryGetStringField(TEXT("category"), Category) && !Category.IsEmpty())
				{
					Hint->SetStringField(TEXT("category"), Category);
				}
				FString ActionNodeClass;
				if (ActionObj.IsValid() && ActionObj->TryGetStringField(TEXT("node_class"), ActionNodeClass) && !ActionNodeClass.IsEmpty())
				{
					Hint->SetStringField(TEXT("node_class"), ActionNodeClass);
				}
				FString ActionNodeClassPath;
				if (ActionObj.IsValid() && ActionObj->TryGetStringField(TEXT("node_class_path"), ActionNodeClassPath) && !ActionNodeClassPath.IsEmpty())
				{
					Hint->SetStringField(TEXT("node_class_path"), ActionNodeClassPath);
				}
				FString Signature;
				if (ActionObj.IsValid() && ActionObj->TryGetStringField(TEXT("signature"), Signature) && !Signature.IsEmpty())
				{
					Hint->SetStringField(TEXT("signature"), Signature);
				}
			}
			else
			{
				Hint->SetBoolField(TEXT("ambiguous"), true);
				Hint->SetNumberField(TEXT("candidate_count"), BestCount);
			}
		}

		return Hint;
	}

	bool TrySpawnBlueprintActionNode(
		UEdGraph* TargetGraph,
		const TSharedPtr<FJsonObject>& Payload,
		const bool bAllowNameFallback,
		UEdGraphNode*& OutNode,
		TSharedPtr<FJsonObject>& OutError)
	{
		OutNode = nullptr;
		OutError.Reset();

		FString Handle;
		Payload->TryGetStringField(TEXT("action_handle"), Handle);
		if (Handle.IsEmpty()) Payload->TryGetStringField(TEXT("actionHandle"), Handle);
		if (Handle.IsEmpty()) Payload->TryGetStringField(TEXT("handle"), Handle);
		if (Handle.IsEmpty()) Payload->TryGetStringField(TEXT("nodeHandle"), Handle);
		if (Handle.IsEmpty()) Payload->TryGetStringField(TEXT("node_handle"), Handle);

		FString Query;
		Payload->TryGetStringField(TEXT("nodeName"), Query);
		if (Query.IsEmpty()) Payload->TryGetStringField(TEXT("memberName"), Query);
		if (Query.IsEmpty()) Payload->TryGetStringField(TEXT("nodeType"), Query);
		if (Query.IsEmpty()) Payload->TryGetStringField(TEXT("name"), Query);

		FString Category;
		Payload->TryGetStringField(TEXT("category"), Category);
		FString NodeClass;
		Payload->TryGetStringField(TEXT("nodeClass"), NodeClass);
		if (NodeClass.IsEmpty()) Payload->TryGetStringField(TEXT("node_class"), NodeClass);
		FString NodeClassPath;
		Payload->TryGetStringField(TEXT("nodeClassPath"), NodeClassPath);
		if (NodeClassPath.IsEmpty()) Payload->TryGetStringField(TEXT("node_class_path"), NodeClassPath);

		FString Signature;
		Payload->TryGetStringField(TEXT("signature"), Signature);
		if (Signature.IsEmpty()) Payload->TryGetStringField(TEXT("spawner_signature"), Signature);

		const bool bHasExplicitActionRequest =
			!Handle.IsEmpty() ||
			Payload->HasField(TEXT("useActionMenu")) ||
			Payload->HasField(TEXT("handle")) ||
			Payload->HasField(TEXT("category")) ||
			Payload->HasField(TEXT("signature")) ||
			Payload->HasField(TEXT("spawner_signature")) ||
			Payload->HasField(TEXT("nodeClass")) ||
			Payload->HasField(TEXT("node_class")) ||
			Payload->HasField(TEXT("nodeClassPath")) ||
			Payload->HasField(TEXT("node_class_path"));
		if (!bHasExplicitActionRequest && !bAllowNameFallback)
		{
			return false;
		}
		if (Handle.IsEmpty() && Query.IsEmpty() && Signature.IsEmpty())
		{
			return false;
		}

		bool bContextSensitive = true;
		Payload->TryGetBoolField(TEXT("context_sensitive"), bContextSensitive);

		FBlueprintActionMenuBuilder Menu;
		BuildBlueprintActionMenu(ResolveBlueprintFromGraph(TargetGraph), TargetGraph, bContextSensitive, Menu);

		struct FMatch
		{
			TSharedPtr<FEdGraphSchemaAction> Action;
			int32 Score = 0;
			bool bExactHandle = false;
		};

		TArray<FMatch> Matches;
		for (int32 Index = 0; Index < Menu.GetNumActions(); ++Index)
		{
			TSharedPtr<FEdGraphSchemaAction> Action = Menu.GetSchemaAction(Index);
			if (!Action.IsValid() || Action->GetTypeId() != FBlueprintActionMenuItem::StaticGetTypeId())
			{
				continue;
			}

			const FString ActionHandle = MakeActionHandle(Action);
			const FString Name = Action->GetMenuDescription().ToString();
			const FString ActionCategory = Action->GetCategory().ToString();
			const FString SearchText = Action->GetFullSearchText();

			const TSharedPtr<FBlueprintActionMenuItem> MenuItem = StaticCastSharedPtr<FBlueprintActionMenuItem>(Action);
			const UBlueprintNodeSpawner* Spawner = MenuItem.IsValid() ? MenuItem->GetRawAction() : nullptr;
			const FString ActionNodeClass = (Spawner && Spawner->NodeClass) ? Spawner->NodeClass->GetName() : TEXT("");
			const FString ActionNodeClassPath = (Spawner && Spawner->NodeClass) ? Spawner->NodeClass->GetPathName() : TEXT("");
			const FString ActionSignature = Spawner ? Spawner->GetSpawnerSignature().ToString() : TEXT("");

			if (!Handle.IsEmpty())
			{
				if (ActionHandle == Handle)
				{
					Matches.Add({Action, 2000, true});
					continue;
				}
				const bool bHasFallbackEvidence = !Signature.IsEmpty() || !Category.IsEmpty() || !NodeClass.IsEmpty() || !NodeClassPath.IsEmpty();
				if (!bHasFallbackEvidence)
				{
					continue;
				}
			}

			if (!Category.IsEmpty() && !ActionCategory.Contains(Category, ESearchCase::IgnoreCase))
			{
				continue;
			}
			if (!NodeClass.IsEmpty() && !ActionNodeClass.Contains(NodeClass, ESearchCase::IgnoreCase))
			{
				continue;
			}
			if (!NodeClassPath.IsEmpty() && !ActionNodeClassPath.Contains(NodeClassPath, ESearchCase::IgnoreCase))
			{
				continue;
			}
			if (!Signature.IsEmpty() && !ActionSignature.Equals(Signature, ESearchCase::IgnoreCase))
			{
				continue;
			}

			int32 Score = 0;
			if (!Signature.IsEmpty())
			{
				Score += 1500;
			}
			if (Name.Equals(Query, ESearchCase::IgnoreCase))
			{
				Score += 800;
			}
			else if (SearchText.Contains(Query, ESearchCase::IgnoreCase))
			{
				Score += 300;
			}
			else if (ActionCategory.Contains(Query, ESearchCase::IgnoreCase))
			{
				Score += 150;
			}

			if (Score > 0)
			{
				Matches.Add({Action, Score, false});
			}
		}

		if (Matches.Num() == 0)
		{
			if (bHasExplicitActionRequest)
			{
				OutError = MakeShared<FJsonObject>();
				OutError->SetBoolField(TEXT("success"), false);
				OutError->SetStringField(TEXT("error"), FString::Printf(TEXT("No Blueprint action matched '%s'. Run bluecode_search_nodes first and pass action_handle."), *Query));
			}
			return false;
		}

		Matches.Sort([](const FMatch& A, const FMatch& B) { return A.Score > B.Score; });
		const int32 BestScore = Matches[0].Score;
		int32 BestCount = 0;
		for (const FMatch& Match : Matches)
		{
			if (Match.Score == BestScore)
			{
				++BestCount;
			}
		}

		if (BestCount > 1)
		{
			OutError = MakeShared<FJsonObject>();
			OutError->SetBoolField(TEXT("success"), false);
			OutError->SetStringField(TEXT("error"), FString::Printf(
				TEXT("Blueprint action '%s' is ambiguous or the supplied action_handle no longer uniquely resolves. Run bluecode_search_nodes and pass one exact handle or full action_hints."),
				*Query));
			TArray<TSharedPtr<FJsonValue>> Candidates;
			for (int32 i = 0; i < FMath::Min(10, Matches.Num()); ++i)
			{
				Candidates.Add(MakeShared<FJsonValueObject>(ActionToJson(Matches[i].Action, TargetGraph, false)));
			}
			OutError->SetArrayField(TEXT("candidates"), Candidates);
			return false;
		}

		const TSharedPtr<FBlueprintActionMenuItem> BestMenuItem = StaticCastSharedPtr<FBlueprintActionMenuItem>(Matches[0].Action);
		if (!BestMenuItem.IsValid())
		{
			return false;
		}

		double X = 0.0;
		double Y = 0.0;
		Payload->TryGetNumberField(TEXT("x"), X);
		Payload->TryGetNumberField(TEXT("y"), Y);

		OutNode = BestMenuItem->PerformAction(TargetGraph, nullptr, FVector2f(static_cast<float>(X), static_cast<float>(Y)), false);
		if (OutNode)
		{
			return true;
		}

		OutError = MakeShared<FJsonObject>();
		OutError->SetBoolField(TEXT("success"), false);
		OutError->SetStringField(TEXT("error"), TEXT("Matched Blueprint action failed to spawn a node."));
		return false;
	}
}
#endif

void UUmgBlueprintFunctionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	UE_LOG(LogUmgBlueprint, Log, TEXT("UmgBlueprintFunctionSubsystem Initialized."));
}

void UUmgBlueprintFunctionSubsystem::Deinitialize()
{
	UE_LOG(LogUmgBlueprint, Log, TEXT("UmgBlueprintFunctionSubsystem Deinitialized."));
	Super::Deinitialize();
}

FString UUmgBlueprintFunctionSubsystem::HandleBlueprintGraphAction(UBlueprint* Blueprint, const FString& Action, const FString& PayloadJson)
{
#if WITH_EDITOR
	if (!Blueprint)
	{
		return TEXT("{\"success\": false, \"error\": \"Invalid Blueprint\"}");
	}

	TSharedPtr<FJsonObject> Payload;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(PayloadJson);
	if (!FJsonSerializer::Deserialize(Reader, Payload) || !Payload.IsValid())
	{
		return TEXT("{\"success\": false, \"error\": \"Invalid JSON Payload\"}");
	}

	return ExecuteGraphAction(Blueprint, Payload);
#else
	return TEXT("{\"success\": false, \"error\": \"Editor Only\"}");
#endif
}

#if WITH_EDITOR
FString UUmgBlueprintFunctionSubsystem::ExecuteGraphAction(UBlueprint* Blueprint, const TSharedPtr<FJsonObject>& Payload)
{
	FString SubAction;
	Payload->TryGetStringField(TEXT("subAction"), SubAction);

    if (SubAction == TEXT("get_events"))
    {
        TSharedPtr<FJsonObject> Result = GetEvents(Blueprint, Payload);
        if (Result.IsValid())
        {
            FString OutputString;
            TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
            FJsonSerializer::Serialize(Result.ToSharedRef(), Writer);
            return OutputString;
        }
        return TEXT("{\"success\": false, \"error\": \"Failed to read Blueprint events\"}");
    }

	// Determine target graph
	FString GraphName;
	Payload->TryGetStringField(TEXT("graphName"), GraphName);

	UEdGraph* TargetGraph = nullptr;

	if (GraphName.IsEmpty() || GraphName.Equals(TEXT("EventGraph"), ESearchCase::IgnoreCase))
	{
		if (Blueprint->UbergraphPages.Num() > 0)
		{
			TargetGraph = Blueprint->UbergraphPages[0];
		}
	}
	else
	{
		for (UEdGraph* Graph : Blueprint->FunctionGraphs)
		{
			if (Graph->GetName() == GraphName)
			{
				TargetGraph = Graph;
				break;
			}
		}
		if (!TargetGraph)
		{
			for (UEdGraph* Graph : Blueprint->UbergraphPages)
			{
				if (Graph->GetName() == GraphName)
				{
					TargetGraph = Graph;
					break;
				}
			}
		}
	}

	if (!TargetGraph)
	{
		return TEXT("{\"success\": false, \"error\": \"Graph not found\"}");
	}

	TSharedPtr<FJsonObject> Result;

	if (SubAction == TEXT("add_function_step"))
	{
		// Inject default nodeType for function steps based on Intent
		TSharedPtr<FJsonObject> NewPayload = MakeShared<FJsonObject>(*Payload);

        FString CurrentType;
        NewPayload->TryGetStringField(TEXT("nodeType"), CurrentType);

		if (CurrentType.IsEmpty())
		{
            FString Name;
            NewPayload->TryGetStringField(TEXT("nodeName"), Name);

			if (Name.Equals("Set") || Name.Equals("VariableSet") || Name.Equals("SetMemberVariable"))
            {
                NewPayload->SetStringField(TEXT("nodeType"), TEXT("Set"));
            }
            else if (Name.Equals("Get") || Name.Equals("VariableGet") || Name.Equals("GetMemberVariable"))
            {
                NewPayload->SetStringField(TEXT("nodeType"), TEXT("Get"));
            }
            else if (Name.Equals("Cast") || Name.Equals("DynamicCast"))
            {
                NewPayload->SetStringField(TEXT("nodeType"), TEXT("Cast"));
            }
            else
            {
			    NewPayload->SetStringField(TEXT("nodeType"), TEXT("CallFunction"));
            }
		}
        else
        {
             // Smart Mapping: If users pass "PrintString" as nodeType, map it to CallFunction
             if (!CurrentType.Equals("CallFunction") &&
                 !CurrentType.Equals("Set") && !CurrentType.Equals("VariableSet") &&
                 !CurrentType.Equals("Get") && !CurrentType.Equals("VariableGet") &&
                 !CurrentType.Equals("Cast") && !CurrentType.Equals("Event") &&
                 !CurrentType.Equals("CustomEvent") && !CurrentType.Equals("ComponentBoundEvent") &&
                 !CurrentType.Equals("Branch") && !CurrentType.Equals("If") &&
                 !CurrentType.Equals("Sequence"))
             {
                  NewPayload->SetStringField(TEXT("nodeName"), CurrentType);
                  NewPayload->SetStringField(TEXT("nodeType"), TEXT("CallFunction"));
             }
        }
		Result = AddNode(TargetGraph, NewPayload);
	}
	else if (SubAction == TEXT("add_node") || SubAction == TEXT("create_node"))
	{
		Result = AddNode(TargetGraph, Payload);
	}
    else if (SubAction == TEXT("add_param") || SubAction == TEXT("add_step_param"))
    {
        Result = AddParam(TargetGraph, Payload);
    }
	else if (SubAction == TEXT("connect_pins"))
	{
		Result = ConnectPins(TargetGraph, Payload);
	}
	else if (SubAction == TEXT("bluecode_connect"))
	{
		Result = ApplyBluecodeConnect(TargetGraph, Payload);
	}
    else if (SubAction == TEXT("set_node_property"))
    {
        Result = SetNodeProperty(Blueprint, TargetGraph, Payload);
    }
    else if (SubAction == TEXT("delete_node"))
    {
        Result = DeleteNode(Blueprint, TargetGraph, Payload);
    }
    else if (SubAction == TEXT("find_functions") || SubAction == TEXT("search_function_library"))
    {
        Result = FindFunctions(Payload, Blueprint);
    }
    else if (SubAction == TEXT("search_nodes") || SubAction == TEXT("bluecode_search_nodes") || SubAction == TEXT("search_blueprint_nodes"))
    {
        Result = SearchNodes(TargetGraph, Blueprint, Payload);
    }
    else if (SubAction == TEXT("bluecode_read_function"))
    {
        Result = ReadBluecodeFunction(TargetGraph, Payload);
    }
    else if (SubAction == TEXT("bluecode_apply"))
    {
        Result = ApplyBluecode(TargetGraph, Blueprint, Payload);
    }
    else if (SubAction == TEXT("bluecode_apply_variables"))
    {
        Result = ApplyBluecodeVariables(Blueprint, Payload);
    }
    else if (SubAction == TEXT("bluecode_delete"))
    {
        Result = DeleteBluecode(Blueprint, TargetGraph, Payload);
    }
    else if (SubAction == TEXT("add_variable"))
    {
        Result = AddVariable(Blueprint, Payload);
    }
    else if (SubAction == TEXT("delete_variable"))
    {
        Result = DeleteVariable(Blueprint, Payload);
    }
    else if (SubAction == TEXT("get_variables"))
    {
        Result = GetVariables(Blueprint);
    }
    else if (SubAction == TEXT("get_nodes"))
    {
        Result = GetNodes(TargetGraph);
    }

	if (Result.IsValid())
	{
        bool bResultSucceeded = false;
        Result->TryGetBoolField(TEXT("success"), bResultSucceeded);
        bool bShouldMarkBlueprintModified = bResultSucceeded;
        if (SubAction == TEXT("bluecode_apply"))
        {
            double AppliedCount = 0.0;
            bool bDryRun = false;
            Result->TryGetNumberField(TEXT("applied_count"), AppliedCount);
            Result->TryGetBoolField(TEXT("dry_run"), bDryRun);
            bShouldMarkBlueprintModified = bResultSucceeded && !bDryRun && AppliedCount > 0.0;
        }
        // Refresh UI (Skip for read-only actions)
        if (bShouldMarkBlueprintModified &&
            SubAction != TEXT("find_functions") && SubAction != TEXT("search_function_library") &&
            SubAction != TEXT("search_nodes") && SubAction != TEXT("bluecode_search_nodes") && SubAction != TEXT("search_blueprint_nodes") &&
            SubAction != TEXT("get_variables") && SubAction != TEXT("get_widget_tree") &&
            SubAction != TEXT("get_nodes") &&
            SubAction != TEXT("bluecode_read_function") && SubAction != TEXT("bluecode_read_variables") && SubAction != TEXT("bluecode_read_events"))
        {
             FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        }

		FString OutputString;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
		FJsonSerializer::Serialize(Result.ToSharedRef(), Writer);
		return OutputString;
	}

	return TEXT("{\"success\": false, \"error\": \"Action Failed or Unknown\"}");
}

UClass* UUmgBlueprintFunctionSubsystem::ResolveUClass(const FString& ClassName)
{
    // Basic implementation for referencing commonly used classes
    UClass* Result = FindObject<UClass>(nullptr, *ClassName);
    if (Result) return Result;

    Result = LoadObject<UClass>(nullptr, *ClassName);
    if (Result) return Result;

    // Fallback for short names is tricky without iteration, but we can try some common ones
    if (ClassName == "KismetSystemLibrary") return UKismetSystemLibrary::StaticClass();
    if (ClassName == "GameplayStatics") return UGameplayStatics::StaticClass();
    if (ClassName == "KismetMathLibrary") return UKismetMathLibrary::StaticClass();

    return nullptr;
}


TSharedPtr<FJsonObject> UUmgBlueprintFunctionSubsystem::CreateNodeInstance(UEdGraph* TargetGraph, const TSharedPtr<FJsonObject>& Payload, UEdGraphNode*& OutNode)
{
    if (!TargetGraph || !Payload.IsValid()) return nullptr;

    FString NodeType;
    Payload->TryGetStringField(TEXT("nodeType"), NodeType);
    if (NodeType.IsEmpty()) Payload->TryGetStringField(TEXT("nodeName"), NodeType);

    const TSharedPtr<FJsonObject>* InputWiresPtr = nullptr;
    Payload->TryGetObjectField(TEXT("inputWires"), InputWiresPtr);

    const TArray<TSharedPtr<FJsonValue>>* InferenceArgs = nullptr;
    Payload->TryGetArrayField(TEXT("extraArgs"), InferenceArgs);

    UBlueprint* TargetBlueprint = ResolveBlueprintFromGraph(TargetGraph);
    TArray<TSharedPtr<FJsonValue>> InputWarnings;
    TSharedPtr<FJsonObject> NodePropertyResult;

    // Helper Setup Lambda
    auto SetupNode = [&](UEdGraphNode* Node) {
        if (!Node) return;
        int32 ArgIndex = 0;

        auto AddInputWarning = [&](const UEdGraphPin* InPin, const FString& Value, const FString& Message)
        {
            TSharedPtr<FJsonObject> Warning = MakeShared<FJsonObject>();
            Warning->SetStringField(TEXT("pin"), InPin ? InPin->PinName.ToString() : TEXT(""));
            Warning->SetStringField(TEXT("value"), Value);
            Warning->SetStringField(TEXT("message"), Message);
            InputWarnings.Add(MakeShared<FJsonValueObject>(Warning));
        };

        NodePropertyResult = ApplyBluecodeNodeProperties(Node, Payload);

        FString MemberClassHint;
        Payload->TryGetStringField(TEXT("memberClass"), MemberClassHint);
        auto HasExplicitSelfInput = [&]() -> bool
        {
            if (!InputWiresPtr || !(*InputWiresPtr).IsValid())
            {
                return false;
            }
            for (const auto& Pair : (*InputWiresPtr)->Values)
            {
                if (IsBluecodeSelfPinAlias(UmgMcpJsonCompat::KeyToString(Pair.Key)))
                {
                    return true;
                }
            }
            return false;
        };
        const bool bAllowExplicitSelfTarget = !MemberClassHint.IsEmpty() || HasExplicitSelfInput();

        auto IsAssignableInputPin = [&](const UEdGraphPin* Pin) -> bool
        {
            if (!Pin ||
                Pin->Direction != EGPD_Input ||
                Pin->bHidden ||
                Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
            {
                return false;
            }

            if (IsBluecodeSelfPin(Pin) && !bAllowExplicitSelfTarget)
            {
                return false;
            }
            return true;
        };

        auto CountAssignableDataInputs = [&]() -> int32
        {
            int32 Count = 0;
            for (const UEdGraphPin* Pin : Node->Pins)
            {
                if (IsAssignableInputPin(Pin))
                {
                    ++Count;
                }
            }
            return Count;
        };

        auto HasInputPinForKey = [&](const FString& Key) -> bool
        {
            for (const UEdGraphPin* Pin : Node->Pins)
            {
                if (IsAssignableInputPin(Pin) && BluecodePinNameMatches(Pin, Key))
                {
                    return true;
                }
            }
            return false;
        };

        auto HasUnmatchedDynamicInputKey = [&]() -> bool
        {
            if (!InputWiresPtr || !(*InputWiresPtr).IsValid())
            {
                return false;
            }

            for (const auto& Pair : (*InputWiresPtr)->Values)
            {
                const FString Key = UmgMcpJsonCompat::KeyToString(Pair.Key);
                if (IsBluecodeDynamicInputKey(Key) && !HasInputPinForKey(Key))
                {
                    return true;
                }
            }
            return false;
        };

        auto AddUnmatchedNamedInputWarnings = [&]()
        {
            if (!InputWiresPtr || !(*InputWiresPtr).IsValid())
            {
                return;
            }

            for (const auto& Pair : (*InputWiresPtr)->Values)
            {
                const FString Key = UmgMcpJsonCompat::KeyToString(Pair.Key);
                if (!HasInputPinForKey(Key))
                {
                    AddInputWarning(
                        nullptr,
                        BluecodeJsonValueToString(Pair.Value),
                        FString::Printf(TEXT("Node '%s' has no assignable input pin named '%s'."), *BluecodeNodeTitle(Node), *Key));
                }
            }
        };

        auto PreconfigureFormatTextNode = [&]()
        {
            UK2Node_FormatText* FormatTextNode = Cast<UK2Node_FormatText>(Node);
            if (!FormatTextNode)
            {
                return;
            }

            UEdGraphPin* FormatPin = FormatTextNode->GetFormatPin();
            if (!FormatPin)
            {
                return;
            }

            FString FormatValue;
            bool bHasFormatValue = false;
            if (InputWiresPtr && (*InputWiresPtr).IsValid() && TryGetBluecodeInputWireValue(*InputWiresPtr, FormatPin, FormatValue))
            {
                bHasFormatValue = true;
            }
            else if (InferenceArgs && InferenceArgs->Num() > 0)
            {
                FormatValue = (*InferenceArgs)[0]->AsString();
                bHasFormatValue = !FormatValue.IsEmpty();
            }

            if (!bHasFormatValue)
            {
                return;
            }

            FString ErrorMessage;
            if (TrySetBluecodePinDefault(FormatPin, FormatValue, ErrorMessage))
            {
                FormatTextNode->PinDefaultValueChanged(FormatPin);
            }
            else
            {
                AddInputWarning(FormatPin, FormatValue, ErrorMessage);
            }
        };

        auto ExpandDynamicInputPins = [&]()
        {
            IK2Node_AddPinInterface* AddPinNode = Cast<IK2Node_AddPinInterface>(Node);
            if (!AddPinNode)
            {
                return;
            }

            int32 DesiredInputCount = 0;
            if (InferenceArgs)
            {
                DesiredInputCount += InferenceArgs->Num();
            }
            if (InputWiresPtr && (*InputWiresPtr).IsValid())
            {
                DesiredInputCount += (*InputWiresPtr)->Values.Num();
            }

            constexpr int32 MaxBluecodeAutoAddedPins = 32;
            int32 AddedCount = 0;
            while (AddedCount < MaxBluecodeAutoAddedPins)
            {
                const bool bNeedsMoreByCount = DesiredInputCount > CountAssignableDataInputs();
                const bool bNeedsMoreByDynamicKey = HasUnmatchedDynamicInputKey();
                if (!bNeedsMoreByCount && !bNeedsMoreByDynamicKey)
                {
                    break;
                }

                AddPinNode = Cast<IK2Node_AddPinInterface>(Node);
                if (!AddPinNode || !AddPinNode->CanAddPin())
                {
                    AddInputWarning(
                        nullptr,
                        TEXT(""),
                        FString::Printf(TEXT("Node '%s' cannot add enough dynamic input pins for the requested BlueCode inputs."), *BluecodeNodeTitle(Node)));
                    break;
                }

                AddPinNode->AddInputPin();
                if (UEdGraph* OwningGraph = Node->GetGraph())
                {
                    OwningGraph->NotifyNodeChanged(Node);
                }
                ++AddedCount;
            }

            if (AddedCount >= MaxBluecodeAutoAddedPins &&
                (DesiredInputCount > CountAssignableDataInputs() || HasUnmatchedDynamicInputKey()))
            {
                AddInputWarning(
                    nullptr,
                    TEXT(""),
                    FString::Printf(TEXT("Node '%s' still needs more dynamic pins after adding %d pins."), *BluecodeNodeTitle(Node), AddedCount));
            }
        };

        PreconfigureFormatTextNode();
        ExpandDynamicInputPins();
        AddUnmatchedNamedInputWarnings();

        auto TryWireSelfMember = [&](const FString& MemberName, UEdGraphPin* TargetPin, int32 LayoutIndex)
        {
            if (!TargetGraph || !TargetPin || MemberName.IsEmpty())
            {
                return false;
            }

            FGraphNodeCreator<UK2Node_VariableGet> VarCreator(*TargetGraph);
            UK2Node_VariableGet* GetNode = VarCreator.CreateNode(false);
            GetNode->VariableReference.SetSelfMember(FName(*MemberName));

            GetNode->NodePosX = Node->NodePosX - 250;
            GetNode->NodePosY = Node->NodePosY + (LayoutIndex * 50);

            VarCreator.Finalize();

            UEdGraphPin* OutPin = GetNode->GetValuePin();
            if (OutPin && TargetGraph->GetSchema()->CanCreateConnection(OutPin, TargetPin).Response != CONNECT_RESPONSE_DISALLOW)
            {
                TargetGraph->GetSchema()->TryCreateConnection(OutPin, TargetPin);
                return true;
            }

            TargetGraph->RemoveNode(GetNode);
            return false;
        };

        auto TryWireExpressionNode = [&](const FString& Expression, UEdGraphPin* TargetPin, int32 LayoutIndex, FString& OutError)
        {
            if (!TargetGraph || !TargetPin || Expression.IsEmpty())
            {
                return false;
            }

            TSet<FGuid> ExistingNodeGuids;
            for (UEdGraphNode* ExistingNode : TargetGraph->Nodes)
            {
                if (ExistingNode)
                {
                    ExistingNodeGuids.Add(ExistingNode->NodeGuid);
                }
            }

            auto RollbackCreatedExpressionNodes = [&]()
            {
                TArray<UEdGraphNode*> NodesToRemove;
                for (UEdGraphNode* CandidateNode : TargetGraph->Nodes)
                {
                    if (CandidateNode && !ExistingNodeGuids.Contains(CandidateNode->NodeGuid))
                    {
                        NodesToRemove.Add(CandidateNode);
                    }
                }
                for (UEdGraphNode* CandidateNode : NodesToRemove)
                {
                    TargetGraph->RemoveNode(CandidateNode);
                }
            };

            TSharedPtr<FJsonObject> ValuePayload = MakeShared<FJsonObject>();
            ValuePayload->SetStringField(TEXT("subAction"), TEXT("create_node"));
            ValuePayload->SetNumberField(TEXT("x"), Node->NodePosX - 260.0);
            ValuePayload->SetNumberField(TEXT("y"), Node->NodePosY + (LayoutIndex * 80.0));

            FString BinaryLeft;
            FString BinaryOp;
            FString BinaryRight;
            if (TryParseBluecodeBinaryExpression(Expression, BinaryLeft, BinaryOp, BinaryRight))
            {
                FString Family = BluecodeMathTypeFamily(TargetBlueprint, TEXT(""), BinaryLeft, BinaryRight);
                const FString TargetCategory = TargetPin->PinType.PinCategory.ToString().ToLower();
                if (TargetCategory.Contains(TEXT("int")) || TargetCategory.Contains(TEXT("byte")))
                {
                    Family = TEXT("int");
                }
                else if (TargetCategory.Contains(TEXT("float")) || TargetCategory.Contains(TEXT("double")) || TargetCategory.Contains(TEXT("real")))
                {
                    Family = TEXT("float");
                }

                const FString MathFunctionName = BluecodeMathFunctionName(BinaryOp, Family);
                if (MathFunctionName.IsEmpty())
                {
                    OutError = FString::Printf(TEXT("Unsupported expression operator '%s'."), *BinaryOp);
                    return false;
                }

                ValuePayload->SetStringField(TEXT("nodeName"), MathFunctionName);
                TArray<TSharedPtr<FJsonValue>> ValueArgs;
                ValueArgs.Add(MakeShared<FJsonValueString>(BinaryLeft));
                ValueArgs.Add(MakeShared<FJsonValueString>(BinaryRight));
                ValuePayload->SetArrayField(TEXT("extraArgs"), ValueArgs);
            }
            else
            {
                FString ExprCallName;
                TArray<FString> ExprCallArgs;
                if (!TryParseBluecodeCall(Expression, ExprCallName, ExprCallArgs))
                {
                    return false;
                }

                const bool bGenericValueNode =
                    ExprCallName.Equals(TEXT("value"), ESearchCase::IgnoreCase) ||
                    ExprCallName.Equals(TEXT("pure"), ESearchCase::IgnoreCase);
                const bool bGenericExecNode =
                    ExprCallName.Equals(TEXT("node"), ESearchCase::IgnoreCase) ||
                    ExprCallName.Equals(TEXT("action"), ESearchCase::IgnoreCase) ||
                    ExprCallName.Equals(TEXT("exec"), ESearchCase::IgnoreCase);
                if (bGenericExecNode)
                {
                    OutError = TEXT("Executable node expression cannot be used as a data input.");
                    return false;
                }

                if (bGenericValueNode)
                {
                    if (ExprCallArgs.Num() == 0 || ExprCallArgs[0].IsEmpty())
                    {
                        OutError = TEXT("value(...) expression requires an Action Menu name.");
                        return false;
                    }
                    ValuePayload->SetStringField(TEXT("nodeName"), ExprCallArgs[0]);
                    ValuePayload->SetBoolField(TEXT("useActionMenu"), true);
                    ApplyBluecodeActionHints(Expression, ExprCallName, ExprCallArgs[0], Payload, ValuePayload);
                    ApplyBluecodeArgsToPayload(ExprCallArgs, 1, ValuePayload);
                }
                else
                {
                    const FName TargetCategory = TargetPin->PinType.PinCategory;
                    if (TargetCategory == UEdGraphSchema_K2::PC_String ||
                        TargetCategory == UEdGraphSchema_K2::PC_Text ||
                        TargetCategory == UEdGraphSchema_K2::PC_Name)
                    {
                        return false;
                    }
                    ValuePayload->SetStringField(TEXT("nodeName"), ExprCallName);
                    ApplyBluecodeActionHints(Expression, ExprCallName, ExprCallName, Payload, ValuePayload);
                    ApplyBluecodeArgsToPayload(ExprCallArgs, 0, ValuePayload);
                }
            }

            UEdGraphNode* ValueNode = nullptr;
            TSharedPtr<FJsonObject> ValueResult = CreateNodeInstance(TargetGraph, ValuePayload, ValueNode);
            if (!ValueResult.IsValid() || !ValueNode)
            {
                OutError = TEXT("Failed to create expression value node.");
                if (ValueResult.IsValid())
                {
                    ValueResult->TryGetStringField(TEXT("error"), OutError);
                }
                RollbackCreatedExpressionNodes();
                return false;
            }

            TArray<UEdGraphPin*> CompatibleOutputs;
            for (UEdGraphPin* SourcePin : ValueNode->Pins)
            {
                if (!SourcePin || SourcePin->Direction != EGPD_Output || SourcePin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec || SourcePin->bHidden)
                {
                    continue;
                }
                const FPinConnectionResponse Response = TargetGraph->GetSchema()->CanCreateConnection(SourcePin, TargetPin);
                if (Response.Response == CONNECT_RESPONSE_MAKE ||
                    Response.Response == CONNECT_RESPONSE_MAKE_WITH_CONVERSION_NODE ||
                    Response.Response == CONNECT_RESPONSE_MAKE_WITH_PROMOTION)
                {
                    CompatibleOutputs.Add(SourcePin);
                }
            }

            if (CompatibleOutputs.Num() != 1)
            {
                OutError = CompatibleOutputs.Num() == 0
                    ? TEXT("Expression value node has no compatible output pin.")
                    : TEXT("Expression value node has multiple compatible output pins; use bluecode_connect with an explicit source pin.");
                RollbackCreatedExpressionNodes();
                return false;
            }

            if (!TargetGraph->GetSchema()->TryCreateConnection(CompatibleOutputs[0], TargetPin))
            {
                OutError = TEXT("Expression value node connection failed.");
                RollbackCreatedExpressionNodes();
                return false;
            }
            return true;
        };

        for (UEdGraphPin* Pin : Node->Pins)
        {
            if (IsAssignableInputPin(Pin))
            {
                FString ArgVal;
                bool bFoundVal = false;

                // 1. Try Named Argument (inputWires)
                if (InputWiresPtr && (*InputWiresPtr).IsValid())
                {
                    if (TryGetBluecodeInputWireValue(*InputWiresPtr, Pin, ArgVal))
                    {
                        bFoundVal = true;
                    }
                }

                // 2. Try Positional Argument (extraArgs) - Only if not named
                if (!bFoundVal && InferenceArgs && ArgIndex < InferenceArgs->Num())
                {
                    ArgVal = (*InferenceArgs)[ArgIndex]->AsString();
                    ArgIndex++; // Increment index only if we used a positional arg

                    if (ArgVal.Equals("null", ESearchCase::IgnoreCase) ||
                        ArgVal.Equals("(null)", ESearchCase::IgnoreCase) ||
                        ArgVal.Equals("wait", ESearchCase::IgnoreCase) ||
                        ArgVal.Equals("(wait)", ESearchCase::IgnoreCase))
                    {
                        continue;
                    }
                    bFoundVal = true;
                }

                if (bFoundVal)
                {
                     bool bWired = false;

                     // -----------------------------------------------------------
                     // 1. Try Node ID (Connect Pure Data Node) with Pin Support (NodeID:PinName)
                     // -----------------------------------------------------------
                     FString SourceNodeId = ArgVal;
                     FString SourcePinName;
                     ArgVal.Split(TEXT(":"), &SourceNodeId, &SourcePinName);

                     UEdGraphNode* SourceNode = FindNodeByIdOrName(TargetGraph, SourceNodeId);
                     if (SourceNode && SourceNode != Node)
                     {
                          for (UEdGraphPin* SourcePin : SourceNode->Pins)
                          {
                              // If PinName specified, match it. Otherwise match any compatible output.
                              bool bNameMatch = SourcePinName.IsEmpty() || SourcePin->PinName.ToString() == SourcePinName;

                              if (bNameMatch && SourcePin->Direction == EGPD_Output && SourcePin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec && !SourcePin->bHidden)
                              {
                                  if (TargetGraph->GetSchema()->CanCreateConnection(SourcePin, Pin).Response != CONNECT_RESPONSE_DISALLOW)
                                  {
                                      TargetGraph->GetSchema()->TryCreateConnection(SourcePin, Pin);
                                      bWired = true;

                                      // CONSUMPTION LAYOUT only if not manually positioned
                                      if (!Payload->HasField(TEXT("x")))
                                      {
                                         SourceNode->NodePosX = Node->NodePosX - 250;
                                         SourceNode->NodePosY = Node->NodePosY + (Pin->SourceIndex * 100);
                                      }
                                      break;
                                  }
                              }
                          }
                     }

                     // -----------------------------------------------------------
                     // 2. Try BlueCode value/function/math expression
                     // -----------------------------------------------------------
                     if (!bWired)
                     {
                         FString ExpressionError;
                         bWired = TryWireExpressionNode(ArgVal, Pin, Pin->SourceIndex, ExpressionError);
                         if (!bWired && !ExpressionError.IsEmpty())
                         {
                             AddInputWarning(Pin, ArgVal, ExpressionError);
                         }
                     }

                     // -----------------------------------------------------------
                     // 3. Try Smart Wiring (Member Variable)
                     // -----------------------------------------------------------
                     if (!bWired && TargetBlueprint)
                     {
                          if (FindFProperty<FProperty>(TargetBlueprint->SkeletonGeneratedClass, *ArgVal))
                          {
                               bWired = TryWireSelfMember(ArgVal, Pin, Pin->SourceIndex);
                          }
                     }

                     // -----------------------------------------------------------
                     // 4. Try Smart Wiring (Widget Tree Variable)
                     // -----------------------------------------------------------
                     if (!bWired && TargetBlueprint)
                     {
                         UWidgetBlueprint* WidgetBP = Cast<UWidgetBlueprint>(TargetBlueprint);
                         UWidget* FoundWidget = (WidgetBP && WidgetBP->WidgetTree) ? WidgetBP->WidgetTree->FindWidget(FName(*ArgVal)) : nullptr;
                         if (FoundWidget)
                         {
                             if (!FoundWidget->bIsVariable || !WidgetBP->WidgetVariableNameToGuidMap.Contains(FoundWidget->GetFName()))
                             {
                                 FoundWidget->Modify();
                                 WidgetBP->Modify();
                                 FoundWidget->bIsVariable = true;
                                 if (!WidgetBP->WidgetVariableNameToGuidMap.Contains(FoundWidget->GetFName()))
                                 {
                                     WidgetBP->WidgetVariableNameToGuidMap.Add(FoundWidget->GetFName(), FGuid::NewGuid());
                                 }
                                 FKismetEditorUtilities::CompileBlueprint(WidgetBP);
                             }

                             bWired = TryWireSelfMember(ArgVal, Pin, Pin->SourceIndex);
                         }
                     }

                     // -----------------------------------------------------------
                     // 5. Fallback: Set Default Value (Literal)
                     // -----------------------------------------------------------
                     if (!bWired)
                     {
                         FString ErrorMessage;
                         if (!TrySetBluecodePinDefault(Pin, ArgVal, ErrorMessage))
                         {
                             UE_LOG(LogUmgBlueprint, Warning, TEXT("AddNode: %s"), *ErrorMessage);
                             AddInputWarning(Pin, ArgVal, ErrorMessage);
                         }
                     }
                }
            }
        }
    };

    UEdGraphNode* NewNode = nullptr;

    // ----------------------------------------------------
    // Node Type Resolution & Fallback Logic
    // ----------------------------------------------------
    TSharedPtr<FJsonObject> ActionSpawnError;
    if (TrySpawnBlueprintActionNode(TargetGraph, Payload, false, NewNode, ActionSpawnError))
    {
        SetupNode(NewNode);
    }
    else if (ActionSpawnError.IsValid())
    {
        return ActionSpawnError;
    }

    // 1. Try to resolve specific UK2Node class logic (e.g. ComponentBoundEvent, Event)
    // (Existing special cases kept for structure, but falling back to CallFunction is the key)

    // Explicit Special Cases
    if (!NewNode && (NodeType == TEXT("Branch") || NodeType == TEXT("If")))
    {
         FGraphNodeCreator<UK2Node_IfThenElse> NodeCreator(*TargetGraph);
         UK2Node_IfThenElse* BranchNode = NodeCreator.CreateNode(false);
         NodeCreator.Finalize();
         SetupNode(BranchNode);
         NewNode = BranchNode;
    }
    else if (!NewNode && NodeType == TEXT("Sequence"))
    {
         FGraphNodeCreator<UK2Node_ExecutionSequence> NodeCreator(*TargetGraph);
         UK2Node_ExecutionSequence* SeqNode = NodeCreator.CreateNode(false);
         NodeCreator.Finalize();
         SetupNode(SeqNode);
         NewNode = SeqNode;
    }
    else if (!NewNode && (NodeType == TEXT("CallFunction") || NodeType.StartsWith(TEXT("Call")) ||
        // Implicit Fallback: If it's not a special reserved keyword, assume it's a function name
        (!NodeType.Equals("ComponentBoundEvent") && !NodeType.Equals("Event") &&
         !NodeType.Equals("Get") && !NodeType.Equals("VariableGet") &&
         !NodeType.Equals("Set") && !NodeType.Equals("VariableSet") &&
         !NodeType.Equals("Cast") && !NodeType.Equals("DynamicCast"))))
    {
         FString MemberName;
		 Payload->TryGetStringField(TEXT("memberName"), MemberName);
         if (MemberName.IsEmpty()) Payload->TryGetStringField(TEXT("nodeName"), MemberName);

         // KEY CHANGE: usage of NodeType as MemberName if MemberName is missing
         if (MemberName.IsEmpty()) MemberName = NodeType;

         FString MemberClass;
         Payload->TryGetStringField(TEXT("memberClass"), MemberClass);

         // Try to find the function in various places
         UFunction* FoundFunc = nullptr;

         // Priority 0: Explicit member class, e.g. /Script/UMG.WidgetSwitcher.
         if (!MemberClass.IsEmpty())
         {
             UClass* ExplicitClass = FindObject<UClass>(nullptr, *MemberClass);
             if (!ExplicitClass)
             {
                 ExplicitClass = LoadObject<UClass>(nullptr, *MemberClass);
             }
             if (ExplicitClass)
             {
                 FoundFunc = ExplicitClass->FindFunctionByName(*MemberName);
             }
         }

         // Priority 1: Self (Widget Class)
         if (!FoundFunc && TargetBlueprint && TargetBlueprint->GeneratedClass)
         {
             FoundFunc = TargetBlueprint->GeneratedClass->FindFunctionByName(*MemberName);
         }

         // Priority 2: Standard Libraries (KismetSystemLibrary, etc)
         if (!FoundFunc) FoundFunc = UKismetSystemLibrary::StaticClass()->FindFunctionByName(*MemberName);
         if (!FoundFunc) FoundFunc = UGameplayStatics::StaticClass()->FindFunctionByName(*MemberName);
         if (!FoundFunc) FoundFunc = UKismetMathLibrary::StaticClass()->FindFunctionByName(*MemberName);

         if (FoundFunc)
         {
            FGraphNodeCreator<UK2Node_CallFunction> NodeCreator(*TargetGraph);
            UK2Node_CallFunction* CallFuncNode = NodeCreator.CreateNode(false);
            CallFuncNode->SetFromFunction(FoundFunc);
            NodeCreator.Finalize();
            SetupNode(CallFuncNode);
            NewNode = CallFuncNode;
         }
         else
         {
             TSharedPtr<FJsonObject> FallbackActionError;
             if (TrySpawnBlueprintActionNode(TargetGraph, Payload, true, NewNode, FallbackActionError))
             {
                 SetupNode(NewNode);
             }
             else if (FallbackActionError.IsValid())
             {
                 return FallbackActionError;
             }
             else
             {
             // FUZZY SEARCH FALLBACK
             TArray<FString> Suggestions = GetFuzzySuggestions(MemberName, TargetBlueprint ? TargetBlueprint->GeneratedClass : nullptr);
             TSharedPtr<FJsonObject> ErrorResult = MakeShared<FJsonObject>();
             ErrorResult->SetBoolField(TEXT("success"), false);

             FString SuggestionStr = FString::Join(Suggestions, TEXT(", "));
             ErrorResult->SetStringField(TEXT("error"), FString::Printf(TEXT("Function '%s' not found. Did you mean: %s?"), *MemberName, *SuggestionStr));

             // We return this error object immediately
             return ErrorResult;
             }
         }
    }
    else if (!NewNode && NodeType == TEXT("ComponentBoundEvent")) // FIX: Replaced unconditional block with else if
    {
        FString ComponentName;
        Payload->TryGetStringField(TEXT("componentName"), ComponentName);
        FString EventName;
        Payload->TryGetStringField(TEXT("eventName"), EventName);

        if (InferenceArgs)
        {
            if (ComponentName.IsEmpty() && InferenceArgs->Num() > 0) ComponentName = (*InferenceArgs)[0]->AsString();
            if (EventName.IsEmpty() && InferenceArgs->Num() > 1) EventName = (*InferenceArgs)[1]->AsString();
        }

        if (!ComponentName.IsEmpty() && !EventName.IsEmpty() && TargetBlueprint)
        {
            FObjectProperty* ComponentProp = FindFProperty<FObjectProperty>(TargetBlueprint->SkeletonGeneratedClass, *ComponentName);
            if (ComponentProp)
            {
                 FMulticastDelegateProperty* DelegateProp = FindFProperty<FMulticastDelegateProperty>(ComponentProp->PropertyClass, *EventName);
                 if (DelegateProp)
                 {
                     FGraphNodeCreator<UK2Node_ComponentBoundEvent> NodeCreator(*TargetGraph);
                     UK2Node_ComponentBoundEvent* BoundEventNode = NodeCreator.CreateNode(false);
                     BoundEventNode->InitializeComponentBoundEventParams(ComponentProp, DelegateProp);

                     NodeCreator.Finalize();
                     SetupNode(BoundEventNode);
                     NewNode = BoundEventNode;
                 }
            }
        }
    }
    else if (!NewNode && NodeType == TEXT("Event"))
    {
         FString EventName;
         Payload->TryGetStringField(TEXT("eventName"), EventName);
         if (EventName.IsEmpty()) EventName = NodeType;

        if (TargetBlueprint && TargetBlueprint->ParentClass)
        {
            UFunction* EventFunc = TargetBlueprint->ParentClass->FindFunctionByName(*EventName);
            if (EventFunc)
            {
                FGraphNodeCreator<UK2Node_Event> NodeCreator(*TargetGraph);
                UK2Node_Event* EventNode = NodeCreator.CreateNode(false);
                EventNode->EventReference.SetFromField<UFunction>(EventFunc, false);
                EventNode->bOverrideFunction = true;
                NodeCreator.Finalize();
                SetupNode(EventNode);
                NewNode = EventNode;
            }
        }
    }
    else if (!NewNode && (NodeType == TEXT("Get") || NodeType == TEXT("VariableGet") || NodeType == TEXT("GetVariable")))
    {
         FString VarName;
         Payload->TryGetStringField(TEXT("variableName"), VarName);
         if (VarName.IsEmpty()) Payload->TryGetStringField(TEXT("name"), VarName);

         if (VarName.IsEmpty() && InferenceArgs && InferenceArgs->Num() > 0)
         {
             VarName = (*InferenceArgs)[0]->AsString();
         }

         if (!VarName.IsEmpty() && TargetBlueprint)
         {
             FGraphNodeCreator<UK2Node_VariableGet> NodeCreator(*TargetGraph);
             UK2Node_VariableGet* GetNode = NodeCreator.CreateNode(false);
             GetNode->VariableReference.SetSelfMember(FName(*VarName));
             NodeCreator.Finalize();
             SetupNode(GetNode);
             NewNode = GetNode;
         }
    }
    else if (!NewNode && (NodeType == TEXT("Set") || NodeType == TEXT("VariableSet") || NodeType == TEXT("SetVariable")))
    {
         FString VarName;
         Payload->TryGetStringField(TEXT("variableName"), VarName);
         if (VarName.IsEmpty()) Payload->TryGetStringField(TEXT("name"), VarName);

         if (VarName.IsEmpty() && InferenceArgs && InferenceArgs->Num() > 0)
         {
             VarName = (*InferenceArgs)[0]->AsString();
         }

         if (!VarName.IsEmpty() && TargetBlueprint)
         {
             FGraphNodeCreator<UK2Node_VariableSet> NodeCreator(*TargetGraph);
             UK2Node_VariableSet* SetNode = NodeCreator.CreateNode(false);
             SetNode->VariableReference.SetSelfMember(FName(*VarName));
             NodeCreator.Finalize();
             SetupNode(SetNode);
             NewNode = SetNode;
         }
    }
    else if (!NewNode && (NodeType == TEXT("Cast") || NodeType == TEXT("DynamicCast")))
    {
         FString TargetClassName;
         Payload->TryGetStringField(TEXT("targetClass"), TargetClassName);

         if (TargetClassName.IsEmpty() && InferenceArgs && InferenceArgs->Num() > 0)
         {
             TargetClassName = (*InferenceArgs)[0]->AsString();
         }

         if (!TargetClassName.IsEmpty())
         {
             UClass* TargetClass = ResolveUClass(TargetClassName);
             if (TargetClass)
             {
                 FGraphNodeCreator<UK2Node_DynamicCast> NodeCreator(*TargetGraph);
                 UK2Node_DynamicCast* CastNode = NodeCreator.CreateNode(false);
                 CastNode->TargetType = TargetClass;
                 NodeCreator.Finalize();
                 SetupNode(CastNode);
                 NewNode = CastNode;
             }
         }
    }

	if (NewNode)
	{
        // Apply optional comment
        FString Comment;
        if (Payload->TryGetStringField(TEXT("comment"), Comment) && !Comment.IsEmpty())
        {
            NewNode->NodeComment = Comment;
            NewNode->bCommentBubbleVisible = true;
        }

        OutNode = NewNode;
        TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
        Result->SetBoolField(TEXT("success"), true);
        Result->SetStringField(TEXT("nodeId"), NewNode->NodeGuid.ToString());
        Result->SetStringField(TEXT("nodeName"), NewNode->GetName());
        AddBluecodeNodeResultFields(NewNode, Result);
        if (NodePropertyResult.IsValid())
        {
            double RequestedProperties = 0.0;
            NodePropertyResult->TryGetNumberField(TEXT("requested_count"), RequestedProperties);
            if (RequestedProperties > 0.0)
            {
                Result->SetObjectField(TEXT("node_properties"), NodePropertyResult);
                bool bNodePropertiesOk = true;
                NodePropertyResult->TryGetBoolField(TEXT("success"), bNodePropertiesOk);
                if (!bNodePropertiesOk)
                {
                    Result->SetBoolField(TEXT("success"), false);
                }
            }
        }

        bool bIsExec = false;
        for (UEdGraphPin* Pin : NewNode->Pins)
        {
            if (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
            {
                bIsExec = true;
                break;
            }
        }
        Result->SetBoolField(TEXT("isExec"), bIsExec);

        // SEMANTIC WARNINGS
        FString SubAction;
        Payload->TryGetStringField(TEXT("subAction"), SubAction);

        if (SubAction == TEXT("add_function_step") && !bIsExec)
        {
             Result->SetStringField(TEXT("warning"), TEXT("Created Pure Node via add_step. Execution flow (PC) was NOT advanced. Use prepare_value for data nodes."));
        }
        else if (SubAction == TEXT("create_node") && bIsExec) // create_node == prepare_value
        {
             Result->SetStringField(TEXT("warning"), TEXT("Created Executable Node via prepare_value. It is NOT connected to execution flow. Did you mean add_step? Use connect_pins to fix."));
        }

        // Only include unconnected input pins on non-exec (pure/data) nodes,
        // where the caller may need to know what data inputs to wire up.
        // For exec nodes the AI should focus on whether wiring succeeded, not listing all pins.
        if (!bIsExec)
        {
            TArray<TSharedPtr<FJsonValue>> PinInfo;
            for (UEdGraphPin* Pin : NewNode->Pins)
            {
                if (Pin->Direction == EGPD_Input && Pin->LinkedTo.Num() == 0 && !Pin->bHidden)
                {
                    TSharedPtr<FJsonObject> PinObj = MakeShared<FJsonObject>();
                    PinObj->SetStringField(TEXT("name"), Pin->PinName.ToString());
                    PinObj->SetStringField(TEXT("type"), Pin->PinType.PinCategory.ToString());
                    if (Pin->PinType.PinSubCategoryObject.IsValid())
                    {
                        PinObj->SetStringField(TEXT("subType"), Pin->PinType.PinSubCategoryObject->GetName());
                    }
                    PinInfo.Add(MakeShared<FJsonValueObject>(PinObj));
                }
            }
            if (PinInfo.Num() > 0)
            {
                Result->SetArrayField(TEXT("unconnectedInputs"), PinInfo);
            }
        }
        if (InputWarnings.Num() > 0)
        {
            Result->SetArrayField(TEXT("input_warnings"), InputWarnings);
        }

        return Result;
	}

	return nullptr;
}

TSharedPtr<FJsonObject> UUmgBlueprintFunctionSubsystem::AddNode(UEdGraph* TargetGraph, const TSharedPtr<FJsonObject>& Payload)
{
    if (!TargetGraph || !Payload.IsValid())
    {
        TSharedPtr<FJsonObject> Error = MakeShared<FJsonObject>();
        Error->SetBoolField(TEXT("success"), false);
        Error->SetStringField(TEXT("error_code"), TEXT("invalid_add_node_context"));
        Error->SetStringField(TEXT("error"), TEXT("AddNode requires a valid graph and payload."));
        return Error;
    }

    // CreateNodeInstance may create helper/data nodes while configuring the requested
    // node. Keep a graph-local snapshot so every failed path can remove all artifacts.
    UBlueprint* OwningBlueprint = ResolveBlueprintFromGraph(TargetGraph);
    UPackage* OwningPackage = OwningBlueprint ? OwningBlueprint->GetOutermost() : nullptr;
    const bool bPackageWasDirty = OwningPackage ? OwningPackage->IsDirty() : false;
    TSet<FGuid> ExistingNodeGuids;
    for (UEdGraphNode* ExistingNode : TargetGraph->Nodes)
    {
        if (ExistingNode)
        {
            ExistingNodeGuids.Add(ExistingNode->NodeGuid);
        }
    }

    auto RollbackCreatedNodes = [&]()
    {
        TArray<UEdGraphNode*> NodesToRemove;
        for (UEdGraphNode* Candidate : TargetGraph->Nodes)
        {
            if (Candidate && !ExistingNodeGuids.Contains(Candidate->NodeGuid))
            {
                NodesToRemove.Add(Candidate);
            }
        }

        for (UEdGraphNode* Candidate : NodesToRemove)
        {
            if (OwningBlueprint)
            {
                FBlueprintEditorUtils::RemoveNode(OwningBlueprint, Candidate, true);
            }
            else
            {
                TargetGraph->RemoveNode(Candidate);
            }
        }
        if (OwningPackage)
        {
            OwningPackage->SetDirtyFlag(bPackageWasDirty);
        }
    };

    auto MakeFailure = [&](const FString& ErrorCode, const FString& Message)
    {
        RollbackCreatedNodes();
        TSharedPtr<FJsonObject> Error = MakeShared<FJsonObject>();
        Error->SetBoolField(TEXT("success"), false);
        Error->SetStringField(TEXT("error_code"), ErrorCode);
        Error->SetStringField(TEXT("error"), Message);
        Error->SetBoolField(TEXT("rolled_back"), true);
        return Error;
    };

    UEdGraphNode* NewNode = nullptr;
    TSharedPtr<FJsonObject> Result = CreateNodeInstance(TargetGraph, Payload, NewNode);
    if (!Result.IsValid())
    {
        return MakeFailure(TEXT("node_creation_failed"), TEXT("Node creation returned no result."));
    }

    bool bCreatedSuccessfully = false;
    Result->TryGetBoolField(TEXT("success"), bCreatedSuccessfully);
    if (!bCreatedSuccessfully || !NewNode)
    {
        FString ErrorMessage = TEXT("Node creation failed before a valid graph node was produced.");
        Result->TryGetStringField(TEXT("error"), ErrorMessage);
        RollbackCreatedNodes();
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(TEXT("error_code"), TEXT("node_creation_failed"));
        Result->SetStringField(TEXT("error"), ErrorMessage);
        Result->SetBoolField(TEXT("rolled_back"), true);
        return Result;
    }

    const UEdGraphSchema* Schema = TargetGraph->GetSchema();
    if (!Schema)
    {
        return MakeFailure(TEXT("missing_graph_schema"), TEXT("Target graph has no schema for safe connection validation."));
    }

    auto FindExecPin = [](UEdGraphNode* Node, const EEdGraphPinDirection Direction, const FString& PreferredName, const bool bRequireUnlinked) -> UEdGraphPin*
    {
        if (!Node)
        {
            return nullptr;
        }
        UEdGraphPin* FirstMatch = nullptr;
        for (UEdGraphPin* Pin : Node->Pins)
        {
            if (!Pin || Pin->Direction != Direction || Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec)
            {
                continue;
            }
            if (bRequireUnlinked && Pin->LinkedTo.Num() > 0)
            {
                continue;
            }
            if (!PreferredName.IsEmpty() && BluecodePinNameMatches(Pin, PreferredName))
            {
                return Pin;
            }
            if (!FirstMatch)
            {
                FirstMatch = Pin;
            }
        }
        return PreferredName.IsEmpty() ? FirstMatch : nullptr;
    };

    auto CountExecPins = [](const UEdGraphNode* Node, const EEdGraphPinDirection Direction, const bool bRequireUnlinked)
    {
        int32 Count = 0;
        if (Node)
        {
            for (const UEdGraphPin* Pin : Node->Pins)
            {
                if (Pin &&
                    Pin->Direction == Direction &&
                    Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec &&
                    (!bRequireUnlinked || Pin->LinkedTo.Num() == 0))
                {
                    ++Count;
                }
            }
        }
        return Count;
    };

    auto IsUnionSafeResponse = [](const FPinConnectionResponse& Response)
    {
        return Response.Response == CONNECT_RESPONSE_MAKE ||
            Response.Response == CONNECT_RESPONSE_MAKE_WITH_CONVERSION_NODE ||
            Response.Response == CONNECT_RESPONSE_MAKE_WITH_PROMOTION;
    };

    // A semantic right-anchor insertion is an explicit splice: preserve both
    // existing nodes and replace only the single anchor edge with prev->new->anchor.
    FString InsertBeforeNodeId;
    Payload->TryGetStringField(TEXT("insertBeforeNodeId"), InsertBeforeNodeId);
    if (InsertBeforeNodeId.IsEmpty())
    {
        Payload->TryGetStringField(TEXT("insert_before_node_id"), InsertBeforeNodeId);
    }
    if (!InsertBeforeNodeId.IsEmpty())
    {
        UEdGraphNode* AnchorNode = FindNodeByIdOrName(TargetGraph, InsertBeforeNodeId);
        UEdGraphPin* AnchorExecIn = FindExecPin(AnchorNode, EGPD_Input, TEXT(""), false);
        UEdGraphPin* NewExecIn = FindExecPin(NewNode, EGPD_Input, TEXT(""), true);
        UEdGraphPin* NewExecOut = FindExecPin(NewNode, EGPD_Output, TEXT(""), true);
        if (!AnchorNode ||
            !AnchorExecIn ||
            AnchorExecIn->LinkedTo.Num() != 1 ||
            !NewExecIn ||
            !NewExecOut ||
            CountExecPins(NewNode, EGPD_Input, true) != 1 ||
            CountExecPins(NewNode, EGPD_Output, true) != 1)
        {
            return MakeFailure(
                TEXT("unsafe_right_anchor"),
                TEXT("Right-anchor insertion requires one existing incoming exec edge and one unambiguous exec input/output on the new node."));
        }

        UEdGraphPin* PreviousExecOut = AnchorExecIn->LinkedTo[0];
        if (!PreviousExecOut || PreviousExecOut->Direction != EGPD_Output || PreviousExecOut->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec)
        {
            return MakeFailure(TEXT("invalid_right_anchor_edge"), TEXT("The right anchor's incoming edge is not a valid exec connection."));
        }

        Schema->BreakSinglePinLink(PreviousExecOut, AnchorExecIn);
        const bool bConnectedPrevious =
            IsUnionSafeResponse(Schema->CanCreateConnection(PreviousExecOut, NewExecIn)) &&
            Schema->TryCreateConnection(PreviousExecOut, NewExecIn);
        const bool bConnectedAnchor =
            bConnectedPrevious &&
            IsUnionSafeResponse(Schema->CanCreateConnection(NewExecOut, AnchorExecIn)) &&
            Schema->TryCreateConnection(NewExecOut, AnchorExecIn);
        if (!bConnectedPrevious || !bConnectedAnchor)
        {
            Schema->BreakPinLinks(*NewExecIn, false);
            Schema->BreakPinLinks(*NewExecOut, false);
            Schema->TryCreateConnection(PreviousExecOut, AnchorExecIn);
            return MakeFailure(
                TEXT("right_anchor_splice_failed"),
                TEXT("Failed to splice the new node before the right anchor; the original exec edge was restored."));
        }

        NewNode->NodePosX = AnchorNode->NodePosX - 300;
        NewNode->NodePosY = AnchorNode->NodePosY;
        Result->SetBoolField(TEXT("inserted_before_anchor"), true);
        Result->SetStringField(TEXT("anchor_node_id"), AnchorNode->NodeGuid.ToString());
    }

    // FORWARD WIRING (Sequence)
    FString AutoConnectId;
    if (InsertBeforeNodeId.IsEmpty() && Payload->TryGetStringField(TEXT("autoConnectToNodeId"), AutoConnectId) && !AutoConnectId.IsEmpty())
    {
        UEdGraphNode* PrevNode = FindNodeByIdOrName(TargetGraph, AutoConnectId);
        if (!PrevNode)
        {
            return MakeFailure(TEXT("auto_connect_source_missing"), TEXT("The requested auto-connect source node no longer exists."));
        }

        FString SourcePinName;
        Payload->TryGetStringField(TEXT("autoConnectFromPin"), SourcePinName);
        if (SourcePinName.IsEmpty()) Payload->TryGetStringField(TEXT("auto_connect_from_pin"), SourcePinName);
        FString TargetPinName;
        Payload->TryGetStringField(TEXT("autoConnectToPin"), TargetPinName);
        if (TargetPinName.IsEmpty()) Payload->TryGetStringField(TEXT("auto_connect_to_pin"), TargetPinName);

        UEdGraphPin* ExecOut = FindExecPin(PrevNode, EGPD_Output, SourcePinName, true);
        UEdGraphPin* ExecIn = FindExecPin(NewNode, EGPD_Input, TargetPinName, true);
        if (SourcePinName.IsEmpty() && CountExecPins(PrevNode, EGPD_Output, true) != 1)
        {
            return MakeFailure(
                TEXT("ambiguous_auto_connect_source"),
                TEXT("Auto-connect found zero or multiple free exec outputs; specify autoConnectFromPin or use structured BlueCode control flow."));
        }
        if (TargetPinName.IsEmpty() && CountExecPins(NewNode, EGPD_Input, true) != 1)
        {
            return MakeFailure(
                TEXT("ambiguous_auto_connect_target"),
                TEXT("The new node has zero or multiple exec inputs; specify autoConnectToPin."));
        }
        if (!ExecOut || !ExecIn)
        {
            return MakeFailure(
                TEXT("auto_connect_would_replace_edge"),
                TEXT("No unlinked exec pin is available for union-safe auto-connect; existing execution links were preserved."));
        }

        const FPinConnectionResponse Response = Schema->CanCreateConnection(ExecOut, ExecIn);
        if (!IsUnionSafeResponse(Response) || !Schema->TryCreateConnection(ExecOut, ExecIn))
        {
            return MakeFailure(
                TEXT("auto_connect_failed"),
                FString::Printf(TEXT("Exec auto-connect was rejected without changing existing links (%s)."), *Response.Message.ToString()));
        }

        // Auto-Layout: Place new node to the right of Previous Node.
        NewNode->NodePosX = PrevNode->NodePosX + 300;
        NewNode->NodePosY = PrevNode->NodePosY;
        Result->SetBoolField(TEXT("auto_connected"), true);
    }
    else if (InsertBeforeNodeId.IsEmpty())
    {
        // FLOATING STRATEGY: No Auto-Connect (Prepare Data)
        // Place below the current cursor node to keep organized
        if (GEditor)
        {
             UUmgAttentionSubsystem* AttentionSystem = GEditor->GetEditorSubsystem<UUmgAttentionSubsystem>();
             if (AttentionSystem)
             {
                 FString CursorNodeId = AttentionSystem->GetCursorNode();
                 if (!CursorNodeId.IsEmpty())
                 {
                     UEdGraphNode* CursorNode = FindNodeByIdOrName(TargetGraph, CursorNodeId);
                     if (CursorNode)
                     {
                         // Stack below cursor
                         NewNode->NodePosX = CursorNode->NodePosX;
                         NewNode->NodePosY = CursorNode->NodePosY + 250;
                     }
                 }
                 else
                 {
                     // No cursor? Default to 0,0 or maybe some offset
                     NewNode->NodePosX = 0;
                     NewNode->NodePosY = 300;
                 }
             }
        }
    }
    AddBluecodeNodeResultFields(NewNode, Result);
    return Result;
}

TSharedPtr<FJsonObject> UUmgBlueprintFunctionSubsystem::AddParam(UEdGraph* TargetGraph, const TSharedPtr<FJsonObject>& Payload)
{
    if (!TargetGraph || !Payload.IsValid()) return nullptr;

    FString AutoConnectId;
    Payload->TryGetStringField(TEXT("autoConnectToNodeId"), AutoConnectId);

    // Find Target Node (Reverse Pointer from add_function_step)
    UEdGraphNode* TargetNode = FindNodeByIdOrName(TargetGraph, AutoConnectId);
    if (!TargetNode)
    {
         return MakeShared<FJsonObject>();
    }

    FString ParamName;
    Payload->TryGetStringField(TEXT("param_name"), ParamName);
    if (ParamName.IsEmpty()) Payload->TryGetStringField(TEXT("nodeName"), ParamName);

    const TArray<TSharedPtr<FJsonValue>>* ExtraArgs = nullptr;
    Payload->TryGetArrayField(TEXT("extraArgs"), ExtraArgs);

    bool bSuccess = false;

    // Find the Input Pin
    UEdGraphPin* TargetPin = TargetNode->FindPin(FName(*ParamName));
    if (!TargetPin)
    {
         for (UEdGraphPin* Pin : TargetNode->Pins)
         {
             if (Pin->Direction == EGPD_Input && Pin->PinName.ToString().Equals(ParamName, ESearchCase::IgnoreCase))
             {
                 TargetPin = Pin;
                 break;
             }
         }
    }

    if (TargetPin)
    {
         // Found wire!
         FString ArgVal;
         if (ExtraArgs && ExtraArgs->Num() > 0)
         {
              ArgVal = (*ExtraArgs)[0]->AsString();
         }

         if (!ArgVal.IsEmpty())
         {
              FString ErrorMessage;
              bSuccess = TrySetBluecodePinDefault(TargetPin, ArgVal, ErrorMessage);
         }
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), bSuccess);
    return Result;
}

TSharedPtr<FJsonObject> UUmgBlueprintFunctionSubsystem::ConnectPins(UEdGraph* Graph, const TSharedPtr<FJsonObject>& Params)
{
	if (!Graph || !Params.IsValid())
	{
		TSharedPtr<FJsonObject> Error = MakeShared<FJsonObject>();
		Error->SetBoolField(TEXT("success"), false);
		Error->SetStringField(TEXT("error"), TEXT("Missing graph or params for connect_pins."));
		return Error;
	}

	FString NodeIdA, PinNameA, NodeIdB, PinNameB;
	Params->TryGetStringField(TEXT("nodeIdA"), NodeIdA);
	Params->TryGetStringField(TEXT("pinNameA"), PinNameA);
	Params->TryGetStringField(TEXT("nodeIdB"), NodeIdB);
	Params->TryGetStringField(TEXT("pinNameB"), PinNameB);

	auto ParseEndpoint = [](const FString& Endpoint, FString& OutNode, FString& OutPin, const FString& DefaultPin)
	{
		if (Endpoint.IsEmpty())
		{
			return;
		}
		FString Node = Endpoint;
		FString Pin = DefaultPin;
		if (Endpoint.Split(TEXT(":"), &Node, &Pin))
		{
			Node.TrimStartAndEndInline();
			Pin.TrimStartAndEndInline();
		}
		else
		{
			Node.TrimStartAndEndInline();
		}
		OutNode = Node;
		OutPin = Pin;
	};

	FString Source;
	FString Target;
	if (Params->TryGetStringField(TEXT("source"), Source))
	{
		ParseEndpoint(Source, NodeIdA, PinNameA, TEXT("Return Value"));
	}
	else if (Params->TryGetStringField(TEXT("from"), Source))
	{
		ParseEndpoint(Source, NodeIdA, PinNameA, TEXT("Return Value"));
	}
	if (Params->TryGetStringField(TEXT("target"), Target))
	{
		ParseEndpoint(Target, NodeIdB, PinNameB, TEXT("InPin"));
	}
	else if (Params->TryGetStringField(TEXT("to"), Target))
	{
		ParseEndpoint(Target, NodeIdB, PinNameB, TEXT("InPin"));
	}

	if (NodeIdA.IsEmpty()) Params->TryGetStringField(TEXT("source_node"), NodeIdA);
	if (NodeIdA.IsEmpty()) Params->TryGetStringField(TEXT("from_node"), NodeIdA);
	if (PinNameA.IsEmpty()) Params->TryGetStringField(TEXT("source_pin"), PinNameA);
	if (PinNameA.IsEmpty()) Params->TryGetStringField(TEXT("from_pin"), PinNameA);
	if (NodeIdB.IsEmpty()) Params->TryGetStringField(TEXT("target_node"), NodeIdB);
	if (NodeIdB.IsEmpty()) Params->TryGetStringField(TEXT("to_node"), NodeIdB);
	if (PinNameB.IsEmpty()) Params->TryGetStringField(TEXT("target_pin"), PinNameB);
	if (PinNameB.IsEmpty()) Params->TryGetStringField(TEXT("to_pin"), PinNameB);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), false);
	Result->SetStringField(TEXT("source"), FString::Printf(TEXT("%s:%s"), *NodeIdA, *PinNameA));
	Result->SetStringField(TEXT("target"), FString::Printf(TEXT("%s:%s"), *NodeIdB, *PinNameB));

	UEdGraphNode* NodeA = FindNodeByIdOrName(Graph, NodeIdA);
	UEdGraphNode* NodeB = FindNodeByIdOrName(Graph, NodeIdB);
	if (!NodeA)
	{
		Result->SetStringField(TEXT("error"), FString::Printf(TEXT("Source node '%s' not found."), *NodeIdA));
		return Result;
	}
	if (!NodeB)
	{
		Result->SetStringField(TEXT("error"), FString::Printf(TEXT("Target node '%s' not found."), *NodeIdB));
		return Result;
	}

	UEdGraphPin* PinA = NodeA->FindPin(FName(*PinNameA));
	if (!PinA || PinA->Direction != EGPD_Output)
	{
		TArray<FString> Candidates;
		PinA = FindBluecodePin(NodeA, PinNameA, EGPD_Output, &Candidates);
		if (!PinA && Candidates.Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> CandidateValues;
			for (const FString& Candidate : Candidates)
			{
				CandidateValues.Add(MakeShared<FJsonValueString>(Candidate));
			}
			Result->SetArrayField(TEXT("source_pin_candidates"), CandidateValues);
		}
	}

	UEdGraphPin* PinB = NodeB->FindPin(FName(*PinNameB));
	if (!PinB || PinB->Direction != EGPD_Input)
	{
		TArray<FString> Candidates;
		PinB = FindBluecodePin(NodeB, PinNameB, EGPD_Input, &Candidates);
		if (!PinB && Candidates.Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> CandidateValues;
			for (const FString& Candidate : Candidates)
			{
				CandidateValues.Add(MakeShared<FJsonValueString>(Candidate));
			}
			Result->SetArrayField(TEXT("target_pin_candidates"), CandidateValues);
		}
	}
	if (!PinA)
	{
		Result->SetStringField(TEXT("error"), FString::Printf(TEXT("Source pin '%s' not found on '%s'."), *PinNameA, *NodeIdA));
		return Result;
	}
	if (!PinB)
	{
		Result->SetStringField(TEXT("error"), FString::Printf(TEXT("Target pin '%s' not found on '%s'."), *PinNameB, *NodeIdB));
		return Result;
	}

	if (PinA->LinkedTo.Contains(PinB))
	{
		Result->SetBoolField(TEXT("success"), true);
		Result->SetBoolField(TEXT("existed"), true);
		return Result;
	}

	const FPinConnectionResponse Response = Graph->GetSchema()->CanCreateConnection(PinA, PinB);
	Result->SetNumberField(TEXT("connection_response"), static_cast<int32>(Response.Response.GetValue()));
	if (Response.Response == CONNECT_RESPONSE_DISALLOW)
	{
		Result->SetStringField(TEXT("error"), Response.Message.ToString());
		return Result;
	}
	if (Response.Response != CONNECT_RESPONSE_MAKE &&
		Response.Response != CONNECT_RESPONSE_MAKE_WITH_CONVERSION_NODE &&
		Response.Response != CONNECT_RESPONSE_MAKE_WITH_PROMOTION)
	{
		Result->SetStringField(TEXT("error"), FString::Printf(
			TEXT("Connection would replace or break existing links (%s). bluecode_connect is union-only; use explicit bluecode_delete before reconnecting."),
			*Response.Message.ToString()));
		return Result;
	}

	if (Graph->GetSchema()->TryCreateConnection(PinA, PinB))
	{
		Result->SetBoolField(TEXT("success"), true);
		Result->SetBoolField(TEXT("existed"), false);
	}
	else
	{
		Result->SetStringField(TEXT("error"), TEXT("TryCreateConnection returned false."));
	}
	return Result;
}

TSharedPtr<FJsonObject> UUmgBlueprintFunctionSubsystem::ApplyBluecodeConnect(UEdGraph* Graph, const TSharedPtr<FJsonObject>& Params)
{
	if (!Graph || !Params.IsValid())
	{
		TSharedPtr<FJsonObject> Error = MakeShared<FJsonObject>();
		Error->SetBoolField(TEXT("success"), false);
		Error->SetStringField(TEXT("error"), TEXT("Missing graph or params for bluecode_connect."));
		return Error;
	}

	TArray<TSharedPtr<FJsonValue>> Operations;
	TArray<TSharedPtr<FJsonValue>> Failures;
	int32 AppliedCount = 0;

	const TSharedPtr<FJsonObject>* AliasObj = nullptr;
	Params->TryGetObjectField(TEXT("aliases"), AliasObj);

	auto ResolveAlias = [&](const FString& MaybeAlias)
	{
		FString Resolved = MaybeAlias;
		if (AliasObj && AliasObj->IsValid())
		{
			FString AliasValue;
			if ((*AliasObj)->TryGetStringField(MaybeAlias, AliasValue) && !AliasValue.IsEmpty())
			{
				Resolved = AliasValue;
			}
		}
		return Resolved;
	};

	auto ResolveEndpointAliases = [&](const TSharedPtr<FJsonObject>& ConnectObj)
	{
		if (!ConnectObj.IsValid())
		{
			return;
		}

		auto ResolveEndpointField = [&](const TCHAR* Field)
		{
			FString Endpoint;
			if (!ConnectObj->TryGetStringField(Field, Endpoint) || Endpoint.IsEmpty())
			{
				return;
			}
			FString NodePart;
			FString PinPart;
			if (Endpoint.Split(TEXT(":"), &NodePart, &PinPart))
			{
				NodePart.TrimStartAndEndInline();
				PinPart.TrimStartAndEndInline();
				ConnectObj->SetStringField(Field, FString::Printf(TEXT("%s:%s"), *ResolveAlias(NodePart), *PinPart));
			}
			else
			{
				Endpoint.TrimStartAndEndInline();
				ConnectObj->SetStringField(Field, ResolveAlias(Endpoint));
			}
		};

		auto ResolveNodeField = [&](const TCHAR* Field)
		{
			FString NodeName;
			if (ConnectObj->TryGetStringField(Field, NodeName) && !NodeName.IsEmpty())
			{
				ConnectObj->SetStringField(Field, ResolveAlias(NodeName));
			}
		};

		ResolveEndpointField(TEXT("source"));
		ResolveEndpointField(TEXT("from"));
		ResolveEndpointField(TEXT("target"));
		ResolveEndpointField(TEXT("to"));
		ResolveNodeField(TEXT("source_node"));
		ResolveNodeField(TEXT("from_node"));
		ResolveNodeField(TEXT("target_node"));
		ResolveNodeField(TEXT("to_node"));
		ResolveNodeField(TEXT("nodeIdA"));
		ResolveNodeField(TEXT("nodeIdB"));
	};

	auto AddOperation = [&](const TSharedPtr<FJsonObject>& ConnectObj)
	{
		ResolveEndpointAliases(ConnectObj);
		TSharedPtr<FJsonObject> Operation = MakeShared<FJsonObject>();
		Operation->SetObjectField(TEXT("connect"), ConnectObj);
		TSharedPtr<FJsonObject> ResultObj = ConnectPins(Graph, ConnectObj);
		if (ResultObj.IsValid())
		{
			Operation->SetObjectField(TEXT("result"), ResultObj);
			bool bOk = false;
			ResultObj->TryGetBoolField(TEXT("success"), bOk);
			if (bOk)
			{
				++AppliedCount;
			}
			else
			{
				Failures.Add(MakeShared<FJsonValueObject>(ResultObj));
			}
		}
		else
		{
			TSharedPtr<FJsonObject> Failure = MakeShared<FJsonObject>();
			Failure->SetBoolField(TEXT("success"), false);
			Failure->SetStringField(TEXT("error"), TEXT("ConnectPins returned no result."));
			Operation->SetObjectField(TEXT("result"), Failure);
			Failures.Add(MakeShared<FJsonValueObject>(Failure));
		}
		Operations.Add(MakeShared<FJsonValueObject>(Operation));
	};

	const TArray<TSharedPtr<FJsonValue>>* Connects = nullptr;
	if (Params->TryGetArrayField(TEXT("connects"), Connects) && Connects)
	{
		for (const TSharedPtr<FJsonValue>& ConnectValue : *Connects)
		{
			TSharedPtr<FJsonObject> ConnectObj;
			if (ConnectValue.IsValid() && ConnectValue->Type == EJson::Object)
			{
				ConnectObj = ConnectValue->AsObject();
			}
			else if (ConnectValue.IsValid())
			{
				const FString Spec = ConnectValue->AsString();
				FString Source;
				FString Target;
				if (Spec.Split(TEXT("->"), &Source, &Target))
				{
					ConnectObj = MakeShared<FJsonObject>();
					Source.TrimStartAndEndInline();
					Target.TrimStartAndEndInline();
					ConnectObj->SetStringField(TEXT("source"), Source);
					ConnectObj->SetStringField(TEXT("target"), Target);
				}
			}

			if (ConnectObj.IsValid())
			{
				AddOperation(ConnectObj);
			}
			else
			{
				TSharedPtr<FJsonObject> Failure = MakeShared<FJsonObject>();
				Failure->SetBoolField(TEXT("success"), false);
				Failure->SetStringField(TEXT("error"), TEXT("bluecode_connect entries must be objects or 'SourceNode:Pin -> TargetNode:Pin' strings."));
				Failures.Add(MakeShared<FJsonValueObject>(Failure));
			}
		}
	}
	else
	{
		AddOperation(Params);
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), Failures.Num() == 0);
	Result->SetNumberField(TEXT("applied_count"), AppliedCount);
	Result->SetNumberField(TEXT("deleted_count"), 0);
	Result->SetArrayField(TEXT("operations"), Operations);
	Result->SetArrayField(TEXT("failures"), Failures);
	return Result;
}

TSharedPtr<FJsonObject> UUmgBlueprintFunctionSubsystem::GetEvents(UBlueprint* Blueprint, const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    TArray<TSharedPtr<FJsonValue>> WidgetsArray;
    TArray<TSharedPtr<FJsonValue>> AvailableEventsArray;
    TArray<TSharedPtr<FJsonValue>> BoundEventsArray;
    TSet<FString> BoundTargets;
    TSet<FString> SeenAvailable;
    TSet<FString> SeenBound;

    if (!Blueprint)
    {
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(TEXT("error"), TEXT("Invalid Blueprint"));
        return Result;
    }

    UWidgetBlueprint* WidgetBlueprint = Cast<UWidgetBlueprint>(Blueprint);
    FString ScopeWidgetName;
    Params->TryGetStringField(TEXT("scope_widget"), ScopeWidgetName);
    if (ScopeWidgetName.IsEmpty())
    {
        Params->TryGetStringField(TEXT("widgetName"), ScopeWidgetName);
    }

    UWidget* ScopeWidget = nullptr;
    if (!ScopeWidgetName.IsEmpty() && WidgetBlueprint && WidgetBlueprint->WidgetTree)
    {
        ScopeWidget = WidgetBlueprint->WidgetTree->FindWidget(FName(*ScopeWidgetName));
        if (!ScopeWidget)
        {
            Result->SetStringField(TEXT("scope_warning"), FString::Printf(TEXT("Focused widget '%s' was not found; scanned all widgets."), *ScopeWidgetName));
            ScopeWidgetName.Empty();
        }
    }

    auto IsInScope = [&](UWidget* Widget) -> bool
    {
        if (!Widget || !ScopeWidget)
        {
            return true;
        }
        if (Widget == ScopeWidget)
        {
            return true;
        }
        for (UPanelWidget* Parent = Widget->GetParent(); Parent; Parent = Parent->GetParent())
        {
            if (Parent == ScopeWidget)
            {
                return true;
            }
        }
        return false;
    };

    auto AddBoundEvent = [&](const FString& Kind, const FString& WidgetName, const FString& EventName, UEdGraphNode* Node)
    {
        if (EventName.IsEmpty() || !Node)
        {
            return;
        }
        const FString Target = WidgetName.IsEmpty() ? EventName : FString::Printf(TEXT("%s.%s"), *WidgetName, *EventName);
        const FString Key = FString::Printf(TEXT("%s|%s|%s"), *Kind, *Target, *Node->NodeGuid.ToString());
        if (SeenBound.Contains(Key))
        {
            return;
        }
        SeenBound.Add(Key);
        BoundTargets.Add(Target);

        TSharedPtr<FJsonObject> EventObj = MakeShared<FJsonObject>();
        EventObj->SetStringField(TEXT("kind"), Kind);
        EventObj->SetStringField(TEXT("widget"), WidgetName);
        EventObj->SetStringField(TEXT("event"), EventName);
        EventObj->SetStringField(TEXT("target"), Target);
        EventObj->SetStringField(TEXT("node_id"), Node->NodeGuid.ToString());
        EventObj->SetStringField(TEXT("title"), Node->GetNodeTitle(ENodeTitleType::ListView).ToString());
        BoundEventsArray.Add(MakeShared<FJsonValueObject>(EventObj));
    };

    if (UEdGraph* EventGraph = FBlueprintEditorUtils::FindEventGraph(Blueprint))
    {
        for (UEdGraphNode* Node : EventGraph->Nodes)
        {
            if (UK2Node_ComponentBoundEvent* BoundEvent = Cast<UK2Node_ComponentBoundEvent>(Node))
            {
                AddBoundEvent(TEXT("component_event"), BoundEvent->ComponentPropertyName.ToString(), BoundEvent->DelegatePropertyName.ToString(), Node);
            }
            else if (UK2Node_CustomEvent* CustomEvent = Cast<UK2Node_CustomEvent>(Node))
            {
                AddBoundEvent(TEXT("custom_event"), TEXT(""), CustomEvent->CustomFunctionName.ToString(), Node);
            }
            else if (UK2Node_Event* EventNode = Cast<UK2Node_Event>(Node))
            {
                AddBoundEvent(TEXT("event"), TEXT(""), EventNode->EventReference.GetMemberName().ToString(), Node);
            }
        }
    }

    if (WidgetBlueprint && WidgetBlueprint->WidgetTree)
    {
        WidgetBlueprint->WidgetTree->ForEachWidget([&](UWidget* Widget)
        {
            if (!Widget || !IsInScope(Widget))
            {
                return;
            }

            const FString WidgetName = Widget->GetName();
            UClass* WidgetClass = Widget->GetClass();
            FObjectProperty* ComponentProp = Blueprint->SkeletonGeneratedClass
                ? FindFProperty<FObjectProperty>(Blueprint->SkeletonGeneratedClass, *WidgetName)
                : nullptr;
            UClass* DelegateClass = ComponentProp ? ComponentProp->PropertyClass : nullptr;
            if (!DelegateClass)
            {
                return;
            }

            TSharedPtr<FJsonObject> WidgetObj = MakeShared<FJsonObject>();
            WidgetObj->SetStringField(TEXT("widget"), WidgetName);
            WidgetObj->SetStringField(TEXT("class"), WidgetClass ? WidgetClass->GetName() : TEXT(""));
            WidgetObj->SetStringField(TEXT("delegate_class"), DelegateClass->GetName());
            WidgetsArray.Add(MakeShared<FJsonValueObject>(WidgetObj));

            for (TFieldIterator<FMulticastDelegateProperty> It(DelegateClass, EFieldIteratorFlags::IncludeSuper); It; ++It)
            {
                FMulticastDelegateProperty* DelegateProp = *It;
                if (!DelegateProp)
                {
                    continue;
                }

                const FString EventName = DelegateProp->GetName();
                const FString Target = FString::Printf(TEXT("%s.%s"), *WidgetName, *EventName);
                if (SeenAvailable.Contains(Target))
                {
                    continue;
                }
                SeenAvailable.Add(Target);

                TSharedPtr<FJsonObject> EventObj = MakeShared<FJsonObject>();
                EventObj->SetStringField(TEXT("widget"), WidgetName);
                EventObj->SetStringField(TEXT("class"), WidgetClass ? WidgetClass->GetName() : TEXT(""));
                EventObj->SetStringField(TEXT("event"), EventName);
                EventObj->SetStringField(TEXT("target"), Target);
                EventObj->SetStringField(TEXT("set_function"), FString::Printf(TEXT("bluecode_set_function(\"%s\")"), *Target));
                EventObj->SetStringField(TEXT("source"), TEXT("reflection"));
                EventObj->SetBoolField(TEXT("bound"), BoundTargets.Contains(Target));

                TArray<TSharedPtr<FJsonValue>> ParametersArray;
                if (UFunction* Signature = DelegateProp->SignatureFunction)
                {
                    for (TFieldIterator<FProperty> ParamIt(Signature); ParamIt; ++ParamIt)
                    {
                        FProperty* Param = *ParamIt;
                        if (!Param || !Param->HasAnyPropertyFlags(CPF_Parm) || Param->HasAnyPropertyFlags(CPF_ReturnParm))
                        {
                            continue;
                        }

                        TSharedPtr<FJsonObject> ParamObj = MakeShared<FJsonObject>();
                        ParamObj->SetStringField(TEXT("name"), Param->GetName());
                        ParamObj->SetStringField(TEXT("type"), Param->GetCPPType());
                        ParametersArray.Add(MakeShared<FJsonValueObject>(ParamObj));
                    }
                }
                if (ParametersArray.Num() > 0)
                {
                    EventObj->SetArrayField(TEXT("parameters"), ParametersArray);
                }

                AvailableEventsArray.Add(MakeShared<FJsonValueObject>(EventObj));
            }
        });
    }

    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("scope"), ScopeWidgetName.IsEmpty() ? TEXT("all_widgets") : TEXT("target_widget"));
    if (!ScopeWidgetName.IsEmpty())
    {
        Result->SetStringField(TEXT("root_widget"), ScopeWidgetName);
    }
    Result->SetStringField(TEXT("bound_scope"), TEXT("EventGraph"));
    Result->SetStringField(TEXT("available_events_source"), TEXT("reflection"));
    Result->SetBoolField(TEXT("full_event_graph_scan"), true);
    Result->SetArrayField(TEXT("widgets"), WidgetsArray);
    Result->SetArrayField(TEXT("available_events"), AvailableEventsArray);
    Result->SetArrayField(TEXT("bound_events"), BoundEventsArray);
    return Result;
}

TSharedPtr<FJsonObject> UUmgBlueprintFunctionSubsystem::GetNodes(UEdGraph* Graph)
{
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    TArray<TSharedPtr<FJsonValue>> NodesArray;

    // CONTEXT FILTERING
    // If we are in the Event Graph (which is huge), and we have a Cursor,
    // we should only return the "execution chain" connected to the cursor.
    // This solves the problem of returning thousands of unrelated nodes.

    TSet<UEdGraphNode*> RelevantNodes;

    if (Graph->GetName() == TEXT("EventGraph") && GEditor)
    {
         UUmgAttentionSubsystem* AttentionSystem = GEditor->GetEditorSubsystem<UUmgAttentionSubsystem>();
         if (AttentionSystem)
         {
             FString CursorId = AttentionSystem->GetCursorNode();
             UEdGraphNode* StartNode = FindNodeByIdOrName(Graph, CursorId);

             if (StartNode)
             {
                 // BFS Traversal
                 TArray<UEdGraphNode*> Queue;
                 Queue.Add(StartNode);
                 RelevantNodes.Add(StartNode);

                 int32 Head = 0;
                 while(Head < Queue.Num())
                 {
                     UEdGraphNode* Current = Queue[Head++];

                     // Limit depth/count to prevent infinite scan? 50 nodes seems reasonable for a context.
                     if (RelevantNodes.Num() > 50) break;

                     for (UEdGraphPin* Pin : Current->Pins)
                     {
                         for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
                         {
                             UEdGraphNode* Neighbor = LinkedPin->GetOwningNode();
                             if (!RelevantNodes.Contains(Neighbor))
                             {
                                 RelevantNodes.Add(Neighbor);
                                 Queue.Add(Neighbor);
                             }
                         }
                     }
                 }
             }
         }
    }

    // If no context found or not EventGraph, return all (or filtered list if RelevantNodes populated)
    const TArray<UEdGraphNode*>& TargetList = (RelevantNodes.Num() > 0) ? RelevantNodes.Array() : Graph->Nodes;

    for (UEdGraphNode* Node : TargetList)
    {
        TSharedPtr<FJsonObject> NodeObj = MakeShared<FJsonObject>();
        NodeObj->SetStringField(TEXT("id"), Node->NodeGuid.ToString());
        NodeObj->SetStringField(TEXT("name"), Node->GetName());
        NodeObj->SetStringField(TEXT("class"), Node->GetClass()->GetName());
        NodeObj->SetStringField(TEXT("title"), Node->GetNodeTitle(ENodeTitleType::ListView).ToString());

        FString Kind = TEXT("node");
        FString MemberName;
        if (UK2Node_ComponentBoundEvent* BoundEventNode = Cast<UK2Node_ComponentBoundEvent>(Node))
        {
            Kind = TEXT("component_event");
            MemberName = BoundEventNode->GetNodeTitle(ENodeTitleType::ListView).ToString();
        }
        else if (UK2Node_CustomEvent* CustomEventNode = Cast<UK2Node_CustomEvent>(Node))
        {
            Kind = TEXT("custom_event");
            MemberName = CustomEventNode->CustomFunctionName.ToString();
        }
        else if (UK2Node_Event* EventNode = Cast<UK2Node_Event>(Node))
        {
            Kind = TEXT("event");
            MemberName = EventNode->EventReference.GetMemberName().ToString();
        }
        else if (Cast<UK2Node_FunctionEntry>(Node))
        {
            Kind = TEXT("function_entry");
            MemberName = Node->GetNodeTitle(ENodeTitleType::ListView).ToString();
        }
        else if (UK2Node_CallFunction* CallNode = Cast<UK2Node_CallFunction>(Node))
        {
            Kind = TEXT("call");
            MemberName = CallNode->FunctionReference.GetMemberName().ToString();
            if (MemberName.IsEmpty())
            {
                if (UFunction* Function = CallNode->GetTargetFunction())
                {
                    MemberName = Function->GetName();
                }
            }
        }
        else if (UK2Node_VariableGet* GetNode = Cast<UK2Node_VariableGet>(Node))
        {
            Kind = TEXT("variable_get");
            MemberName = GetNode->VariableReference.GetMemberName().ToString();
        }
        else if (UK2Node_VariableSet* SetNode = Cast<UK2Node_VariableSet>(Node))
        {
            Kind = TEXT("variable_set");
            MemberName = SetNode->VariableReference.GetMemberName().ToString();
        }
        else if (Cast<UK2Node_IfThenElse>(Node))
        {
            Kind = TEXT("branch");
            MemberName = TEXT("Branch");
        }
        else if (Cast<UK2Node_ExecutionSequence>(Node))
        {
            Kind = TEXT("sequence");
            MemberName = TEXT("Sequence");
        }

        NodeObj->SetStringField(TEXT("kind"), Kind);
        if (!MemberName.IsEmpty())
        {
            NodeObj->SetStringField(TEXT("member"), MemberName);
        }

        bool bIsExec = false;
        TArray<TSharedPtr<FJsonValue>> InputsArray;
        TArray<TSharedPtr<FJsonValue>> OutputsArray;
        TArray<TSharedPtr<FJsonValue>> ExecNextArray;
        TArray<TSharedPtr<FJsonValue>> DataDependenciesArray;
        TSet<FString> ExecNextSeen;

        for (UEdGraphPin* Pin : Node->Pins)
        {
            if (!Pin || Pin->bHidden)
            {
                continue;
            }

            const bool bPinIsExec = Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec;
            bIsExec = bIsExec || bPinIsExec;

            TSharedPtr<FJsonObject> PinObj = MakeBluecodePinHintJson(Pin);
            if (Pin->PinType.PinSubCategoryObject.IsValid())
            {
                PinObj->SetStringField(TEXT("subType"), Pin->PinType.PinSubCategoryObject->GetName());
                PinObj->SetStringField(TEXT("subTypePath"), Pin->PinType.PinSubCategoryObject->GetPathName());
            }

            TArray<TSharedPtr<FJsonValue>> LinksArray;
            for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
            {
                if (!LinkedPin || !LinkedPin->GetOwningNode())
                {
                    continue;
                }

                UEdGraphNode* LinkedNode = LinkedPin->GetOwningNode();
                TSharedPtr<FJsonObject> LinkObj = MakeShared<FJsonObject>();
                LinkObj->SetStringField(TEXT("node_id"), LinkedNode->NodeGuid.ToString());
                LinkObj->SetStringField(TEXT("node"), LinkedNode->GetName());
                LinkObj->SetStringField(TEXT("title"), LinkedNode->GetNodeTitle(ENodeTitleType::ListView).ToString());
                LinkObj->SetStringField(TEXT("pin"), LinkedPin->PinName.ToString());

                FString LinkedMember;
                if (UK2Node_CallFunction* LinkedCall = Cast<UK2Node_CallFunction>(LinkedNode))
                {
                    LinkedMember = LinkedCall->FunctionReference.GetMemberName().ToString();
                }
                else if (UK2Node_VariableGet* LinkedGet = Cast<UK2Node_VariableGet>(LinkedNode))
                {
                    LinkedMember = LinkedGet->VariableReference.GetMemberName().ToString();
                }
                else if (UK2Node_VariableSet* LinkedSet = Cast<UK2Node_VariableSet>(LinkedNode))
                {
                    LinkedMember = LinkedSet->VariableReference.GetMemberName().ToString();
                }
                if (!LinkedMember.IsEmpty())
                {
                    LinkObj->SetStringField(TEXT("member"), LinkedMember);
                }

                LinksArray.Add(MakeShared<FJsonValueObject>(LinkObj));

                if (Pin->Direction == EGPD_Output && bPinIsExec)
                {
                    const FString NextId = LinkedNode->NodeGuid.ToString();
                    if (!ExecNextSeen.Contains(NextId))
                    {
                        ExecNextSeen.Add(NextId);
                        ExecNextArray.Add(MakeShared<FJsonValueString>(NextId));
                    }
                }
                else if (Pin->Direction == EGPD_Input && !bPinIsExec)
                {
                    TSharedPtr<FJsonObject> DepObj = MakeShared<FJsonObject>();
                    DepObj->SetStringField(TEXT("pin"), Pin->PinName.ToString());
                    DepObj->SetStringField(TEXT("node_id"), LinkedNode->NodeGuid.ToString());
                    DepObj->SetStringField(TEXT("title"), LinkedNode->GetNodeTitle(ENodeTitleType::ListView).ToString());
                    if (!LinkedMember.IsEmpty())
                    {
                        DepObj->SetStringField(TEXT("name"), LinkedMember);
                    }
                    DataDependenciesArray.Add(MakeShared<FJsonValueObject>(DepObj));
                }
            }

            if (LinksArray.Num() > 0)
            {
                PinObj->SetArrayField(TEXT("linked_to"), LinksArray);
            }

            if (Pin->Direction == EGPD_Input)
            {
                InputsArray.Add(MakeShared<FJsonValueObject>(PinObj));
            }
            else
            {
                OutputsArray.Add(MakeShared<FJsonValueObject>(PinObj));
            }
        }
        NodeObj->SetBoolField(TEXT("isExec"), bIsExec);
        NodeObj->SetArrayField(TEXT("inputs"), InputsArray);
        NodeObj->SetArrayField(TEXT("outputs"), OutputsArray);
        NodeObj->SetArrayField(TEXT("exec_next"), ExecNextArray);
        NodeObj->SetArrayField(TEXT("data_dependencies"), DataDependenciesArray);

        NodesArray.Add(MakeShared<FJsonValueObject>(NodeObj));
    }

    Result->SetArrayField(TEXT("nodes"), NodesArray);
    Result->SetBoolField(TEXT("success"), true);
    return Result;
}

TSharedPtr<FJsonObject> UUmgBlueprintFunctionSubsystem::DeleteNode(UBlueprint* Blueprint, UEdGraph* Graph, const TSharedPtr<FJsonObject>& Params)
{
    FString NodeId;
    Params->TryGetStringField(TEXT("nodeId"), NodeId);

    UEdGraphNode* NodeToDelete = FindNodeByIdOrName(Graph, NodeId);
    if (NodeToDelete)
    {
         // If we are deleting the CURRENT cursor node, we should step back
         if (GEditor)
         {
             UUmgAttentionSubsystem* AttentionSystem = GEditor->GetEditorSubsystem<UUmgAttentionSubsystem>();
             if (AttentionSystem && AttentionSystem->GetCursorNode() == NodeId)
             {
                 // Find prev node?
                 UEdGraphPin* ExecIn = NodeToDelete->FindPin(UEdGraphSchema_K2::PN_Execute);
                 if (ExecIn && ExecIn->LinkedTo.Num() > 0)
                 {
                     UEdGraphNode* PrevNode = ExecIn->LinkedTo[0]->GetOwningNode();
                     // Return new cursor
                     TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
                     Result->SetBoolField(TEXT("success"), true);
                     Result->SetStringField(TEXT("newCursorNode"), PrevNode->NodeGuid.ToString());

                     FBlueprintEditorUtils::RemoveNode(Blueprint, NodeToDelete, true);
                     return Result;
                 }
             }
         }

        FBlueprintEditorUtils::RemoveNode(Blueprint, NodeToDelete, true);

        TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
        Result->SetBoolField(TEXT("success"), true);
        return Result;
    }
    return nullptr;
}

TSharedPtr<FJsonObject> UUmgBlueprintFunctionSubsystem::SetNodeProperty(UBlueprint* Blueprint, UEdGraph* Graph, const TSharedPtr<FJsonObject>& Params)
{
    if (!Graph || !Params.IsValid())
    {
        TSharedPtr<FJsonObject> Error = MakeShared<FJsonObject>();
        Error->SetBoolField(TEXT("success"), false);
        Error->SetStringField(TEXT("error"), TEXT("Missing graph or params for set_node_property."));
        return Error;
    }

    FString NodeId;
    Params->TryGetStringField(TEXT("nodeId"), NodeId);
    if (NodeId.IsEmpty()) Params->TryGetStringField(TEXT("node_id"), NodeId);
    if (NodeId.IsEmpty()) Params->TryGetStringField(TEXT("id"), NodeId);
    if (NodeId.IsEmpty()) Params->TryGetStringField(TEXT("name"), NodeId);
    if (NodeId.IsEmpty())
    {
        TSharedPtr<FJsonObject> Error = MakeShared<FJsonObject>();
        Error->SetBoolField(TEXT("success"), false);
        Error->SetStringField(TEXT("error"), TEXT("set_node_property requires nodeId/node_id/id/name."));
        return Error;
    }

    UEdGraphNode* Node = FindNodeByIdOrName(Graph, NodeId);
    if (!Node)
    {
        TSharedPtr<FJsonObject> Error = MakeShared<FJsonObject>();
        Error->SetBoolField(TEXT("success"), false);
        Error->SetStringField(TEXT("error"), FString::Printf(TEXT("Node '%s' not found."), *NodeId));
        return Error;
    }

    TSharedPtr<FJsonObject> Payload = MakeShared<FJsonObject>(*Params);
    const TSharedPtr<FJsonObject>* PropertiesObj = nullptr;
    if (Params->TryGetObjectField(TEXT("properties"), PropertiesObj) && PropertiesObj && PropertiesObj->IsValid())
    {
        Payload->SetObjectField(TEXT("nodeProperties"), *PropertiesObj);
    }
    else if (Params->TryGetObjectField(TEXT("node_properties"), PropertiesObj) && PropertiesObj && PropertiesObj->IsValid())
    {
        Payload->SetObjectField(TEXT("nodeProperties"), *PropertiesObj);
    }

    TSharedPtr<FJsonObject> Result = ApplyBluecodeNodeProperties(Node, Payload);
    Result->SetStringField(TEXT("nodeId"), Node->NodeGuid.ToString());
    Result->SetStringField(TEXT("nodeName"), Node->GetName());
    AddBluecodeNodeResultFields(Node, Result);
    return Result;
}

TSharedPtr<FJsonObject> UUmgBlueprintFunctionSubsystem::AddVariable(UBlueprint* Blueprint, const TSharedPtr<FJsonObject>& Params)
{
    FString Name, Type, SubType;
    Params->TryGetStringField(TEXT("name"), Name);
    Params->TryGetStringField(TEXT("type"), Type);
    Params->TryGetStringField(TEXT("subType"), SubType);

    if (Name.IsEmpty() || Type.IsEmpty()) return nullptr;

    FEdGraphPinType PinType;
    PinType.PinCategory = FName(*Type);
    if (!SubType.IsEmpty())
    {
         UObject* SubObj = ResolveUClass(SubType); // Simplified resolution
         if (SubObj) PinType.PinSubCategoryObject = SubObj;
    }

    const FName VarName(*Name);
    const int32 ExistingIndex = FBlueprintEditorUtils::FindNewVariableIndex(Blueprint, VarName);
    if (ExistingIndex != INDEX_NONE)
    {
        const FBPVariableDescription& ExistingVar = Blueprint->NewVariables[ExistingIndex];
        const FString ExistingSubType = ExistingVar.VarType.PinSubCategoryObject.IsValid()
            ? ExistingVar.VarType.PinSubCategoryObject->GetName()
            : TEXT("");
        const FString RequestedSubType = PinType.PinSubCategoryObject.IsValid()
            ? PinType.PinSubCategoryObject->GetName()
            : TEXT("");

        TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
        if (ExistingVar.VarType.PinCategory == PinType.PinCategory && ExistingSubType == RequestedSubType)
        {
            Result->SetBoolField(TEXT("success"), true);
            Result->SetBoolField(TEXT("existed"), true);
            Result->SetStringField(TEXT("name"), Name);
            return Result;
        }

        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(TEXT("error"), FString::Printf(
            TEXT("Variable '%s' already exists with type '%s:%s'; refusing implicit type replacement with '%s:%s'. Use explicit delete/migration."),
            *Name,
            *ExistingVar.VarType.PinCategory.ToString(),
            *ExistingSubType,
            *PinType.PinCategory.ToString(),
            *RequestedSubType));
        return Result;
    }

    const bool bAdded = FBlueprintEditorUtils::AddMemberVariable(Blueprint, VarName, PinType);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), bAdded);
    Result->SetBoolField(TEXT("existed"), false);
    Result->SetStringField(TEXT("name"), Name);
    return Result;
}

TSharedPtr<FJsonObject> UUmgBlueprintFunctionSubsystem::DeleteVariable(UBlueprint* Blueprint, const TSharedPtr<FJsonObject>& Params)
{
    FString Name;
    Params->TryGetStringField(TEXT("name"), Name);
    if (Name.IsEmpty()) return nullptr;

    FBlueprintEditorUtils::RemoveMemberVariable(Blueprint, FName(*Name));

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    return Result;
}

TSharedPtr<FJsonObject> UUmgBlueprintFunctionSubsystem::GetVariables(UBlueprint* Blueprint)
{
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    TArray<TSharedPtr<FJsonValue>> VarsArray;

    for (const FBPVariableDescription& Var : Blueprint->NewVariables)
    {
        TSharedPtr<FJsonObject> VarObj = MakeShared<FJsonObject>();
        VarObj->SetStringField(TEXT("name"), Var.VarName.ToString());
        VarObj->SetStringField(TEXT("type"), Var.VarType.PinCategory.ToString());
        if (Var.VarType.PinSubCategoryObject.IsValid())
        {
            VarObj->SetStringField(TEXT("subType"), Var.VarType.PinSubCategoryObject->GetName());
        }
        VarsArray.Add(MakeShared<FJsonValueObject>(VarObj));
    }

    Result->SetArrayField(TEXT("variables"), VarsArray);
    Result->SetBoolField(TEXT("success"), true);
    return Result;
}

TSharedPtr<FJsonObject> UUmgBlueprintFunctionSubsystem::ReadBluecodeFunction(UEdGraph* Graph, const TSharedPtr<FJsonObject>& Params)
{
    TArray<FString> Lines;
    TSharedPtr<FJsonObject> ActionHints = MakeShared<FJsonObject>();
    TSharedPtr<FJsonObject> ExpressionHints = MakeShared<FJsonObject>();
    TSharedPtr<FJsonObject> SourceMap = MakeShared<FJsonObject>();
    TArray<TSharedPtr<FJsonValue>> ActionHintsByLine;
    Lines.Add(TEXT("main()"));
    int32 StatementCount = 0;

    FString Detail;
    Params->TryGetStringField(TEXT("detail"), Detail);
    if (Detail.IsEmpty())
    {
        Detail = TEXT("semantic");
    }
    const bool bRoundtripDetail =
        Detail.Equals(TEXT("roundtrip"), ESearchCase::IgnoreCase) ||
        Detail.Equals(TEXT("debug"), ESearchCase::IgnoreCase);
    const bool bDebugDetail = Detail.Equals(TEXT("debug"), ESearchCase::IgnoreCase);
    bool bIncludeReconstructionHints = bRoundtripDetail;
    Params->TryGetBoolField(TEXT("include_reconstruction_hints"), bIncludeReconstructionHints);
    bIncludeReconstructionHints = bRoundtripDetail && bIncludeReconstructionHints;
    if (!Detail.Equals(TEXT("semantic"), ESearchCase::IgnoreCase) &&
        !Detail.Equals(TEXT("roundtrip"), ESearchCase::IgnoreCase) &&
        !bDebugDetail)
    {
        TSharedPtr<FJsonObject> Error = MakeShared<FJsonObject>();
        Error->SetBoolField(TEXT("success"), false);
        Error->SetStringField(TEXT("error_code"), TEXT("invalid_bluecode_detail"));
        Error->SetStringField(TEXT("error"), TEXT("detail must be 'semantic', 'roundtrip', or 'debug'."));
        return Error;
    }
    bool bIncludeConnections = false;
    Params->TryGetBoolField(TEXT("include_connections"), bIncludeConnections);
    bIncludeConnections = bIncludeConnections || bDebugDetail;

    auto AddStatementLine = [&](const FString& Statement, UEdGraphNode* SourceNode, const bool bIncludeActionHint)
    {
        const int32 LineNumber = Lines.Num() + 1;
        const FString SourcePath = FString::Printf(TEXT("/body/%d"), StatementCount++);
        Lines.Add(FString::Printf(TEXT("  %s"), *Statement));
        if (bRoundtripDetail && SourceNode)
        {
            TSharedPtr<FJsonObject> Source = MakeShared<FJsonObject>();
            Source->SetStringField(TEXT("statement"), Statement);
            Source->SetStringField(TEXT("node_guid"), SourceNode->NodeGuid.ToString());
            Source->SetStringField(TEXT("node_id"), SourceNode->NodeGuid.ToString());
            Source->SetStringField(TEXT("semantic_key"), MakeBluecodeSemanticKey(SourceNode));
            Source->SetStringField(
                TEXT("node_class_path"),
                SourceNode->GetClass() ? SourceNode->GetClass()->GetPathName() : TEXT(""));
            if (const UK2Node_CallFunction* CallNode = Cast<UK2Node_CallFunction>(SourceNode))
            {
                TSharedPtr<FJsonObject> Descriptor = MakeShared<FJsonObject>();
                const UFunction* Function = CallNode->GetTargetFunction();
                Descriptor->SetStringField(TEXT("member_name"), CallNode->FunctionReference.GetMemberName().ToString());
                Descriptor->SetStringField(
                    TEXT("member_owner"),
                    Function && Function->GetOwnerClass() ? Function->GetOwnerClass()->GetPathName() : TEXT("self"));
                Source->SetStringField(
                    TEXT("member_class"),
                    Function && Function->GetOwnerClass() ? Function->GetOwnerClass()->GetPathName() : TEXT(""));
                Descriptor->SetStringField(TEXT("signature"), Function ? Function->GetPathName() : TEXT(""));
                Source->SetObjectField(TEXT("action_descriptor"), Descriptor);
            }
            SourceMap->SetObjectField(SourcePath, Source);
        }
        if (bIncludeReconstructionHints && bIncludeActionHint && SourceNode)
        {
            TSharedPtr<FJsonObject> Hint = MakeBluecodeRoundtripHint(Graph, SourceNode, Statement);
            ActionHints->SetObjectField(Statement, Hint);

            TSharedPtr<FJsonObject> LineHint = MakeShared<FJsonObject>();
            LineHint->SetNumberField(TEXT("line"), LineNumber);
            LineHint->SetStringField(TEXT("statement"), Statement);
            LineHint->SetObjectField(TEXT("hint"), Hint);
            ActionHintsByLine.Add(MakeShared<FJsonValueObject>(LineHint));
        }
    };

    if (Graph)
    {
        TArray<UEdGraphNode*> SortedNodes;
        for (UEdGraphNode* Node : Graph->Nodes)
        {
            if (Node)
            {
                SortedNodes.Add(Node);
            }
        }
        // Semantic read order must be layout-independent. Exec traversal below
        // supplies control-flow order; this key only stabilizes roots/floating nodes.
        SortedNodes.Sort([](const UEdGraphNode& A, const UEdGraphNode& B) {
            const FString AKey = MakeBluecodeSemanticKey(&A) + TEXT(":") + A.NodeGuid.ToString();
            const FString BKey = MakeBluecodeSemanticKey(&B) + TEXT(":") + B.NodeGuid.ToString();
            return AKey < BKey;
        });

        TSet<const UEdGraphNode*> DependencyNodes;
        for (UEdGraphNode* Node : SortedNodes)
        {
            CollectBluecodeDependencyNodes(Node, DependencyNodes);
        }

        for (UEdGraphNode* Node : SortedNodes)
        {
            if (!bIncludeReconstructionHints || !Node || !DependencyNodes.Contains(Node))
            {
                continue;
            }
            TSet<const UEdGraphNode*> Seen;
            const FString Expression = BluecodeExpressionFromNode(Node, Seen);
            if (Expression.IsEmpty())
            {
                continue;
            }
            TSharedPtr<FJsonObject> Hint = MakeBluecodeRoundtripHint(Graph, Node, Expression);
            ExpressionHints->SetObjectField(Expression, Hint);
            ActionHints->SetObjectField(Expression, Hint);
            if (const UK2Node_CallFunction* CallNode = Cast<UK2Node_CallFunction>(Node))
            {
                const FString FunctionName = CallNode->FunctionReference.GetMemberName().ToString();
                if (!FunctionName.IsEmpty())
                {
                    ActionHints->SetObjectField(FunctionName, Hint);
                }
            }
            else
            {
                const FString Title = BluecodeNodeTitle(Node);
                if (!Title.IsEmpty())
                {
                    ActionHints->SetObjectField(Title, Hint);
                }
            }
        }

        const TArray<UEdGraphNode*> ReadOrder = BuildBluecodeReadOrder(SortedNodes);
        for (UEdGraphNode* Node : ReadOrder)
        {
            if (!Node)
            {
                continue;
            }

            if (Cast<UK2Node_Event>(Node) || Cast<UK2Node_ComponentBoundEvent>(Node) || Cast<UK2Node_CustomEvent>(Node) || Cast<UK2Node_FunctionEntry>(Node))
            {
                continue;
            }

            if (DependencyNodes.Contains(Node))
            {
                continue;
            }

            if (UK2Node_VariableSet* SetNode = Cast<UK2Node_VariableSet>(Node))
            {
                FString Value;
                for (UEdGraphPin* Pin : SetNode->Pins)
                {
                    if (Pin && Pin->Direction == EGPD_Input && Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec && Pin->PinName != UEdGraphSchema_K2::PN_Self)
                    {
                        Value = BluecodePinValue(Pin);
                        if (!Value.IsEmpty())
                        {
                            break;
                        }
                    }
                }
                if (Value.IsEmpty())
                {
                    Value = TEXT("<unset>");
                }
                AddStatementLine(
                    FString::Printf(TEXT("%s = %s"), *SetNode->VariableReference.GetMemberName().ToString(), *Value),
                    Node,
                    false);
            }
            else if (Cast<UK2Node_IfThenElse>(Node))
            {
                FString Condition = TEXT("Condition");
                for (UEdGraphPin* Pin : Node->Pins)
                {
                    if (Pin && Pin->PinName.ToString().Equals(TEXT("Condition"), ESearchCase::IgnoreCase))
                    {
                        if (!Pin->DefaultValue.IsEmpty())
                        {
                            Condition = BluecodePinValue(Pin);
                        }
                        else if (Pin->LinkedTo.Num() > 0 && Pin->LinkedTo[0] && Pin->LinkedTo[0]->GetOwningNode())
                        {
                            Condition = BluecodePinValue(Pin);
                        }
                        break;
                    }
                }
                AddStatementLine(FString::Printf(TEXT("if %s:"), *Condition), Node, false);
            }
            else if (UK2Node_CallFunction* CallNode = Cast<UK2Node_CallFunction>(Node))
            {
                const FString FunctionName = CallNode->FunctionReference.GetMemberName().ToString();
                TArray<FString> Args;
                AddBluecodeInputArgs(Node, Args);
                AddStatementLine(
                    FString::Printf(TEXT("%s(%s)"), *FunctionName, *JoinBluecodeCallArgs(Args)),
                    Node,
                    false);
            }
            else if (UK2Node_VariableGet* GetNode = Cast<UK2Node_VariableGet>(Node))
            {
                AddStatementLine(GetNode->VariableReference.GetMemberName().ToString(), Node, false);
            }
            else
            {
                const FString Title = Node->GetNodeTitle(ENodeTitleType::ListView).ToString();
                if (!Title.IsEmpty())
                {
                    TArray<FString> Args;
                    Args.Add(QuoteBluecodeString(Title));
                    AddBluecodeInputArgs(Node, Args);
                    AddStatementLine(FString::Printf(
                        TEXT("%s(%s)"),
                        NodeHasExecPin(Node) ? TEXT("node") : TEXT("value"),
                        *JoinBluecodeCallArgs(Args)),
                        Node,
                        true);
                }
            }
        }
    }

    Lines.Add(TEXT("  end"));

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("protocol_version"), TEXT("2"));
    Result->SetStringField(TEXT("detail"), Detail.ToLower());
    Result->SetStringField(TEXT("function"), Graph ? Graph->GetName() : TEXT("EventGraph"));
    if (UBlueprint* Blueprint = ResolveBlueprintFromGraph(Graph))
    {
        Result->SetStringField(TEXT("target"), Blueprint->GetPathName());
    }
    Result->SetStringField(TEXT("base_revision"), MakeBluecodeGraphRevision(Graph));
    Result->SetStringField(TEXT("code"), FString::Join(Lines, TEXT("\n")));
    Result->SetNumberField(TEXT("statement_count"), StatementCount);
    if (bRoundtripDetail)
    {
        Result->SetStringField(TEXT("entry"), TEXT("main"));
        Result->SetStringField(TEXT("exit"), TEXT("end"));
        Result->SetObjectField(TEXT("source_map"), SourceMap);
        if (bIncludeReconstructionHints)
        {
            Result->SetObjectField(TEXT("action_hints"), ActionHints);
            Result->SetObjectField(TEXT("expression_hints"), ExpressionHints);
            Result->SetArrayField(TEXT("action_hints_by_line"), ActionHintsByLine);
            Result->SetStringField(TEXT("roundtrip_usage"), TEXT("Pass base_revision, source_map, and hints back to bluecode_apply for an exact union edit."));
        }
    }
    if (bIncludeConnections)
    {
        const TArray<TSharedPtr<FJsonValue>> Connections = CollectBluecodeConnections(Graph);
        Result->SetNumberField(TEXT("connection_count"), Connections.Num());
        Result->SetArrayField(TEXT("connections"), Connections);
    }

    if (bDebugDetail)
    {
        TSharedPtr<FJsonObject> Nodes = GetNodes(Graph);
        if (Nodes.IsValid())
        {
            const TArray<TSharedPtr<FJsonValue>>* NodesArray = nullptr;
            if (Nodes->TryGetArrayField(TEXT("nodes"), NodesArray) && NodesArray)
            {
                Result->SetArrayField(TEXT("nodes"), *NodesArray);
            }
        }
    }
    return Result;
}

TSharedPtr<FJsonObject> UUmgBlueprintFunctionSubsystem::ApplyBluecode(UEdGraph* Graph, UBlueprint* Blueprint, const TSharedPtr<FJsonObject>& Params)
{
    if (!Graph || !Blueprint || !Params.IsValid())
    {
        TSharedPtr<FJsonObject> Error = MakeShared<FJsonObject>();
        Error->SetBoolField(TEXT("success"), false);
        Error->SetStringField(TEXT("error"), TEXT("Missing graph, blueprint, or params for bluecode_apply."));
        return Error;
    }

    FString Code;
    Params->TryGetStringField(TEXT("code"), Code);
    if (Code.IsEmpty())
    {
        TSharedPtr<FJsonObject> Error = MakeShared<FJsonObject>();
        Error->SetBoolField(TEXT("success"), false);
        Error->SetStringField(TEXT("error"), TEXT("bluecode_apply requires non-empty code."));
        return Error;
    }

    FString ExpectedRevision;
    Params->TryGetStringField(TEXT("base_revision"), ExpectedRevision);
    if (ExpectedRevision.IsEmpty()) Params->TryGetStringField(TEXT("expected_revision"), ExpectedRevision);
    const FString CurrentRevision = MakeBluecodeGraphRevision(Graph);
    UPackage* BlueprintPackage = Blueprint->GetOutermost();
    const bool bPackageWasDirty = BlueprintPackage ? BlueprintPackage->IsDirty() : false;
    if (!ExpectedRevision.IsEmpty() && ExpectedRevision != CurrentRevision)
    {
        TSharedPtr<FJsonObject> Error = MakeShared<FJsonObject>();
        Error->SetBoolField(TEXT("success"), false);
        Error->SetStringField(TEXT("error_code"), TEXT("graph_revision_conflict"));
        Error->SetStringField(TEXT("error"), TEXT("The Blueprint graph changed after it was read; re-read before applying this BlueCode patch."));
        Error->SetStringField(TEXT("expected_revision"), ExpectedRevision);
        Error->SetStringField(TEXT("actual_revision"), CurrentRevision);
        return Error;
    }

    FString Mode = TEXT("union");
    Params->TryGetStringField(TEXT("mode"), Mode);
    if (Mode.IsEmpty())
    {
        Mode = TEXT("union");
    }
    const bool bUpsertMode =
        Mode.Equals(TEXT("union"), ESearchCase::IgnoreCase) ||
        Mode.Equals(TEXT("upsert"), ESearchCase::IgnoreCase);
    if (!bUpsertMode && !Mode.Equals(TEXT("append"), ESearchCase::IgnoreCase))
    {
        TSharedPtr<FJsonObject> Error = MakeShared<FJsonObject>();
        Error->SetBoolField(TEXT("success"), false);
        Error->SetStringField(TEXT("error"), TEXT("bluecode_apply supports mode='union' (or legacy 'upsert') and explicit mode='append'."));
        return Error;
    }

    double BaseX = 0.0;
    double BaseY = 0.0;
    Params->TryGetNumberField(TEXT("x"), BaseX);
    Params->TryGetNumberField(TEXT("y"), BaseY);

    FString AutoConnectNodeId;
    Params->TryGetStringField(TEXT("autoConnectToNodeId"), AutoConnectNodeId);

    TArray<TSharedPtr<FJsonValue>> Operations;
    TArray<TSharedPtr<FJsonValue>> Skipped;
    TArray<TSharedPtr<FJsonValue>> Unsupported;
    TArray<TSharedPtr<FJsonValue>> Failures;
    TSharedPtr<FJsonObject> AliasMap = MakeShared<FJsonObject>();
    const TArray<FString> ParsedStatements = ParseBluecodeStatements(Code);
    TSet<FString> ExistingKeys;
    TArray<FBluecodeMergeStatement> ExistingMergeStatements;
    if (bUpsertMode)
    {
        TSharedPtr<FJsonObject> RoundtripParams = MakeShared<FJsonObject>();
        RoundtripParams->SetStringField(TEXT("detail"), TEXT("roundtrip"));
        RoundtripParams->SetBoolField(TEXT("include_reconstruction_hints"), false);
        TSharedPtr<FJsonObject> Existing = ReadBluecodeFunction(Graph, RoundtripParams);
        FString ExistingCode;
        if (Existing.IsValid() && Existing->TryGetStringField(TEXT("code"), ExistingCode))
        {
            const TArray<FString> ExistingStatements = ParseBluecodeStatements(ExistingCode);
            const TSharedPtr<FJsonObject>* ExistingSourceMap = nullptr;
            Existing->TryGetObjectField(TEXT("source_map"), ExistingSourceMap);
            for (int32 ExistingIndex = 0; ExistingIndex < ExistingStatements.Num(); ++ExistingIndex)
            {
                const FString& ExistingStatement = ExistingStatements[ExistingIndex];
                ExistingKeys.Add(NormalizeBluecodeStatement(ExistingStatement));
                FBluecodeMergeStatement MergeStatement;
                MergeStatement.Statement = ExistingStatement;
                MergeStatement.Normalized = NormalizeBluecodeStatement(ExistingStatement);
                if (ExistingSourceMap && ExistingSourceMap->IsValid())
                {
                    const FString Path = FString::Printf(TEXT("/body/%d"), ExistingIndex);
                    const TSharedPtr<FJsonObject>* Source = nullptr;
                    if ((*ExistingSourceMap)->TryGetObjectField(Path, Source) && Source && Source->IsValid())
                    {
                        (*Source)->TryGetStringField(TEXT("node_id"), MergeStatement.NodeId);
                    }
                }
                ExistingMergeStatements.Add(MoveTemp(MergeStatement));
            }
        }
    }

    TArray<FBluecodeMergeStatement> PatchMergeStatements;
    PatchMergeStatements.Reserve(ParsedStatements.Num());
    for (const FString& Statement : ParsedStatements)
    {
        FBluecodeMergeStatement MergeStatement;
        MergeStatement.Statement = Statement;
        MergeStatement.Normalized = NormalizeBluecodeStatement(Statement);
        PatchMergeStatements.Add(MoveTemp(MergeStatement));
    }

    FBluecodeMergePlan MergePlan;
    if (bUpsertMode)
    {
        MergePlan = FBluecodeMergePlanner::PlanUnion(ExistingMergeStatements, PatchMergeStatements);
    }
    else
    {
        for (int32 PatchIndex = 0; PatchIndex < PatchMergeStatements.Num(); ++PatchIndex)
        {
            FBluecodeMergeOperation Operation;
            Operation.Kind = EBluecodeMergeOperationKind::Append;
            Operation.PatchIndex = PatchIndex;
            Operation.Statement = PatchMergeStatements[PatchIndex].Statement;
            MergePlan.Operations.Add(MoveTemp(Operation));
        }
    }

    auto FindLinearExecTailId = [&](const FString& RequestedStartId)
    {
        UEdGraphNode* Current = RequestedStartId.IsEmpty() ? nullptr : FindNodeByIdOrName(Graph, RequestedStartId);
        if (!Current)
        {
            TArray<UEdGraphNode*> Entries;
            for (UEdGraphNode* Node : Graph->Nodes)
            {
                if (IsBluecodeEntryNode(Node))
                {
                    Entries.Add(Node);
                }
            }
            if (Entries.Num() == 1)
            {
                Current = Entries[0];
            }
        }

        TSet<UEdGraphNode*> Visited;
        while (Current && !Visited.Contains(Current))
        {
            Visited.Add(Current);
            TSet<UEdGraphNode*> NextNodes;
            for (UEdGraphPin* Pin : Current->Pins)
            {
                if (!Pin || Pin->Direction != EGPD_Output || Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec)
                {
                    continue;
                }
                for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
                {
                    if (LinkedPin && LinkedPin->GetOwningNode())
                    {
                        NextNodes.Add(LinkedPin->GetOwningNode());
                    }
                }
            }
            if (NextNodes.Num() != 1)
            {
                break;
            }
            for (UEdGraphNode* NextNode : NextNodes)
            {
                Current = NextNode;
                break;
            }
        }
        return Current ? Current->NodeGuid.ToString() : FString();
    };

    auto FindSourceMappedTargetNodeId = [&](const int32 StatementOrdinal, const FString& Statement)
    {
        const TSharedPtr<FJsonObject>* SourceMap = nullptr;
        if (!Params->TryGetObjectField(TEXT("source_map"), SourceMap) ||
            !SourceMap ||
            !SourceMap->IsValid() ||
            (*SourceMap)->Values.Num() != ParsedStatements.Num())
        {
            return FString();
        }

        const FString Path = FString::Printf(TEXT("/body/%d"), StatementOrdinal);
        const TSharedPtr<FJsonObject>* Source = nullptr;
        if (!(*SourceMap)->TryGetObjectField(Path, Source) || !Source || !Source->IsValid())
        {
            return FString();
        }

        FString CallName;
        FString ResolvedNodeName;
        FString IgnoredValue;
        TArray<FString> CallArgs;
        if (TryParseBluecodeAssignment(Statement, ResolvedNodeName, IgnoredValue))
        {
            CallName = ResolvedNodeName;
        }
        else if (TryParseBluecodeBranch(Statement, IgnoredValue))
        {
            CallName = TEXT("if");
            ResolvedNodeName = TEXT("Branch");
        }
        else if (TryParseBluecodeCall(Statement, CallName, CallArgs))
        {
            const bool bGenericNode =
                CallName.Equals(TEXT("node"), ESearchCase::IgnoreCase) ||
                CallName.Equals(TEXT("action"), ESearchCase::IgnoreCase) ||
                CallName.Equals(TEXT("exec"), ESearchCase::IgnoreCase) ||
                CallName.Equals(TEXT("value"), ESearchCase::IgnoreCase) ||
                CallName.Equals(TEXT("pure"), ESearchCase::IgnoreCase);
            if (bGenericNode)
            {
                if (CallArgs.Num() > 0)
                {
                    ResolvedNodeName = CallArgs[0];
                }
            }
            else
            {
                FString IgnoredMemberClass;
                ResolveBluecodeCallName(CallName, Params, ResolvedNodeName, IgnoredMemberClass);
            }
        }

        FString SemanticKey;
        FString NodeId;
        (*Source)->TryGetStringField(TEXT("semantic_key"), SemanticKey);
        (*Source)->TryGetStringField(TEXT("node_id"), NodeId);
        return IsBluecodeSemanticKeyCompatible(SemanticKey, CallName, ResolvedNodeName) ? NodeId : FString();
    };

    bool bDryRun = false;
    Params->TryGetBoolField(TEXT("dry_run"), bDryRun);
    if (bDryRun)
    {
        TArray<TSharedPtr<FJsonValue>> PlannedOperations;
        TArray<FString> PlanIdentityParts = { CurrentRevision, Mode };
        for (const FBluecodeMergeOperation& Operation : MergePlan.Operations)
        {
            TSharedPtr<FJsonObject> Planned = MakeShared<FJsonObject>();
            Planned->SetNumberField(TEXT("patch_index"), Operation.PatchIndex);
            Planned->SetStringField(TEXT("statement"), Operation.Statement);
            FString Kind;
            const FString SourceMappedTarget = FindSourceMappedTargetNodeId(Operation.PatchIndex, Operation.Statement);
            if (Operation.Kind != EBluecodeMergeOperationKind::Noop && !SourceMappedTarget.IsEmpty())
            {
                Kind = TEXT("update");
                Planned->SetStringField(TEXT("target_node_id"), SourceMappedTarget);
            }
            else switch (Operation.Kind)
            {
            case EBluecodeMergeOperationKind::Noop:
                Kind = TEXT("noop");
                Planned->SetStringField(TEXT("matched_node_id"), Operation.MatchedNodeId);
                break;
            case EBluecodeMergeOperationKind::InsertBefore:
                Kind = TEXT("insert_before");
                Planned->SetStringField(TEXT("right_anchor_node_id"), Operation.RightAnchorNodeId);
                break;
            default:
                Kind = TEXT("append");
                Planned->SetStringField(TEXT("append_after_node_id"), FindLinearExecTailId(AutoConnectNodeId));
                break;
            }
            Planned->SetStringField(TEXT("op"), Kind);
            PlanIdentityParts.Add(Kind + TEXT(":") + Operation.Statement + TEXT(":") + Operation.RightAnchorNodeId + TEXT(":") + SourceMappedTarget);
            PlannedOperations.Add(MakeShared<FJsonValueObject>(Planned));
        }

        TSharedPtr<FJsonObject> PlanResult = MakeShared<FJsonObject>();
        PlanResult->SetBoolField(TEXT("success"), MergePlan.Conflicts.Num() == 0);
        PlanResult->SetBoolField(TEXT("dry_run"), true);
        PlanResult->SetStringField(TEXT("expected_revision"), CurrentRevision);
        PlanResult->SetStringField(TEXT("plan_hash"), HashBluecodeIdentity(FString::Join(PlanIdentityParts, TEXT("\n")), TEXT("bcplan:")));
        PlanResult->SetArrayField(TEXT("operations"), PlannedOperations);
        if (MergePlan.Conflicts.Num() > 0)
        {
            TArray<TSharedPtr<FJsonValue>> ConflictValues;
            for (const FString& Conflict : MergePlan.Conflicts)
            {
                ConflictValues.Add(MakeShared<FJsonValueString>(Conflict));
            }
            PlanResult->SetArrayField(TEXT("conflicts"), ConflictValues);
        }
        return PlanResult;
    }

    FString GraphSnapshotText;
    {
        TSet<UObject*> NodesToSnapshot;
        for (UEdGraphNode* Node : Graph->Nodes)
        {
            if (Node)
            {
                NodesToSnapshot.Add(Node);
            }
        }
        FEdGraphUtilities::ExportNodesToText(NodesToSnapshot, GraphSnapshotText);
    }

    auto RestoreGraphSnapshot = [&]()
    {
        TArray<UEdGraphNode*> NodesToRemove;
        for (UEdGraphNode* Node : Graph->Nodes)
        {
            if (Node)
            {
                NodesToRemove.Add(Node);
            }
        }
        for (UEdGraphNode* Node : NodesToRemove)
        {
            if (Node)
            {
                Graph->RemoveNode(Node);
            }
        }
        TSet<UEdGraphNode*> RestoredNodes;
        FEdGraphUtilities::ImportNodesFromText(Graph, GraphSnapshotText, RestoredNodes);
        if (BlueprintPackage)
        {
            BlueprintPackage->SetDirtyFlag(bPackageWasDirty);
        }
        return MakeBluecodeGraphRevision(Graph) == CurrentRevision;
    };

    int32 AppliedCount = 0;
    int32 OperationIndex = 0;
    FString LastNodeId;
    bool bLastNodeExec = false;
    TSet<int32> UsedLineHintIndices;

    auto ApplyPayloadToExistingNode = [&](const TSharedPtr<FJsonObject>& Payload)
    {
        TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
        ResultObj->SetBoolField(TEXT("success"), false);
        if (!Payload.IsValid())
        {
            ResultObj->SetStringField(TEXT("error"), TEXT("Missing payload for existing node update."));
            return ResultObj;
        }

        FString TargetNodeId;
        Payload->TryGetStringField(TEXT("target_node_id"), TargetNodeId);
        if (TargetNodeId.IsEmpty()) Payload->TryGetStringField(TEXT("node_id"), TargetNodeId);
        if (TargetNodeId.IsEmpty()) Payload->TryGetStringField(TEXT("nodeId"), TargetNodeId);
        if (TargetNodeId.IsEmpty())
        {
            ResultObj->SetStringField(TEXT("error"), TEXT("Existing node update requires target_node_id."));
            return ResultObj;
        }

        UEdGraphNode* TargetNode = FindNodeByIdOrName(Graph, TargetNodeId);
        if (!TargetNode)
        {
            ResultObj->SetStringField(TEXT("error"), FString::Printf(TEXT("Target node '%s' not found for BlueCode update."), *TargetNodeId));
            ResultObj->SetStringField(TEXT("nodeId"), TargetNodeId);
            return ResultObj;
        }

        const TSharedPtr<FJsonObject>* InputWiresPtr = nullptr;
        Payload->TryGetObjectField(TEXT("inputWires"), InputWiresPtr);
        const TArray<TSharedPtr<FJsonValue>>* ExtraArgs = nullptr;
        Payload->TryGetArrayField(TEXT("extraArgs"), ExtraArgs);

        TArray<TSharedPtr<FJsonValue>> InputWarnings;
        TArray<TSharedPtr<FJsonValue>> UpdatedPins;
        TArray<TSharedPtr<FJsonValue>> ExistingPins;
        TSet<FString> MatchedInputKeys;

        auto AddInputWarning = [&](const UEdGraphPin* InPin, const FString& Value, const FString& Message)
        {
            TSharedPtr<FJsonObject> Warning = MakeShared<FJsonObject>();
            Warning->SetStringField(TEXT("pin"), InPin ? InPin->PinName.ToString() : TEXT(""));
            Warning->SetStringField(TEXT("value"), Value);
            Warning->SetStringField(TEXT("message"), Message);
            InputWarnings.Add(MakeShared<FJsonValueObject>(Warning));
        };

        auto AddPinResult = [&](TArray<TSharedPtr<FJsonValue>>& Array, const UEdGraphPin* Pin, const FString& Value, const FString& Action)
        {
            TSharedPtr<FJsonObject> PinObj = MakeShared<FJsonObject>();
            PinObj->SetStringField(TEXT("pin"), Pin ? Pin->PinName.ToString() : TEXT(""));
            PinObj->SetStringField(TEXT("value"), Value);
            PinObj->SetStringField(TEXT("action"), Action);
            Array.Add(MakeShared<FJsonValueObject>(PinObj));
        };

        auto IsAssignableInputPinForUpdate = [&](const UEdGraphPin* Pin, const bool bAllowSelf)
        {
            return Pin &&
                Pin->Direction == EGPD_Input &&
                !Pin->bHidden &&
                Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec &&
                (bAllowSelf || !IsBluecodeSelfPin(Pin));
        };

        auto CountAssignableInputs = [&]()
        {
            int32 Count = 0;
            for (const UEdGraphPin* Pin : TargetNode->Pins)
            {
                if (IsAssignableInputPinForUpdate(Pin, false))
                {
                    ++Count;
                }
            }
            return Count;
        };

        auto HasInputPinForKey = [&](const FString& Key)
        {
            const bool bSelfKey = IsBluecodeSelfPinAlias(Key);
            for (const UEdGraphPin* Pin : TargetNode->Pins)
            {
                if (IsAssignableInputPinForUpdate(Pin, bSelfKey) && BluecodePinNameMatches(Pin, Key))
                {
                    return true;
                }
            }
            return false;
        };

        auto HasUnmatchedDynamicInputKey = [&]()
        {
            if (!InputWiresPtr || !(*InputWiresPtr).IsValid())
            {
                return false;
            }
            for (const auto& Pair : (*InputWiresPtr)->Values)
            {
                const FString Key = UmgMcpJsonCompat::KeyToString(Pair.Key);
                if (IsBluecodeDynamicInputKey(Key) && !HasInputPinForKey(Key))
                {
                    return true;
                }
            }
            return false;
        };

        auto PreconfigureFormatTextNode = [&]()
        {
            UK2Node_FormatText* FormatTextNode = Cast<UK2Node_FormatText>(TargetNode);
            if (!FormatTextNode)
            {
                return;
            }
            UEdGraphPin* FormatPin = FormatTextNode->GetFormatPin();
            if (!FormatPin)
            {
                return;
            }

            FString FormatValue;
            bool bHasFormatValue = false;
            if (InputWiresPtr && (*InputWiresPtr).IsValid() && TryGetBluecodeInputWireValue(*InputWiresPtr, FormatPin, FormatValue))
            {
                bHasFormatValue = true;
            }
            else if (ExtraArgs && ExtraArgs->Num() > 0)
            {
                FormatValue = (*ExtraArgs)[0]->AsString();
                bHasFormatValue = !FormatValue.IsEmpty();
            }

            if (bHasFormatValue)
            {
                FString ErrorMessage;
                if (TrySetBluecodePinDefault(FormatPin, FormatValue, ErrorMessage))
                {
                    FormatTextNode->PinDefaultValueChanged(FormatPin);
                }
                else
                {
                    AddInputWarning(FormatPin, FormatValue, ErrorMessage);
                }
            }
        };

        auto ExpandDynamicInputPins = [&]()
        {
            IK2Node_AddPinInterface* AddPinNode = Cast<IK2Node_AddPinInterface>(TargetNode);
            if (!AddPinNode)
            {
                return;
            }

            int32 DesiredInputCount = 0;
            if (ExtraArgs)
            {
                DesiredInputCount += ExtraArgs->Num();
            }
            if (InputWiresPtr && (*InputWiresPtr).IsValid())
            {
                DesiredInputCount += (*InputWiresPtr)->Values.Num();
            }

            constexpr int32 MaxBluecodeAutoAddedPins = 32;
            int32 AddedCount = 0;
            while (AddedCount < MaxBluecodeAutoAddedPins)
            {
                if (DesiredInputCount <= CountAssignableInputs() && !HasUnmatchedDynamicInputKey())
                {
                    break;
                }
                AddPinNode = Cast<IK2Node_AddPinInterface>(TargetNode);
                if (!AddPinNode || !AddPinNode->CanAddPin())
                {
                    AddInputWarning(nullptr, TEXT(""), FString::Printf(
                        TEXT("Node '%s' cannot add enough dynamic input pins for the requested BlueCode update."),
                        *BluecodeNodeTitle(TargetNode)));
                    break;
                }
                AddPinNode->AddInputPin();
                if (UEdGraph* OwningGraph = TargetNode->GetGraph())
                {
                    OwningGraph->NotifyNodeChanged(TargetNode);
                }
                ++AddedCount;
            }
        };

        auto TryWireUpdateExpressionNode = [&](const FString& Expression, UEdGraphPin* TargetPin, FString& OutError)
        {
            if (!Graph || !TargetPin || Expression.IsEmpty())
            {
                return false;
            }
            if (TargetPin->LinkedTo.Num() > 0)
            {
                OutError = TEXT("Pin is already linked; refusing to replace the link with a generated expression node.");
                return false;
            }

            TSet<FGuid> ExistingNodeGuids;
            for (UEdGraphNode* ExistingNode : Graph->Nodes)
            {
                if (ExistingNode)
                {
                    ExistingNodeGuids.Add(ExistingNode->NodeGuid);
                }
            }

            auto RollbackCreatedExpressionNodes = [&]()
            {
                TArray<UEdGraphNode*> NodesToRemove;
                for (UEdGraphNode* CandidateNode : Graph->Nodes)
                {
                    if (CandidateNode && !ExistingNodeGuids.Contains(CandidateNode->NodeGuid))
                    {
                        NodesToRemove.Add(CandidateNode);
                    }
                }
                for (UEdGraphNode* CandidateNode : NodesToRemove)
                {
                    Graph->RemoveNode(CandidateNode);
                }
            };

            TSharedPtr<FJsonObject> ValuePayload = MakeShared<FJsonObject>();
            ValuePayload->SetStringField(TEXT("subAction"), TEXT("create_node"));
            ValuePayload->SetNumberField(TEXT("x"), TargetNode->NodePosX - 260.0);
            ValuePayload->SetNumberField(TEXT("y"), TargetNode->NodePosY + (TargetPin->SourceIndex * 80.0));

            FString BinaryLeft;
            FString BinaryOp;
            FString BinaryRight;
            if (TryParseBluecodeBinaryExpression(Expression, BinaryLeft, BinaryOp, BinaryRight))
            {
                FString Family = BluecodeMathTypeFamily(Blueprint, TEXT(""), BinaryLeft, BinaryRight);
                const FString TargetCategory = TargetPin->PinType.PinCategory.ToString().ToLower();
                if (TargetCategory.Contains(TEXT("int")) || TargetCategory.Contains(TEXT("byte")))
                {
                    Family = TEXT("int");
                }
                else if (TargetCategory.Contains(TEXT("float")) || TargetCategory.Contains(TEXT("double")) || TargetCategory.Contains(TEXT("real")))
                {
                    Family = TEXT("float");
                }

                const FString MathFunctionName = BluecodeMathFunctionName(BinaryOp, Family);
                if (MathFunctionName.IsEmpty())
                {
                    OutError = FString::Printf(TEXT("Unsupported expression operator '%s'."), *BinaryOp);
                    return false;
                }

                ValuePayload->SetStringField(TEXT("nodeName"), MathFunctionName);
                TArray<TSharedPtr<FJsonValue>> ValueArgs;
                ValueArgs.Add(MakeShared<FJsonValueString>(BinaryLeft));
                ValueArgs.Add(MakeShared<FJsonValueString>(BinaryRight));
                ValuePayload->SetArrayField(TEXT("extraArgs"), ValueArgs);
            }
            else
            {
                FString ExprCallName;
                TArray<FString> ExprCallArgs;
                if (!TryParseBluecodeCall(Expression, ExprCallName, ExprCallArgs))
                {
                    return false;
                }

                const bool bGenericValueNode =
                    ExprCallName.Equals(TEXT("value"), ESearchCase::IgnoreCase) ||
                    ExprCallName.Equals(TEXT("pure"), ESearchCase::IgnoreCase);
                const bool bGenericExecNode =
                    ExprCallName.Equals(TEXT("node"), ESearchCase::IgnoreCase) ||
                    ExprCallName.Equals(TEXT("action"), ESearchCase::IgnoreCase) ||
                    ExprCallName.Equals(TEXT("exec"), ESearchCase::IgnoreCase);
                if (bGenericExecNode)
                {
                    OutError = TEXT("Executable node expression cannot be used as a data input.");
                    return false;
                }

                if (bGenericValueNode)
                {
                    if (ExprCallArgs.Num() == 0 || ExprCallArgs[0].IsEmpty())
                    {
                        OutError = TEXT("value(...) expression requires an Action Menu name.");
                        return false;
                    }
                    ValuePayload->SetStringField(TEXT("nodeName"), ExprCallArgs[0]);
                    ValuePayload->SetBoolField(TEXT("useActionMenu"), true);
                    ApplyBluecodeActionHints(Expression, ExprCallName, ExprCallArgs[0], Params, ValuePayload);
                    ApplyBluecodeArgsToPayload(ExprCallArgs, 1, ValuePayload);
                }
                else
                {
                    const FName TargetCategory = TargetPin->PinType.PinCategory;
                    if (TargetCategory == UEdGraphSchema_K2::PC_String ||
                        TargetCategory == UEdGraphSchema_K2::PC_Text ||
                        TargetCategory == UEdGraphSchema_K2::PC_Name)
                    {
                        return false;
                    }
                    ValuePayload->SetStringField(TEXT("nodeName"), ExprCallName);
                    ApplyBluecodeActionHints(Expression, ExprCallName, ExprCallName, Params, ValuePayload);
                    ApplyBluecodeArgsToPayload(ExprCallArgs, 0, ValuePayload);
                }
            }

            UEdGraphNode* ValueNode = nullptr;
            TSharedPtr<FJsonObject> ValueResult = CreateNodeInstance(Graph, ValuePayload, ValueNode);
            if (!ValueResult.IsValid() || !ValueNode)
            {
                OutError = TEXT("Failed to create expression value node.");
                if (ValueResult.IsValid())
                {
                    ValueResult->TryGetStringField(TEXT("error"), OutError);
                }
                RollbackCreatedExpressionNodes();
                return false;
            }

            TArray<UEdGraphPin*> CompatibleOutputs;
            for (UEdGraphPin* SourcePin : ValueNode->Pins)
            {
                if (!SourcePin ||
                    SourcePin->Direction != EGPD_Output ||
                    SourcePin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec ||
                    SourcePin->bHidden)
                {
                    continue;
                }

                const FPinConnectionResponse Response = Graph->GetSchema()->CanCreateConnection(SourcePin, TargetPin);
                if (Response.Response == CONNECT_RESPONSE_MAKE ||
                    Response.Response == CONNECT_RESPONSE_MAKE_WITH_CONVERSION_NODE ||
                    Response.Response == CONNECT_RESPONSE_MAKE_WITH_PROMOTION)
                {
                    CompatibleOutputs.Add(SourcePin);
                }
            }

            if (CompatibleOutputs.Num() != 1)
            {
                OutError = CompatibleOutputs.Num() == 0
                    ? TEXT("Expression value node has no compatible output pin.")
                    : TEXT("Expression value node has multiple compatible output pins; use bluecode_connect with an explicit source pin.");
                RollbackCreatedExpressionNodes();
                return false;
            }

            if (!Graph->GetSchema()->TryCreateConnection(CompatibleOutputs[0], TargetPin))
            {
                OutError = TEXT("Expression value node connection failed.");
                RollbackCreatedExpressionNodes();
                return false;
            }
            return true;
        };

        auto ApplyValueToPin = [&](UEdGraphPin* Pin, const FString& RawValue)
        {
            FString Value = RawValue;
            Value.TrimStartAndEndInline();
            if (!Pin || Value.IsEmpty())
            {
                return false;
            }
            if (Value.Equals(TEXT("null"), ESearchCase::IgnoreCase) ||
                Value.Equals(TEXT("(null)"), ESearchCase::IgnoreCase) ||
                Value.Equals(TEXT("wait"), ESearchCase::IgnoreCase) ||
                Value.Equals(TEXT("(wait)"), ESearchCase::IgnoreCase))
            {
                AddPinResult(ExistingPins, Pin, Value, TEXT("skipped"));
                return true;
            }

            FString SourceNodeId = Value;
            FString SourcePinName;
            SourceNodeId.TrimStartAndEndInline();
            if (Value.Split(TEXT(":"), &SourceNodeId, &SourcePinName))
            {
                SourceNodeId.TrimStartAndEndInline();
                SourcePinName.TrimStartAndEndInline();
            }

            UEdGraphNode* SourceNode = FindNodeByIdOrName(Graph, SourceNodeId);
            if (SourceNode && SourceNode != TargetNode)
            {
                TArray<UEdGraphPin*> CompatibleOutputs;
                for (UEdGraphPin* SourcePin : SourceNode->Pins)
                {
                    if (!SourcePin ||
                        SourcePin->Direction != EGPD_Output ||
                        SourcePin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec ||
                        SourcePin->bHidden ||
                        (!SourcePinName.IsEmpty() && !BluecodePinNameMatches(SourcePin, SourcePinName)))
                    {
                        continue;
                    }

                    const FPinConnectionResponse Response = Graph->GetSchema()->CanCreateConnection(SourcePin, Pin);
                    if (Pin->LinkedTo.Contains(SourcePin) ||
                        Response.Response == CONNECT_RESPONSE_MAKE ||
                        Response.Response == CONNECT_RESPONSE_MAKE_WITH_CONVERSION_NODE ||
                        Response.Response == CONNECT_RESPONSE_MAKE_WITH_PROMOTION)
                    {
                        CompatibleOutputs.Add(SourcePin);
                    }
                }

                if (CompatibleOutputs.Num() != 1)
                {
                    AddInputWarning(Pin, Value, CompatibleOutputs.Num() == 0
                        ? TEXT("No compatible source output pin found for existing node update.")
                        : TEXT("Multiple compatible source output pins found; use bluecode_connect with explicit source pin."));
                    return false;
                }

                UEdGraphPin* SourcePin = CompatibleOutputs[0];
                if (Pin->LinkedTo.Contains(SourcePin))
                {
                    AddPinResult(ExistingPins, Pin, Value, TEXT("link_exists"));
                    return true;
                }
                if (Pin->LinkedTo.Num() > 0)
                {
                    AddInputWarning(Pin, Value, TEXT("Existing pin link would be replaced. Use bluecode_delete(kind='connection') before reconnecting."));
                    return false;
                }

                if (Graph->GetSchema()->TryCreateConnection(SourcePin, Pin))
                {
                    AddPinResult(UpdatedPins, Pin, Value, TEXT("connected"));
                    return true;
                }

                AddInputWarning(Pin, Value, TEXT("TryCreateConnection failed for existing node update."));
                return false;
            }

            FString ExpressionError;
            if (TryWireUpdateExpressionNode(Value, Pin, ExpressionError))
            {
                AddPinResult(UpdatedPins, Pin, Value, TEXT("expression"));
                return true;
            }
            if (!ExpressionError.IsEmpty())
            {
                AddInputWarning(Pin, Value, ExpressionError);
                return false;
            }

            if (Pin->LinkedTo.Num() > 0)
            {
                AddInputWarning(Pin, Value, TEXT("Pin is already linked; refusing to replace the link with a literal default."));
                return false;
            }

            FString ErrorMessage;
            if (TrySetBluecodePinDefault(Pin, Value, ErrorMessage))
            {
                AddPinResult(UpdatedPins, Pin, Value, TEXT("default"));
                return true;
            }

            AddInputWarning(Pin, Value, ErrorMessage);
            return false;
        };

        TargetNode->Modify();
        if (Graph)
        {
            Graph->Modify();
        }
        TSharedPtr<FJsonObject> NodePropertyResult = ApplyBluecodeNodeProperties(TargetNode, Payload);
        bool bNodePropertiesOk = true;
        double RequestedProperties = 0.0;
        if (NodePropertyResult.IsValid())
        {
            NodePropertyResult->TryGetBoolField(TEXT("success"), bNodePropertiesOk);
            NodePropertyResult->TryGetNumberField(TEXT("requested_count"), RequestedProperties);
        }
        PreconfigureFormatTextNode();
        ExpandDynamicInputPins();

        int32 ArgIndex = 0;
        bool bHadInput =
            (InputWiresPtr && (*InputWiresPtr).IsValid() && (*InputWiresPtr)->Values.Num() > 0) ||
            (ExtraArgs && ExtraArgs->Num() > 0);
        for (UEdGraphPin* Pin : TargetNode->Pins)
        {
            bool bAllowSelfForPin = false;
            if (Pin && IsBluecodeSelfPin(Pin) && InputWiresPtr && (*InputWiresPtr).IsValid())
            {
                for (const auto& Pair : (*InputWiresPtr)->Values)
                {
                    const FString Key = UmgMcpJsonCompat::KeyToString(Pair.Key);
                    if (IsBluecodeSelfPinAlias(Key) && BluecodePinNameMatches(Pin, Key))
                    {
                        bAllowSelfForPin = true;
                        break;
                    }
                }
            }
            if (!IsAssignableInputPinForUpdate(Pin, bAllowSelfForPin))
            {
                continue;
            }

            FString Value;
            bool bFoundValue = false;
            if (InputWiresPtr && (*InputWiresPtr).IsValid())
            {
                for (const auto& Pair : (*InputWiresPtr)->Values)
                {
                    const FString Key = UmgMcpJsonCompat::KeyToString(Pair.Key);
                    if (BluecodePinNameMatches(Pin, Key))
                    {
                        Value = BluecodeJsonValueToString(Pair.Value);
                        MatchedInputKeys.Add(NormalizeBluecodePinName(Key));
                        bFoundValue = true;
                        break;
                    }
                }
            }

            if (!bFoundValue && ExtraArgs && ArgIndex < ExtraArgs->Num())
            {
                Value = (*ExtraArgs)[ArgIndex]->AsString();
                ++ArgIndex;
                bFoundValue = true;
            }

            if (bFoundValue)
            {
                ApplyValueToPin(Pin, Value);
            }
        }

        if (InputWiresPtr && (*InputWiresPtr).IsValid())
        {
            for (const auto& Pair : (*InputWiresPtr)->Values)
            {
                const FString Key = UmgMcpJsonCompat::KeyToString(Pair.Key);
                if (!MatchedInputKeys.Contains(NormalizeBluecodePinName(Key)) && !HasInputPinForKey(Key))
                {
                    AddInputWarning(nullptr, BluecodeJsonValueToString(Pair.Value), FString::Printf(
                        TEXT("Node '%s' has no assignable input pin named '%s'."),
                        *BluecodeNodeTitle(TargetNode),
                        *Key));
                }
            }
        }

        bool bIsExec = false;
        for (UEdGraphPin* Pin : TargetNode->Pins)
        {
            if (Pin && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
            {
                bIsExec = true;
                break;
            }
        }

        ResultObj->SetBoolField(TEXT("success"), bNodePropertiesOk && (InputWarnings.Num() == 0 || UpdatedPins.Num() > 0 || ExistingPins.Num() > 0 || !bHadInput));
        ResultObj->SetBoolField(TEXT("updated_existing"), true);
        ResultObj->SetStringField(TEXT("nodeId"), TargetNode->NodeGuid.ToString());
        ResultObj->SetStringField(TEXT("nodeName"), TargetNode->GetName());
        ResultObj->SetBoolField(TEXT("isExec"), bIsExec);
        AddBluecodeNodeResultFields(TargetNode, ResultObj);
        if (NodePropertyResult.IsValid() && RequestedProperties > 0.0)
        {
            ResultObj->SetObjectField(TEXT("node_properties"), NodePropertyResult);
        }
        ResultObj->SetNumberField(TEXT("updated_input_count"), UpdatedPins.Num());
        ResultObj->SetNumberField(TEXT("existing_input_count"), ExistingPins.Num());
        if (UpdatedPins.Num() > 0)
        {
            ResultObj->SetArrayField(TEXT("updated_inputs"), UpdatedPins);
        }
        if (ExistingPins.Num() > 0)
        {
            ResultObj->SetArrayField(TEXT("existing_inputs"), ExistingPins);
        }
        if (InputWarnings.Num() > 0)
        {
            ResultObj->SetArrayField(TEXT("input_warnings"), InputWarnings);
        }
        return ResultObj;
    };

    for (int32 StatementOrdinal = 0; StatementOrdinal < ParsedStatements.Num(); ++StatementOrdinal)
    {
        const FString& Statement = ParsedStatements[StatementOrdinal];
        const FString StatementKey = NormalizeBluecodeStatement(Statement);
        const FBluecodeMergeOperation* PlannedOperation = MergePlan.Operations.IsValidIndex(StatementOrdinal)
            ? &MergePlan.Operations[StatementOrdinal]
            : nullptr;
        if (bUpsertMode && PlannedOperation && PlannedOperation->Kind == EBluecodeMergeOperationKind::Noop)
        {
            TSharedPtr<FJsonObject> SkippedObj = MakeShared<FJsonObject>();
            SkippedObj->SetStringField(TEXT("statement"), Statement);
            SkippedObj->SetStringField(TEXT("reason"), TEXT("semantic_match"));
            if (!PlannedOperation->MatchedNodeId.IsEmpty())
            {
                SkippedObj->SetStringField(TEXT("matched_node_id"), PlannedOperation->MatchedNodeId);
            }
            Skipped.Add(MakeShared<FJsonValueObject>(SkippedObj));
            continue;
        }

        TSharedPtr<FJsonObject> Payload = MakeShared<FJsonObject>();
        Payload->SetStringField(TEXT("subAction"), TEXT("add_function_step"));
        Payload->SetNumberField(TEXT("x"), BaseX + (OperationIndex * 320.0));
        Payload->SetNumberField(TEXT("y"), BaseY + (OperationIndex * 80.0));
        CopyBluecodeHintContext(Params, Payload);
        const FString SourceMappedTargetNodeId = FindSourceMappedTargetNodeId(StatementOrdinal, Statement);
        if (!SourceMappedTargetNodeId.IsEmpty())
        {
            Payload->SetStringField(TEXT("target_node_id"), SourceMappedTargetNodeId);
        }
        if (PlannedOperation &&
            PlannedOperation->Kind == EBluecodeMergeOperationKind::InsertBefore &&
            !PlannedOperation->RightAnchorNodeId.IsEmpty())
        {
            Payload->SetStringField(TEXT("insertBeforeNodeId"), PlannedOperation->RightAnchorNodeId);
        }
        else
        {
            const FString TailNodeId = FindLinearExecTailId(AutoConnectNodeId);
            if (!TailNodeId.IsEmpty())
            {
                Payload->SetStringField(TEXT("autoConnectToNodeId"), TailNodeId);
            }
        }

        FString AssignmentName;
        FString AssignmentValue;
        FString BranchCondition;
        FString CallName;
        TArray<FString> CallArgs;
        FString Kind = TEXT("call");
        FString OperationAlias;
        TSharedPtr<FJsonObject> ValueResultForOperation;

        if (TryParseBluecodeAssignment(Statement, AssignmentName, AssignmentValue))
        {
            Kind = TEXT("assignment");
            Payload->SetStringField(TEXT("nodeType"), TEXT("VariableSet"));
            Payload->SetStringField(TEXT("variableName"), AssignmentName);

            FString BinaryLeft;
            FString BinaryOp;
            FString BinaryRight;
            if (TryParseBluecodeBinaryExpression(AssignmentValue, BinaryLeft, BinaryOp, BinaryRight))
            {
                const FString MathFunctionName = BluecodeMathFunctionName(
                    BinaryOp,
                    BluecodeMathTypeFamily(Blueprint, AssignmentName, BinaryLeft, BinaryRight));
                if (MathFunctionName.IsEmpty())
                {
                    TSharedPtr<FJsonObject> UnsupportedObj = MakeShared<FJsonObject>();
                    UnsupportedObj->SetStringField(TEXT("statement"), Statement);
                    UnsupportedObj->SetStringField(TEXT("reason"), FString::Printf(TEXT("unsupported binary operator %s"), *BinaryOp));
                    Unsupported.Add(MakeShared<FJsonValueObject>(UnsupportedObj));
                    continue;
                }

                TSharedPtr<FJsonObject> ValuePayload = MakeShared<FJsonObject>();
                ValuePayload->SetStringField(TEXT("subAction"), TEXT("create_node"));
                ValuePayload->SetStringField(TEXT("nodeName"), MathFunctionName);
                ValuePayload->SetNumberField(TEXT("x"), BaseX + (OperationIndex * 320.0) - 260.0);
                ValuePayload->SetNumberField(TEXT("y"), BaseY + (OperationIndex * 80.0) + 120.0);

                TArray<TSharedPtr<FJsonValue>> ValueArgs;
                ValueArgs.Add(MakeShared<FJsonValueString>(BinaryLeft));
                ValueArgs.Add(MakeShared<FJsonValueString>(BinaryRight));
                ValuePayload->SetArrayField(TEXT("extraArgs"), ValueArgs);

                ValueResultForOperation = AddNode(Graph, ValuePayload);
                bool bValueOk = false;
                FString ValueNodeId;
                if (!ValueResultForOperation.IsValid() ||
                    !ValueResultForOperation->TryGetBoolField(TEXT("success"), bValueOk) ||
                    !bValueOk ||
                    !ValueResultForOperation->TryGetStringField(TEXT("nodeId"), ValueNodeId) ||
                    ValueNodeId.IsEmpty())
                {
                    TSharedPtr<FJsonObject> Operation = MakeShared<FJsonObject>();
                    Operation->SetStringField(TEXT("statement"), Statement);
                    Operation->SetStringField(TEXT("kind"), TEXT("assignment_expression"));
                    if (ValueResultForOperation.IsValid())
                    {
                        Operation->SetObjectField(TEXT("value_result"), ValueResultForOperation);
                    }
                    TSharedPtr<FJsonObject> ErrorObj = MakeShared<FJsonObject>();
                    ErrorObj->SetBoolField(TEXT("success"), false);
                    ErrorObj->SetStringField(TEXT("error"), TEXT("Failed to create math expression value node."));
                    Operation->SetObjectField(TEXT("result"), ErrorObj);
                    TSharedPtr<FJsonObject> Failure = MakeShared<FJsonObject>();
                    Failure->SetStringField(TEXT("statement"), Statement);
                    Failure->SetStringField(TEXT("kind"), TEXT("assignment_expression"));
                    Failure->SetObjectField(TEXT("result"), ErrorObj);
                    Failures.Add(MakeShared<FJsonValueObject>(Failure));
                    Operations.Add(MakeShared<FJsonValueObject>(Operation));
                    ++OperationIndex;
                    continue;
                }

                Kind = TEXT("assignment_expression");
                TArray<TSharedPtr<FJsonValue>> Args;
                Args.Add(MakeShared<FJsonValueString>(ValueNodeId));
                Payload->SetArrayField(TEXT("extraArgs"), Args);
            }
            else
            {
                TArray<TSharedPtr<FJsonValue>> Args;
                Args.Add(MakeShared<FJsonValueString>(AssignmentValue));
                Payload->SetArrayField(TEXT("extraArgs"), Args);
            }
            ApplyBluecodeActionHints(Statement, AssignmentName, AssignmentName, Params, Payload, StatementOrdinal, &UsedLineHintIndices);
        }
        else if (TryParseBluecodeBranch(Statement, BranchCondition))
        {
            Kind = TEXT("branch");
            Payload->SetStringField(TEXT("nodeType"), TEXT("Branch"));
            TSharedPtr<FJsonObject> InputWires = MakeShared<FJsonObject>();
            InputWires->SetStringField(TEXT("Condition"), BranchCondition);
            Payload->SetObjectField(TEXT("inputWires"), InputWires);
            ApplyBluecodeActionHints(Statement, TEXT("if"), TEXT("Branch"), Params, Payload, StatementOrdinal, &UsedLineHintIndices);
        }
        else if (TryParseBluecodeCall(Statement, CallName, CallArgs))
        {
            const bool bGenericExecNode =
                CallName.Equals(TEXT("node"), ESearchCase::IgnoreCase) ||
                CallName.Equals(TEXT("action"), ESearchCase::IgnoreCase) ||
                CallName.Equals(TEXT("exec"), ESearchCase::IgnoreCase);
            const bool bGenericValueNode =
                CallName.Equals(TEXT("value"), ESearchCase::IgnoreCase) ||
                CallName.Equals(TEXT("pure"), ESearchCase::IgnoreCase);

            if (bGenericExecNode || bGenericValueNode)
            {
                if (CallArgs.Num() == 0 || CallArgs[0].IsEmpty())
                {
                    TSharedPtr<FJsonObject> UnsupportedObj = MakeShared<FJsonObject>();
                    UnsupportedObj->SetStringField(TEXT("statement"), Statement);
                    UnsupportedObj->SetStringField(TEXT("reason"), TEXT("generic node statements require node(\"Action Menu Name\", ...) or value(\"Action Menu Name\", ...)"));
                    Unsupported.Add(MakeShared<FJsonValueObject>(UnsupportedObj));
                    continue;
                }

                const FString NodeName = CallArgs[0];
                ExtractBluecodeAliasArg(CallArgs, 1, OperationAlias);
                ApplyBluecodeAliasHints(Statement, CallName, NodeName, Params, OperationAlias);
                Kind = bGenericValueNode ? TEXT("value_node") : TEXT("action_node");
                Payload->SetStringField(TEXT("nodeName"), NodeName);
                Payload->SetBoolField(TEXT("useActionMenu"), true);
                if (bGenericValueNode)
                {
                    Payload->SetStringField(TEXT("subAction"), TEXT("create_node"));
                    Payload->RemoveField(TEXT("autoConnectToNodeId"));
                    Payload->RemoveField(TEXT("insertBeforeNodeId"));
                }
                ApplyBluecodeActionHints(Statement, CallName, NodeName, Params, Payload, StatementOrdinal, &UsedLineHintIndices);
                ApplyBluecodeArgsToPayload(CallArgs, 1, Payload);
            }
            else
            {
                FString NodeName;
                FString MemberClass;
                ResolveBluecodeCallName(CallName, Params, NodeName, MemberClass);

                Payload->SetStringField(TEXT("nodeName"), NodeName);
                ExtractBluecodeAliasArg(CallArgs, 0, OperationAlias);
                ApplyBluecodeAliasHints(Statement, CallName, NodeName, Params, OperationAlias);
                if (!MemberClass.IsEmpty())
                {
                    Payload->SetStringField(TEXT("memberClass"), MemberClass);
                }

                ApplyBluecodeActionHints(Statement, CallName, NodeName, Params, Payload, StatementOrdinal, &UsedLineHintIndices);

                ApplyBluecodeArgsToPayload(CallArgs, 0, Payload);
            }
        }
        else
        {
            TSharedPtr<FJsonObject> UnsupportedObj = MakeShared<FJsonObject>();
            UnsupportedObj->SetStringField(TEXT("statement"), Statement);
            UnsupportedObj->SetStringField(TEXT("reason"), TEXT("supported forms are FunctionName(arg, ...), node(\"Action Menu Name\", ...), value(\"Action Menu Name\", ...), Name = value, and if condition:"));
            Unsupported.Add(MakeShared<FJsonValueObject>(UnsupportedObj));
            continue;
        }

        FString TargetNodeIdForUpdate;
        Payload->TryGetStringField(TEXT("target_node_id"), TargetNodeIdForUpdate);
        const bool bUpdateExistingNode = !TargetNodeIdForUpdate.IsEmpty();
        TSharedPtr<FJsonObject> OpResult = bUpdateExistingNode
            ? ApplyPayloadToExistingNode(Payload)
            : AddNode(Graph, Payload);
        TSharedPtr<FJsonObject> Operation = MakeShared<FJsonObject>();
        Operation->SetStringField(TEXT("statement"), Statement);
        Operation->SetStringField(TEXT("kind"), Kind);
        if (bUpdateExistingNode)
        {
            Operation->SetBoolField(TEXT("updated_existing"), true);
            Operation->SetStringField(TEXT("target_node_id"), TargetNodeIdForUpdate);
        }
        if (!OperationAlias.IsEmpty())
        {
            Operation->SetStringField(TEXT("alias"), OperationAlias);
        }
        if (ValueResultForOperation.IsValid())
        {
            Operation->SetObjectField(TEXT("value_result"), ValueResultForOperation);
        }
        if (OpResult.IsValid())
        {
            Operation->SetObjectField(TEXT("result"), OpResult);
            bool bOk = false;
            OpResult->TryGetBoolField(TEXT("success"), bOk);
            if (bOk)
            {
                ++AppliedCount;
                bool bIsExec = false;
                OpResult->TryGetBoolField(TEXT("isExec"), bIsExec);
                FString NodeId;
                if (bIsExec && OpResult->TryGetStringField(TEXT("nodeId"), NodeId) && !NodeId.IsEmpty())
                {
                    AutoConnectNodeId = NodeId;
                    LastNodeId = NodeId;
                    bLastNodeExec = true;
                }
                else
                {
                    OpResult->TryGetStringField(TEXT("nodeId"), NodeId);
                }
                if (!OperationAlias.IsEmpty() && !NodeId.IsEmpty())
                {
                    AliasMap->SetStringField(OperationAlias, NodeId);
                }
                if (bUpsertMode)
                {
                    ExistingKeys.Add(StatementKey);
                }
            }
            else
            {
                TSharedPtr<FJsonObject> Failure = MakeShared<FJsonObject>();
                Failure->SetStringField(TEXT("statement"), Statement);
                Failure->SetStringField(TEXT("kind"), Kind);
                Failure->SetObjectField(TEXT("result"), OpResult);
                Failures.Add(MakeShared<FJsonValueObject>(Failure));
            }
        }
        else
        {
            TSharedPtr<FJsonObject> ErrorObj = MakeShared<FJsonObject>();
            ErrorObj->SetBoolField(TEXT("success"), false);
            ErrorObj->SetStringField(TEXT("error"), TEXT("Failed to create node from bluecode statement."));
            Operation->SetObjectField(TEXT("result"), ErrorObj);
            TSharedPtr<FJsonObject> Failure = MakeShared<FJsonObject>();
            Failure->SetStringField(TEXT("statement"), Statement);
            Failure->SetStringField(TEXT("kind"), Kind);
            Failure->SetObjectField(TEXT("result"), ErrorObj);
            Failures.Add(MakeShared<FJsonValueObject>(Failure));
        }
        Operations.Add(MakeShared<FJsonValueObject>(Operation));
        ++OperationIndex;
    }

    const int32 AttemptedAppliedCount = AppliedCount;
    const bool bNeedsRollback = Unsupported.Num() > 0 || Failures.Num() > 0;
    bool bRollbackVerified = false;
    if (bNeedsRollback)
    {
        bRollbackVerified = RestoreGraphSnapshot();
        AppliedCount = 0;
        LastNodeId.Empty();
        bLastNodeExec = false;
        AliasMap = MakeShared<FJsonObject>();
    }

    TSharedPtr<FJsonObject> MergeReport = MakeShared<FJsonObject>();
    MergeReport->SetStringField(TEXT("strategy"), bUpsertMode ? TEXT("lcs_union_right_anchor") : TEXT("explicit_append"));
    MergeReport->SetNumberField(TEXT("deleted_count"), 0);
    MergeReport->SetNumberField(TEXT("attempted_applied_count"), AttemptedAppliedCount);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), !bNeedsRollback && AppliedCount == Operations.Num());
    Result->SetStringField(TEXT("mode"), Mode);
    Result->SetNumberField(TEXT("applied_count"), AppliedCount);
    Result->SetArrayField(TEXT("operations"), Operations);
    Result->SetArrayField(TEXT("skipped"), Skipped);
    Result->SetArrayField(TEXT("unsupported"), Unsupported);
    Result->SetArrayField(TEXT("failures"), Failures);
    Result->SetObjectField(TEXT("merge_report"), MergeReport);
    Result->SetObjectField(TEXT("aliases"), AliasMap);
    Result->SetStringField(TEXT("base_revision"), CurrentRevision);
    Result->SetStringField(TEXT("new_revision"), MakeBluecodeGraphRevision(Graph));
    if (bNeedsRollback)
    {
        Result->SetBoolField(TEXT("rolled_back"), true);
        Result->SetBoolField(TEXT("rollback_verified"), bRollbackVerified);
        Result->SetStringField(
            TEXT("error_code"),
            bRollbackVerified ? TEXT("bluecode_apply_failed_rolled_back") : TEXT("bluecode_apply_rollback_verification_failed"));
    }
    if (!LastNodeId.IsEmpty())
    {
        Result->SetStringField(TEXT("nodeId"), LastNodeId);
        Result->SetBoolField(TEXT("isExec"), bLastNodeExec);
    }
    return Result;
}

TSharedPtr<FJsonObject> UUmgBlueprintFunctionSubsystem::ApplyBluecodeVariables(UBlueprint* Blueprint, const TSharedPtr<FJsonObject>& Params)
{
    if (!Blueprint || !Params.IsValid())
    {
        TSharedPtr<FJsonObject> Error = MakeShared<FJsonObject>();
        Error->SetBoolField(TEXT("success"), false);
        Error->SetStringField(TEXT("error"), TEXT("Missing blueprint or params for bluecode_apply_variables."));
        return Error;
    }

    const TArray<TSharedPtr<FJsonValue>>* Variables = nullptr;
    if (!Params->TryGetArrayField(TEXT("variables"), Variables) || !Variables)
    {
        TSharedPtr<FJsonObject> Error = MakeShared<FJsonObject>();
        Error->SetBoolField(TEXT("success"), false);
        Error->SetStringField(TEXT("error"), TEXT("bluecode_apply_variables requires a variables array."));
        return Error;
    }

    TArray<TSharedPtr<FJsonValue>> Operations;
    int32 AppliedCount = 0;

    for (const TSharedPtr<FJsonValue>& VariableValue : *Variables)
    {
        const TSharedPtr<FJsonObject> VariableObj = VariableValue.IsValid() ? VariableValue->AsObject() : nullptr;
        if (!VariableObj.IsValid())
        {
            continue;
        }

        TSharedPtr<FJsonObject> ResultObj = AddVariable(Blueprint, VariableObj);
        TSharedPtr<FJsonObject> Operation = MakeShared<FJsonObject>();
        Operation->SetObjectField(TEXT("variable"), VariableObj);
        if (ResultObj.IsValid())
        {
            Operation->SetObjectField(TEXT("result"), ResultObj);
            bool bOk = false;
            ResultObj->TryGetBoolField(TEXT("success"), bOk);
            if (bOk)
            {
                ++AppliedCount;
            }
        }
        Operations.Add(MakeShared<FJsonValueObject>(Operation));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), AppliedCount == Operations.Num());
    Result->SetNumberField(TEXT("applied_count"), AppliedCount);
    Result->SetArrayField(TEXT("operations"), Operations);
    return Result;
}

TSharedPtr<FJsonObject> UUmgBlueprintFunctionSubsystem::DeleteBluecode(UBlueprint* Blueprint, UEdGraph* Graph, const TSharedPtr<FJsonObject>& Params)
{
    if (!Blueprint || !Graph || !Params.IsValid())
    {
        TSharedPtr<FJsonObject> Error = MakeShared<FJsonObject>();
        Error->SetBoolField(TEXT("success"), false);
        Error->SetStringField(TEXT("error"), TEXT("Missing blueprint, graph, or params for bluecode_delete."));
        return Error;
    }

    bool bConfirmDelete = false;
    Params->TryGetBoolField(TEXT("confirm_delete"), bConfirmDelete);
    if (!bConfirmDelete)
    {
        TSharedPtr<FJsonObject> Error = MakeShared<FJsonObject>();
        Error->SetBoolField(TEXT("success"), false);
        Error->SetStringField(TEXT("error"), TEXT("bluecode_delete requires confirm_delete=true."));
        return Error;
    }

    const TArray<TSharedPtr<FJsonValue>>* Targets = nullptr;
    if (!Params->TryGetArrayField(TEXT("targets"), Targets) || !Targets)
    {
        TSharedPtr<FJsonObject> Error = MakeShared<FJsonObject>();
        Error->SetBoolField(TEXT("success"), false);
        Error->SetStringField(TEXT("error"), TEXT("bluecode_delete requires a targets array."));
        return Error;
    }

    TArray<TSharedPtr<FJsonValue>> Operations;
    TArray<TSharedPtr<FJsonValue>> Failures;
    int32 DeletedCount = 0;

    const TSharedPtr<FJsonObject>* AliasObj = nullptr;
    Params->TryGetObjectField(TEXT("aliases"), AliasObj);

    auto ResolveAlias = [&](const FString& MaybeAlias)
    {
        FString Resolved = MaybeAlias;
        if (AliasObj && AliasObj->IsValid())
        {
            FString AliasValue;
            if ((*AliasObj)->TryGetStringField(MaybeAlias, AliasValue) && !AliasValue.IsEmpty())
            {
                Resolved = AliasValue;
            }
        }
        return Resolved;
    };

    auto ParseEndpoint = [&](const FString& Endpoint, FString& OutNode, FString& OutPin)
    {
        if (Endpoint.IsEmpty())
        {
            return;
        }

        FString NodePart = Endpoint;
        FString PinPart;
        if (Endpoint.Split(TEXT(":"), &NodePart, &PinPart))
        {
            NodePart.TrimStartAndEndInline();
            PinPart.TrimStartAndEndInline();
            OutNode = ResolveAlias(NodePart);
            OutPin = PinPart;
            return;
        }

        NodePart.TrimStartAndEndInline();
        OutNode = ResolveAlias(NodePart);
    };

    auto PinMatchesDeleteRequest = [](const UEdGraphPin* Pin, const FString& RequestedPin)
    {
        return RequestedPin.IsEmpty() || BluecodePinNameMatches(Pin, RequestedPin);
    };

    auto PinEndpointText = [](const UEdGraphPin* Pin)
    {
        if (!Pin || !Pin->GetOwningNode())
        {
            return FString();
        }

        return FString::Printf(TEXT("%s:%s"),
            *Pin->GetOwningNode()->NodeGuid.ToString(),
            *Pin->PinName.ToString());
    };

    auto MakeConnectionCandidate = [&](const UEdGraphPin* SourcePin, const UEdGraphPin* TargetPin)
    {
        TSharedPtr<FJsonObject> Candidate = MakeShared<FJsonObject>();
        Candidate->SetStringField(TEXT("source"), PinEndpointText(SourcePin));
        Candidate->SetStringField(TEXT("target"), PinEndpointText(TargetPin));
        if (SourcePin && SourcePin->GetOwningNode())
        {
            Candidate->SetStringField(TEXT("source_title"), SourcePin->GetOwningNode()->GetNodeTitle(ENodeTitleType::ListView).ToString());
        }
        if (TargetPin && TargetPin->GetOwningNode())
        {
            Candidate->SetStringField(TEXT("target_title"), TargetPin->GetOwningNode()->GetNodeTitle(ENodeTitleType::ListView).ToString());
        }
        return Candidate;
    };

    auto ReadConnectionTarget = [&](const TSharedPtr<FJsonObject>& TargetObj, FString& SourceNode, FString& SourcePin, FString& TargetNode, FString& TargetPin)
    {
        if (!TargetObj.IsValid())
        {
            return;
        }

        FString SourceEndpoint;
        if (!TargetObj->TryGetStringField(TEXT("source"), SourceEndpoint))
        {
            TargetObj->TryGetStringField(TEXT("from"), SourceEndpoint);
        }
        if (!SourceEndpoint.IsEmpty())
        {
            ParseEndpoint(SourceEndpoint, SourceNode, SourcePin);
        }

        FString TargetEndpoint;
        if (!TargetObj->TryGetStringField(TEXT("target"), TargetEndpoint))
        {
            TargetObj->TryGetStringField(TEXT("to"), TargetEndpoint);
        }
        if (!TargetEndpoint.IsEmpty())
        {
            ParseEndpoint(TargetEndpoint, TargetNode, TargetPin);
        }

        FString FieldValue;
        if (SourceNode.IsEmpty() && (TargetObj->TryGetStringField(TEXT("source_node"), FieldValue) || TargetObj->TryGetStringField(TEXT("from_node"), FieldValue) || TargetObj->TryGetStringField(TEXT("nodeIdA"), FieldValue)))
        {
            SourceNode = ResolveAlias(FieldValue);
        }
        if (SourcePin.IsEmpty() && (TargetObj->TryGetStringField(TEXT("source_pin"), FieldValue) || TargetObj->TryGetStringField(TEXT("from_pin"), FieldValue) || TargetObj->TryGetStringField(TEXT("pinNameA"), FieldValue)))
        {
            SourcePin = FieldValue;
        }
        if (TargetNode.IsEmpty() && (TargetObj->TryGetStringField(TEXT("target_node"), FieldValue) || TargetObj->TryGetStringField(TEXT("to_node"), FieldValue) || TargetObj->TryGetStringField(TEXT("nodeIdB"), FieldValue)))
        {
            TargetNode = ResolveAlias(FieldValue);
        }
        if (TargetPin.IsEmpty() && (TargetObj->TryGetStringField(TEXT("target_pin"), FieldValue) || TargetObj->TryGetStringField(TEXT("to_pin"), FieldValue) || TargetObj->TryGetStringField(TEXT("pinNameB"), FieldValue)))
        {
            TargetPin = FieldValue;
        }
    };

    auto DeleteConnection = [&](const FString& SourceNodeName, const FString& SourcePinName, const FString& TargetNodeName, const FString& TargetPinName)
    {
        TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
        ResultObj->SetBoolField(TEXT("success"), false);
        ResultObj->SetStringField(TEXT("source"), FString::Printf(TEXT("%s:%s"), *SourceNodeName, *SourcePinName));
        ResultObj->SetStringField(TEXT("target"), FString::Printf(TEXT("%s:%s"), *TargetNodeName, *TargetPinName));

        if (SourceNodeName.IsEmpty() || TargetNodeName.IsEmpty())
        {
            ResultObj->SetStringField(TEXT("error"), TEXT("Connection delete requires source and target node endpoints."));
            return ResultObj;
        }

        UEdGraphNode* SourceNode = FindNodeByIdOrName(Graph, SourceNodeName);
        UEdGraphNode* TargetNode = FindNodeByIdOrName(Graph, TargetNodeName);
        if (!SourceNode)
        {
            ResultObj->SetStringField(TEXT("error"), FString::Printf(TEXT("Source node '%s' not found."), *SourceNodeName));
            return ResultObj;
        }
        if (!TargetNode)
        {
            ResultObj->SetStringField(TEXT("error"), FString::Printf(TEXT("Target node '%s' not found."), *TargetNodeName));
            return ResultObj;
        }

        TArray<TPair<UEdGraphPin*, UEdGraphPin*>> CandidatePairs;
        for (UEdGraphPin* SourcePinCandidate : SourceNode->Pins)
        {
            if (!SourcePinCandidate || SourcePinCandidate->Direction != EGPD_Output || !PinMatchesDeleteRequest(SourcePinCandidate, SourcePinName))
            {
                continue;
            }

            for (UEdGraphPin* LinkedPin : SourcePinCandidate->LinkedTo)
            {
                if (!LinkedPin || LinkedPin->Direction != EGPD_Input || LinkedPin->GetOwningNode() != TargetNode || !PinMatchesDeleteRequest(LinkedPin, TargetPinName))
                {
                    continue;
                }

                CandidatePairs.Add(TPair<UEdGraphPin*, UEdGraphPin*>(SourcePinCandidate, LinkedPin));
            }
        }

        if (CandidatePairs.Num() != 1)
        {
            TArray<TSharedPtr<FJsonValue>> CandidateValues;
            for (const TPair<UEdGraphPin*, UEdGraphPin*>& Pair : CandidatePairs)
            {
                CandidateValues.Add(MakeShared<FJsonValueObject>(MakeConnectionCandidate(Pair.Key, Pair.Value)));
            }

            ResultObj->SetArrayField(TEXT("candidates"), CandidateValues);
            ResultObj->SetStringField(TEXT("error"), CandidatePairs.Num() == 0
                ? TEXT("Connection target not found.")
                : TEXT("Connection delete target is ambiguous. Specify source_pin and target_pin."));
            return ResultObj;
        }

        UEdGraphPin* SourcePin = CandidatePairs[0].Key;
        UEdGraphPin* TargetPin = CandidatePairs[0].Value;
        if (!Graph->GetSchema())
        {
            ResultObj->SetStringField(TEXT("error"), TEXT("Graph schema is unavailable."));
            return ResultObj;
        }

        Graph->Modify();
        SourceNode->Modify();
        TargetNode->Modify();
        Graph->GetSchema()->BreakSinglePinLink(SourcePin, TargetPin);

        ResultObj->SetBoolField(TEXT("success"), true);
        ResultObj->SetStringField(TEXT("kind"), TEXT("connection"));
        ResultObj->SetStringField(TEXT("source"), PinEndpointText(SourcePin));
        ResultObj->SetStringField(TEXT("target"), PinEndpointText(TargetPin));
        return ResultObj;
    };

    auto HasVariable = [Blueprint](const FString& Name)
    {
        return Blueprint->NewVariables.ContainsByPredicate([&](const FBPVariableDescription& Var) {
            return Var.VarName.ToString().Equals(Name, ESearchCase::CaseSensitive);
        });
    };

    for (const TSharedPtr<FJsonValue>& TargetValue : *Targets)
    {
        FString Kind;
        FString NameOrId;
        FString SourceNode;
        FString SourcePin;
        FString TargetNode;
        FString TargetPin;
        bool bConnectionTarget = false;

        if (TargetValue.IsValid() && TargetValue->Type == EJson::Object)
        {
            TSharedPtr<FJsonObject> TargetObj = TargetValue->AsObject();
            if (TargetObj.IsValid())
            {
                TargetObj->TryGetStringField(TEXT("kind"), Kind);
                if (Kind.IsEmpty()) TargetObj->TryGetStringField(TEXT("type"), Kind);
                TargetObj->TryGetStringField(TEXT("node_id"), NameOrId);
                if (NameOrId.IsEmpty()) TargetObj->TryGetStringField(TEXT("nodeId"), NameOrId);
                if (NameOrId.IsEmpty()) TargetObj->TryGetStringField(TEXT("id"), NameOrId);
                if (NameOrId.IsEmpty()) TargetObj->TryGetStringField(TEXT("name"), NameOrId);
                if (NameOrId.IsEmpty()) TargetObj->TryGetStringField(TEXT("variable"), NameOrId);
                bConnectionTarget =
                    Kind.Equals(TEXT("connection"), ESearchCase::IgnoreCase) ||
                    Kind.Equals(TEXT("link"), ESearchCase::IgnoreCase) ||
                    Kind.Equals(TEXT("edge"), ESearchCase::IgnoreCase) ||
                    (TargetObj->HasField(TEXT("source")) && TargetObj->HasField(TEXT("target"))) ||
                    (TargetObj->HasField(TEXT("from")) && TargetObj->HasField(TEXT("to")));
                if (bConnectionTarget)
                {
                    ReadConnectionTarget(TargetObj, SourceNode, SourcePin, TargetNode, TargetPin);
                }
            }
        }
        else if (TargetValue.IsValid())
        {
            NameOrId = TargetValue->AsString();
            FString SourceEndpoint;
            FString TargetEndpoint;
            if (NameOrId.Split(TEXT("->"), &SourceEndpoint, &TargetEndpoint))
            {
                SourceEndpoint.TrimStartAndEndInline();
                TargetEndpoint.TrimStartAndEndInline();
                ParseEndpoint(SourceEndpoint, SourceNode, SourcePin);
                ParseEndpoint(TargetEndpoint, TargetNode, TargetPin);
                Kind = TEXT("connection");
                bConnectionTarget = true;
            }
        }

        TSharedPtr<FJsonObject> Operation = MakeShared<FJsonObject>();
        Operation->SetStringField(TEXT("target"), NameOrId);
        Operation->SetStringField(TEXT("kind"), Kind);

        if (bConnectionTarget)
        {
            TSharedPtr<FJsonObject> ResultObj = DeleteConnection(SourceNode, SourcePin, TargetNode, TargetPin);
            bool bOk = false;
            if (ResultObj.IsValid())
            {
                ResultObj->TryGetBoolField(TEXT("success"), bOk);
                Operation->SetObjectField(TEXT("result"), ResultObj);
                Operation->SetStringField(TEXT("source"), FString::Printf(TEXT("%s:%s"), *SourceNode, *SourcePin));
                Operation->SetStringField(TEXT("target"), FString::Printf(TEXT("%s:%s"), *TargetNode, *TargetPin));
            }
            if (bOk)
            {
                ++DeletedCount;
            }
            else
            {
                FString ErrorMessage = TEXT("Delete connection failed.");
                if (ResultObj.IsValid())
                {
                    ResultObj->TryGetStringField(TEXT("error"), ErrorMessage);
                }
                TSharedPtr<FJsonObject> Failure = MakeShared<FJsonObject>();
                Failure->SetStringField(TEXT("target"), FString::Printf(TEXT("%s:%s -> %s:%s"), *SourceNode, *SourcePin, *TargetNode, *TargetPin));
                Failure->SetStringField(TEXT("error"), ErrorMessage);
                if (ResultObj.IsValid() && ResultObj->HasField(TEXT("candidates")))
                {
                    const TArray<TSharedPtr<FJsonValue>>* CandidateValues = nullptr;
                    if (ResultObj->TryGetArrayField(TEXT("candidates"), CandidateValues) && CandidateValues)
                    {
                        Failure->SetArrayField(TEXT("candidates"), *CandidateValues);
                    }
                }
                Failures.Add(MakeShared<FJsonValueObject>(Failure));
            }
            Operations.Add(MakeShared<FJsonValueObject>(Operation));
            continue;
        }

        if (NameOrId.IsEmpty())
        {
            TSharedPtr<FJsonObject> Failure = MakeShared<FJsonObject>();
            Failure->SetStringField(TEXT("target"), NameOrId);
            Failure->SetStringField(TEXT("error"), TEXT("Empty delete target."));
            Failures.Add(MakeShared<FJsonValueObject>(Failure));
            Operation->SetObjectField(TEXT("result"), Failure);
            Operations.Add(MakeShared<FJsonValueObject>(Operation));
            continue;
        }

        const bool bNodeMatch = FindNodeByIdOrName(Graph, NameOrId) != nullptr;
        const bool bVariableMatch = HasVariable(NameOrId);
        const bool bRequestVariable = Kind.Equals(TEXT("variable"), ESearchCase::IgnoreCase);
        const bool bRequestNode = Kind.Equals(TEXT("node"), ESearchCase::IgnoreCase);

        if (!bRequestVariable && !bRequestNode && bNodeMatch && bVariableMatch)
        {
            TSharedPtr<FJsonObject> Failure = MakeShared<FJsonObject>();
            Failure->SetStringField(TEXT("target"), NameOrId);
            Failure->SetStringField(TEXT("error"), TEXT("Delete target is ambiguous between a node and a variable. Add kind='node' or kind='variable'."));
            Failures.Add(MakeShared<FJsonValueObject>(Failure));
            Operation->SetObjectField(TEXT("result"), Failure);
            Operations.Add(MakeShared<FJsonValueObject>(Operation));
            continue;
        }

        TSharedPtr<FJsonObject> ResultObj;
        if (bRequestVariable || (!bRequestNode && bVariableMatch))
        {
            TSharedPtr<FJsonObject> DeletePayload = MakeShared<FJsonObject>();
            DeletePayload->SetStringField(TEXT("name"), NameOrId);
            ResultObj = DeleteVariable(Blueprint, DeletePayload);
        }
        else if (bRequestNode || bNodeMatch)
        {
            TSharedPtr<FJsonObject> DeletePayload = MakeShared<FJsonObject>();
            DeletePayload->SetStringField(TEXT("nodeId"), NameOrId);
            ResultObj = DeleteNode(Blueprint, Graph, DeletePayload);
        }
        else
        {
            ResultObj = MakeShared<FJsonObject>();
            ResultObj->SetBoolField(TEXT("success"), false);
            ResultObj->SetStringField(TEXT("error"), TEXT("Delete target not found."));
        }

        bool bOk = false;
        if (ResultObj.IsValid())
        {
            ResultObj->TryGetBoolField(TEXT("success"), bOk);
            Operation->SetObjectField(TEXT("result"), ResultObj);
        }
        if (bOk)
        {
            ++DeletedCount;
        }
        else
        {
            FString ErrorMessage = TEXT("Delete failed.");
            if (ResultObj.IsValid())
            {
                ResultObj->TryGetStringField(TEXT("error"), ErrorMessage);
            }
            TSharedPtr<FJsonObject> Failure = MakeShared<FJsonObject>();
            Failure->SetStringField(TEXT("target"), NameOrId);
            Failure->SetStringField(TEXT("error"), ErrorMessage);
            Failures.Add(MakeShared<FJsonValueObject>(Failure));
        }
        Operations.Add(MakeShared<FJsonValueObject>(Operation));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), Failures.Num() == 0);
    Result->SetNumberField(TEXT("deleted_count"), DeletedCount);
    Result->SetArrayField(TEXT("operations"), Operations);
    Result->SetArrayField(TEXT("failures"), Failures);
    return Result;
}

TSharedPtr<FJsonObject> UUmgBlueprintFunctionSubsystem::SearchNodes(UEdGraph* Graph, UBlueprint* Blueprint, const TSharedPtr<FJsonObject>& Params)
{
    FString Query;
    Params->TryGetStringField(TEXT("query"), Query);
    if (Query.IsEmpty()) Params->TryGetStringField(TEXT("name"), Query);

    FString Category;
    Params->TryGetStringField(TEXT("category"), Category);

    FString NodeClass;
    Params->TryGetStringField(TEXT("nodeClass"), NodeClass);
    if (NodeClass.IsEmpty()) Params->TryGetStringField(TEXT("node_class"), NodeClass);
    FString NodeClassPath;
    Params->TryGetStringField(TEXT("nodeClassPath"), NodeClassPath);
    if (NodeClassPath.IsEmpty()) Params->TryGetStringField(TEXT("node_class_path"), NodeClassPath);

    bool bExact = false;
    Params->TryGetBoolField(TEXT("exact"), bExact);

    bool bIncludePins = false;
    Params->TryGetBoolField(TEXT("include_pins"), bIncludePins);

    bool bContextSensitive = true;
    Params->TryGetBoolField(TEXT("context_sensitive"), bContextSensitive);

    int32 MaxCount = 50;
    double MaxCountNumber = 0.0;
    if (Params->TryGetNumberField(TEXT("max_count"), MaxCountNumber))
    {
        MaxCount = FMath::Clamp(static_cast<int32>(MaxCountNumber), 1, 200);
    }

    UBlueprint* TargetBlueprint = Blueprint ? Cast<UBlueprint>(Blueprint) : ResolveBlueprintFromGraph(Graph);
    FBlueprintActionMenuBuilder Menu;
    BuildBlueprintActionMenu(TargetBlueprint, Graph, bContextSensitive, Menu);

    TArray<TSharedPtr<FJsonValue>> ActionsArray;
    int32 TotalMatches = 0;

    for (int32 Index = 0; Index < Menu.GetNumActions(); ++Index)
    {
        TSharedPtr<FEdGraphSchemaAction> Action = Menu.GetSchemaAction(Index);
        if (!Action.IsValid() || Action->GetTypeId() != FBlueprintActionMenuItem::StaticGetTypeId())
        {
            continue;
        }

        const TSharedPtr<FBlueprintActionMenuItem> MenuItem = StaticCastSharedPtr<FBlueprintActionMenuItem>(Action);
        const UBlueprintNodeSpawner* Spawner = MenuItem.IsValid() ? MenuItem->GetRawAction() : nullptr;
        const FString ActionNodeClass = (Spawner && Spawner->NodeClass) ? Spawner->NodeClass->GetName() : TEXT("");
        const FString ActionNodeClassPath = (Spawner && Spawner->NodeClass) ? Spawner->NodeClass->GetPathName() : TEXT("");

        const FString ActionName = Action->GetMenuDescription().ToString();
        const FString ActionCategory = Action->GetCategory().ToString();
        const FString ActionTooltip = Action->GetTooltipDescription().ToString();
        const FString ActionKeywords = Action->GetKeywords().ToString();
        const FString ActionSearchText = Action->GetFullSearchText();
        const FString ActionSignature = Spawner ? Spawner->GetSpawnerSignature().ToString() : TEXT("");

        const FString SearchBlob = FString::Printf(
            TEXT("%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s"),
            *ActionName,
            *ActionCategory,
            *ActionTooltip,
            *ActionKeywords,
            *ActionSearchText,
            *ActionSignature,
            *ActionNodeClass,
            *ActionNodeClassPath);

        const bool bMatchesQuery = Query.IsEmpty() ||
            (bExact
                ? ActionName.Equals(Query, ESearchCase::IgnoreCase) ||
                    NormalizeBluecodeStatement(ActionName) == NormalizeBluecodeStatement(Query) ||
                    ActionSignature.Equals(Query, ESearchCase::IgnoreCase) ||
                    NormalizeBluecodeStatement(ActionSignature) == NormalizeBluecodeStatement(Query) ||
                    ActionSearchText.Equals(Query, ESearchCase::IgnoreCase) ||
                    ActionNodeClass.Equals(Query, ESearchCase::IgnoreCase) ||
                    ActionNodeClassPath.Equals(Query, ESearchCase::IgnoreCase)
                : MatchesTextFilter(SearchBlob, Query, false));

        if (!bMatchesQuery)
        {
            continue;
        }
        if (!Category.IsEmpty() && !Action->GetCategory().ToString().Contains(Category, ESearchCase::IgnoreCase))
        {
            continue;
        }
        if (!NodeClass.IsEmpty() && !ActionNodeClass.Contains(NodeClass, ESearchCase::IgnoreCase))
        {
            continue;
        }
        if (!NodeClassPath.IsEmpty() && !ActionNodeClassPath.Contains(NodeClassPath, ESearchCase::IgnoreCase))
        {
            continue;
        }

        ++TotalMatches;
        if (ActionsArray.Num() < MaxCount)
        {
            ActionsArray.Add(MakeShared<FJsonValueObject>(ActionToJson(Action, Graph, bIncludePins)));
        }
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("query"), Query);
    Result->SetNumberField(TEXT("count"), ActionsArray.Num());
    Result->SetNumberField(TEXT("total_matches"), TotalMatches);
    Result->SetArrayField(TEXT("actions"), ActionsArray);
    Result->SetStringField(TEXT("usage"), TEXT("Pass a handle through bluecode_apply action_handles/action_hints, or as action_handle to compatibility add_step/prepare_value."));
    return Result;
}

TSharedPtr<FJsonObject> UUmgBlueprintFunctionSubsystem::FindFunctions(const TSharedPtr<FJsonObject>& Params, UBlueprint* Blueprint)
{
    FString Query, ClassName;
    Params->TryGetStringField(TEXT("query"), Query);
    Params->TryGetStringField(TEXT("className"), ClassName);

    TArray<UFunction*> Functions;

    // Naive search implementation
    TArray<UClass*> ListenClasses = {
        Blueprint ? Blueprint->ParentClass : nullptr,
        UKismetSystemLibrary::StaticClass(),
        UGameplayStatics::StaticClass(),
        UKismetMathLibrary::StaticClass()
    };

    TArray<TSharedPtr<FJsonValue>> FuncArray;

    for (UClass* Cls : ListenClasses)
    {
        if (!Cls) continue;
        for (TFieldIterator<UFunction> It(Cls); It; ++It)
        {
            UFunction* Func = *It;
            if (Func->HasAnyFunctionFlags(FUNC_BlueprintCallable | FUNC_BlueprintPure))
            {
                if (Query.IsEmpty() || Func->GetName().Contains(Query))
                {
                     TSharedPtr<FJsonObject> FuncObj = MakeShared<FJsonObject>();
                     FuncObj->SetStringField(TEXT("name"), Func->GetName());
                     FuncObj->SetStringField(TEXT("class"), Cls->GetName());
                     FuncArray.Add(MakeShared<FJsonValueObject>(FuncObj));

                     if (FuncArray.Num() > 50) break;
                }
            }
        }
        if (FuncArray.Num() > 50) break;
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetArrayField(TEXT("functions"), FuncArray);
    Result->SetBoolField(TEXT("success"), true);
    return Result;
}

UEdGraphNode* UUmgBlueprintFunctionSubsystem::FindNodeByIdOrName(UEdGraph* Graph, const FString& Id)
{
	if (!Graph) return nullptr;

    if (Id.IsEmpty()) return nullptr;

	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (Node->NodeGuid.ToString() == Id || Node->GetName() == Id)
		{
			return Node;
		}
	}
	return nullptr;
}


FString UUmgBlueprintFunctionSubsystem::EnsureFunctionExists(class UBlueprint* Blueprint, const FString& FunctionName, FString& OutStatus, const FString& ParametersJson)
{
     if (!Blueprint) return TEXT("");

     // 1. Check if Function Graph exists
     for (UEdGraph* Graph : Blueprint->FunctionGraphs)
     {
         if (Graph->GetName() == FunctionName)
         {
             OutStatus = TEXT("Found");
             // Return Last Node ID? Or Entry Node ID?
             // Usually Entry.
             for (UEdGraphNode* Node : Graph->Nodes)
             {
                 if (Node->IsA<UK2Node_FunctionEntry>())
                 {
                     return Node->NodeGuid.ToString();
                 }
             }
             return TEXT("");
         }
     }

     // 2. Check if it's an Event in the EventGraph
     // We scan the EventGraph for a CustomEvent with this name
     UEdGraph* EventGraph = FBlueprintEditorUtils::FindEventGraph(Blueprint);
     if (EventGraph)
     {
         for (UEdGraphNode* Node : EventGraph->Nodes)
         {
             if (UK2Node_CustomEvent* CustomEvent = Cast<UK2Node_CustomEvent>(Node))
             {
                 if (CustomEvent->CustomFunctionName.ToString() == FunctionName)
                 {
                     OutStatus = TEXT("Found (Event)");
                     return Node->NodeGuid.ToString();
                 }
             }
         }
     }

     // 3. Not found, Create New
     // Fallback to Creating a CUSTOM EVENT in EventGraph instead of a FunctionGraph
     // This avoids the crash issues with AddFunctionGraph and provides stability.

     if (EventGraph)
     {
         FGraphNodeCreator<UK2Node_CustomEvent> NodeCreator(*EventGraph);
         UK2Node_CustomEvent* NewEvent = NodeCreator.CreateNode(false);
         NewEvent->CustomFunctionName = FName(*FunctionName);
         NodeCreator.Finalize();

         // Auto-Layout: Find the lowest node to avoid overlapping
         float MaxY = -1000.0f;
         for (UEdGraphNode* ExistingNode : EventGraph->Nodes)
         {
             if (ExistingNode->NodePosY > MaxY)
             {
                 MaxY = ExistingNode->NodePosY;
             }
         }

         NewEvent->NodePosX = 0;
         NewEvent->NodePosY = MaxY + 200; // Place 200 units below the lowest node

         OutStatus = TEXT("Created (Event)");
         return NewEvent->NodeGuid.ToString();
     }

     OutStatus = TEXT("Error");
     return TEXT("");
}

FString UUmgBlueprintFunctionSubsystem::EnsureComponentEventExists(class UWidgetBlueprint* WidgetBlueprint, const FString& ComponentName, const FString& EventName, FString& OutStatus)
{
    UEdGraph* EventGraph = FBlueprintEditorUtils::FindEventGraph(WidgetBlueprint);
    if (!EventGraph) return TEXT("");

    // Check existing
    for (UEdGraphNode* Node : EventGraph->Nodes)
    {
        if (UK2Node_ComponentBoundEvent* BoundEvent = Cast<UK2Node_ComponentBoundEvent>(Node))
        {
            if (BoundEvent->ComponentPropertyName.ToString() == ComponentName &&
                BoundEvent->DelegatePropertyName.ToString() == EventName)
            {
                OutStatus = TEXT("Found");
                return Node->NodeGuid.ToString();
            }
        }
    }

    // Create New
    // We defer to the main logic which handles this better via HandleBlueprintGraphAction normally,
    // but here we just want to ensure it exists and get ID.
    // Re-use logic:

    FObjectProperty* ComponentProp = FindFProperty<FObjectProperty>(WidgetBlueprint->SkeletonGeneratedClass, *ComponentName);
    if (ComponentProp)
    {
         FMulticastDelegateProperty* DelegateProp = FindFProperty<FMulticastDelegateProperty>(ComponentProp->PropertyClass, *EventName);
         if (DelegateProp)
         {
             FGraphNodeCreator<UK2Node_ComponentBoundEvent> NodeCreator(*EventGraph);
             UK2Node_ComponentBoundEvent* BoundEventNode = NodeCreator.CreateNode(false);
             BoundEventNode->InitializeComponentBoundEventParams(ComponentProp, DelegateProp);

             NodeCreator.Finalize();

             OutStatus = TEXT("Created");
             return BoundEventNode->NodeGuid.ToString();
         }
    }


    OutStatus = TEXT("Error");
    return TEXT("");
}

// Helper for Levenshtein Distance
int32 LevenshteinDistance(const FString& s, const FString& t)
{
    if (s.IsEmpty()) return t.Len();
    if (t.IsEmpty()) return s.Len();

    int32 n = s.Len();
    int32 m = t.Len();

    // Configure matrix size (n+1) * (m+1)
    TArray<int32> d;
    d.SetNum((n + 1) * (m + 1));

    auto GetAt = [&](int32 i, int32 j) -> int32& { return d[i * (m + 1) + j]; };

    for (int32 i = 0; i <= n; ++i) GetAt(i, 0) = i;
    for (int32 j = 0; j <= m; ++j) GetAt(0, j) = j;

    for (int32 j = 1; j <= m; ++j)
    {
        for (int32 i = 1; i <= n; ++i)
        {
            int32 cost = (s[i - 1] == t[j - 1]) ? 0 : 1;
            GetAt(i, j) = FMath::Min3(
                GetAt(i - 1, j) + 1,       // deletion
                GetAt(i, j - 1) + 1,       // insertion
                GetAt(i - 1, j - 1) + cost // substitution
            );
        }
    }
    return GetAt(n, m);
}

TArray<FString> UUmgBlueprintFunctionSubsystem::GetFuzzySuggestions(const FString& SearchName, UClass* WidgetClass)
{
    TArray<FString> Suggestions;
    TArray<UClass*> TargetClasses = {
        WidgetClass,
        UKismetSystemLibrary::StaticClass(),
        UGameplayStatics::StaticClass(),
        UKismetMathLibrary::StaticClass()
    };

    struct FCandidate
    {
        FString Name;
        int32 Score;
    };
    TArray<FCandidate> Candidates;

    FString LowerSearch = SearchName.ToLower();

    for (UClass* Cls : TargetClasses)
    {
        if (!Cls) continue;
        for (TFieldIterator<UFunction> It(Cls); It; ++It)
        {
             UFunction* Func = *It;
             if (Func->HasAnyFunctionFlags(FUNC_BlueprintCallable | FUNC_BlueprintPure))
             {
                 FString FuncName = Func->GetName();
                 FString LowerFunc = FuncName.ToLower();

                 int32 Score = 1000;

                 // 1. Exact contains (Best match)
                 if (LowerFunc.Contains(LowerSearch))
                 {
                     Score = LevenshteinDistance(LowerSearch, LowerFunc); // Lower is better
                 }
                 // 2. Fuzzy
                 else
                 {
                     int32 Dist = LevenshteinDistance(LowerSearch, LowerFunc);
                     if (Dist < 5) // Threshold
                     {
                         Score = Dist + 50; // Penalize non-contains
                     }
                 }

                 if (Score < 100)
                 {
                     Candidates.Add({FuncName, Score});
                 }
             }
        }
    }

    // Sort by Score
    Candidates.Sort([](const FCandidate& A, const FCandidate& B) {
        return A.Score < B.Score;
    });

    // top 5
    for (int32 i = 0; i < FMath::Min(Candidates.Num(), 5); ++i)
    {
        Suggestions.Add(Candidates[i].Name);
    }

    return Suggestions;
}

#endif
