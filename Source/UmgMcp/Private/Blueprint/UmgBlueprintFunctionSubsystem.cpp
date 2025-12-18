#include "Blueprint/UmgBlueprintFunctionSubsystem.h"
#include "WidgetBlueprint.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Logging/LogMacros.h"
#include "FileManage/UmgAttentionSubsystem.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

// Editor includes
#if WITH_EDITOR
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraph/EdGraphSchema.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"

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
#include "K2Node_ExecutionSequence.h"
#include "K2Node_MakeArray.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogUmgBlueprint, Log, All);

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

FString UUmgBlueprintFunctionSubsystem::HandleBlueprintGraphAction(UWidgetBlueprint* WidgetBlueprint, const FString& Action, const FString& PayloadJson)
{
#if WITH_EDITOR
	if (!WidgetBlueprint)
	{
		return TEXT("{\"success\": false, \"error\": \"Invalid Blueprint\"}");
	}

	TSharedPtr<FJsonObject> Payload;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(PayloadJson);
	if (!FJsonSerializer::Deserialize(Reader, Payload) || !Payload.IsValid())
	{
		return TEXT("{\"success\": false, \"error\": \"Invalid JSON Payload\"}");
	}

	return ExecuteGraphAction(WidgetBlueprint, Payload);
#else
	return TEXT("{\"success\": false, \"error\": \"Editor Only\"}");
#endif
}

#if WITH_EDITOR
FString UUmgBlueprintFunctionSubsystem::ExecuteGraphAction(UWidgetBlueprint* WidgetBlueprint, const TSharedPtr<FJsonObject>& Payload)
{
	FString SubAction;
	Payload->TryGetStringField(TEXT("subAction"), SubAction);

	// Determine target graph
	FString GraphName;
	Payload->TryGetStringField(TEXT("graphName"), GraphName);
	
	UEdGraph* TargetGraph = nullptr;

	if (GraphName.IsEmpty() || GraphName.Equals(TEXT("EventGraph"), ESearchCase::IgnoreCase))
	{
		if (WidgetBlueprint->UbergraphPages.Num() > 0)
		{
			TargetGraph = WidgetBlueprint->UbergraphPages[0];
		}
	}
	else
	{
		for (UEdGraph* Graph : WidgetBlueprint->FunctionGraphs)
		{
			if (Graph->GetName() == GraphName)
			{
				TargetGraph = Graph;
				break;
			}
		}
		if (!TargetGraph)
		{
			for (UEdGraph* Graph : WidgetBlueprint->UbergraphPages)
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
                 !CurrentType.Equals("CustomEvent") && !CurrentType.Equals("ComponentBoundEvent"))
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
    else if (SubAction == TEXT("set_node_property"))
    {
        Result = SetNodeProperty(WidgetBlueprint, TargetGraph, Payload);
    }
    else if (SubAction == TEXT("delete_node"))
    {
        Result = DeleteNode(WidgetBlueprint, TargetGraph, Payload);
    }
    else if (SubAction == TEXT("find_functions") || SubAction == TEXT("search_function_library"))
    {
        Result = FindFunctions(Payload, WidgetBlueprint);
    }
    else if (SubAction == TEXT("add_variable"))
    {
        Result = AddVariable(WidgetBlueprint, Payload);
    }
    else if (SubAction == TEXT("delete_variable"))
    {
        Result = DeleteVariable(WidgetBlueprint, Payload);
    }
    else if (SubAction == TEXT("get_variables"))
    {
        Result = GetVariables(WidgetBlueprint);
    }
    else if (SubAction == TEXT("get_nodes"))
    {
        Result = GetNodes(TargetGraph);
    }

	if (Result.IsValid())
	{
        // Refresh UI (Skip for read-only actions)
        if (SubAction != TEXT("find_functions") && SubAction != TEXT("search_function_library") && 
            SubAction != TEXT("get_variables") && SubAction != TEXT("get_widget_tree") && 
            SubAction != TEXT("get_nodes"))
        {
             FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBlueprint);
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

    UBlueprint* TargetBlueprint = Cast<UBlueprint>(TargetGraph->GetOuter());
    
    // Helper Setup Lambda
    auto SetupNode = [&](UEdGraphNode* Node) {
        if (!Node) return;
        int32 ArgIndex = 0;
        
        for (UEdGraphPin* Pin : Node->Pins)
        {
            if (Pin->Direction == EGPD_Input && Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec && !Pin->bHidden)
            {
                FString ArgVal;
                bool bFoundVal = false;

                // 1. Try Named Argument (inputWires)
                if (InputWiresPtr && (*InputWiresPtr).IsValid())
                {
                    if ((*InputWiresPtr)->TryGetStringField(Pin->PinName.ToString(), ArgVal))
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
                     // 2. Try Smart Wiring (Member Variable)
                     // -----------------------------------------------------------
                     if (!bWired && TargetBlueprint)
                     {
                          if (FindFProperty<FProperty>(TargetBlueprint->SkeletonGeneratedClass, *ArgVal))
                          {
                               FGraphNodeCreator<UK2Node_VariableGet> VarCreator(*TargetGraph);
                               UK2Node_VariableGet* GetNode = VarCreator.CreateNode(false);
                               GetNode->VariableReference.SetSelfMember(FName(*ArgVal));
                               
                               // CONSUMPTION LAYOUT
                               GetNode->NodePosX = Node->NodePosX - 250;
                               GetNode->NodePosY = Node->NodePosY + (Pin->SourceIndex * 50); 
                               
                               VarCreator.Finalize();
                               
                               UEdGraphPin* OutPin = GetNode->GetValuePin();
                               if (OutPin)
                               {
                                   Node->GetSchema()->TryCreateConnection(OutPin, Pin);
                                   bWired = true;
                               }
                          }
                     }

                     // -----------------------------------------------------------
                     // 3. Fallback: Set Default Value (Literal)
                     // -----------------------------------------------------------
                     if (!bWired && Pin->DefaultValue != ArgVal)
                     {
                         Pin->GetSchema()->TrySetDefaultValue(*Pin, ArgVal);
                     }
                }
            }
        }
    };

    UEdGraphNode* NewNode = nullptr;

    // ----------------------------------------------------
    // Node Type Resolution & Fallback Logic
    // ----------------------------------------------------
    
    // 1. Try to resolve specific UK2Node class logic (e.g. ComponentBoundEvent, Event)
    // (Existing special cases kept for structure, but falling back to CallFunction is the key)
    
    // Explicit Special Cases
    if (NodeType == TEXT("Branch") || NodeType == TEXT("If"))
    {
         FGraphNodeCreator<UK2Node_IfThenElse> NodeCreator(*TargetGraph);
         UK2Node_IfThenElse* BranchNode = NodeCreator.CreateNode(false);
         NodeCreator.Finalize();
         SetupNode(BranchNode);
         NewNode = BranchNode;
    }
    else if (NodeType == TEXT("Sequence"))
    {
         FGraphNodeCreator<UK2Node_ExecutionSequence> NodeCreator(*TargetGraph);
         UK2Node_ExecutionSequence* SeqNode = NodeCreator.CreateNode(false);
         NodeCreator.Finalize();
         SetupNode(SeqNode);
         NewNode = SeqNode;
    }
    else if (NodeType == TEXT("CallFunction") || NodeType.StartsWith(TEXT("Call")) ||
        // Implicit Fallback: If it's not a special reserved keyword, assume it's a function name
        (!NodeType.Equals("ComponentBoundEvent") && !NodeType.Equals("Event") && 
         !NodeType.Equals("Get") && !NodeType.Equals("VariableGet") &&
         !NodeType.Equals("Set") && !NodeType.Equals("VariableSet") &&
         !NodeType.Equals("Cast") && !NodeType.Equals("DynamicCast")))
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
         
         // Priority 1: Self (Widget Class)
         if (TargetBlueprint && TargetBlueprint->GeneratedClass)
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
    else if (NodeType == TEXT("ComponentBoundEvent")) // FIX: Replaced unconditional block with else if
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
                     BoundEventNode->ComponentPropertyName = FName(*ComponentName);
                     BoundEventNode->DelegatePropertyName = FName(*EventName);
                     
                     NodeCreator.Finalize();
                     SetupNode(BoundEventNode);
                     NewNode = BoundEventNode;
                 }
            }
        }
    }
    else if (NodeType == TEXT("Event"))
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
    else if (NodeType == TEXT("Get") || NodeType == TEXT("VariableGet") || NodeType == TEXT("GetVariable"))
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
    else if (NodeType == TEXT("Set") || NodeType == TEXT("VariableSet") || NodeType == TEXT("SetVariable"))
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
    else if (NodeType == TEXT("Cast") || NodeType == TEXT("DynamicCast"))
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
        
        // Return detailed pin info
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
        Result->SetArrayField(TEXT("unconnectedInputs"), PinInfo);

        return Result;
	}

	return nullptr;
}

TSharedPtr<FJsonObject> UUmgBlueprintFunctionSubsystem::AddNode(UEdGraph* TargetGraph, const TSharedPtr<FJsonObject>& Payload)
{
    UEdGraphNode* NewNode = nullptr;
    TSharedPtr<FJsonObject> Result = CreateNodeInstance(TargetGraph, Payload, NewNode);
    if (!Result.IsValid()) return nullptr;
    
    // FORWARD WIRING (Sequence)
    FString AutoConnectId;
    if (Payload->TryGetStringField(TEXT("autoConnectToNodeId"), AutoConnectId))
    {
        UEdGraphNode* PrevNode = FindNodeByIdOrName(TargetGraph, AutoConnectId);
        if (PrevNode)
        {
                // Auto-Layout: Place new node to the right of Previous Node
                NewNode->NodePosX = PrevNode->NodePosX + 300;
                NewNode->NodePosY = PrevNode->NodePosY;

                // Wiring Logic
                UEdGraphPin* ExecOut = PrevNode->FindPin(UEdGraphSchema_K2::PN_Then);
                if (!ExecOut) 
                {
                     UEdGraphPin* FirstExec = nullptr;
                     for (UEdGraphPin* Pin : PrevNode->Pins)
                    {
                        if (Pin->Direction == EGPD_Output && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
                        {
                            if (!FirstExec) FirstExec = Pin; // Keep track of the first one (default)
                            
                            // Priority: First FREE pin (supports branching)
                            if (Pin->LinkedTo.Num() == 0)
                            {
                                ExecOut = Pin;
                                break;
                            }
                        }
                    }
                    // Default to first exec if all occupied (or none found)
                    if (!ExecOut) ExecOut = FirstExec;
                }
                // If PN_Then exists but is connected, try to find another free one?
                else if (ExecOut->LinkedTo.Num() > 0)
                {
                     // 'Then' is taken. Search for any other free Exec pin (e.g. False)
                     for (UEdGraphPin* Pin : PrevNode->Pins)
                     {
                         if (Pin->Direction == EGPD_Output && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec && Pin->LinkedTo.Num() == 0)
                         {
                             ExecOut = Pin;
                             break;
                         }
                     }
                }

            UEdGraphPin* ExecIn = NewNode->FindPin(UEdGraphSchema_K2::PN_Execute);
            if (!ExecIn) 
            {
                 for (UEdGraphPin* Pin : NewNode->Pins)
                {
                    if (Pin->Direction == EGPD_Input && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
                    {
                        ExecIn = Pin;
                        break;
                    }
                }
            }

            if (ExecOut && ExecIn)
            {
                TargetGraph->GetSchema()->TryCreateConnection(ExecOut, ExecIn);
            }
        }
    }
    else
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
              TargetPin->GetSchema()->TrySetDefaultValue(*TargetPin, ArgVal);
              bSuccess = true;
         }
    }
    
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), bSuccess);
    return Result;
}

TSharedPtr<FJsonObject> UUmgBlueprintFunctionSubsystem::ConnectPins(UEdGraph* Graph, const TSharedPtr<FJsonObject>& Params)
{
	FString NodeIdA, PinNameA, NodeIdB, PinNameB;
	Params->TryGetStringField(TEXT("nodeIdA"), NodeIdA);
	Params->TryGetStringField(TEXT("pinNameA"), PinNameA);
	Params->TryGetStringField(TEXT("nodeIdB"), NodeIdB);
	Params->TryGetStringField(TEXT("pinNameB"), PinNameB);

	UEdGraphNode* NodeA = FindNodeByIdOrName(Graph, NodeIdA);
	UEdGraphNode* NodeB = FindNodeByIdOrName(Graph, NodeIdB);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), false);

	if (NodeA && NodeB)
	{
		UEdGraphPin* PinA = NodeA->FindPin(FName(*PinNameA));
		UEdGraphPin* PinB = NodeB->FindPin(FName(*PinNameB));

		if (PinA && PinB)
		{
			Graph->GetSchema()->TryCreateConnection(PinA, PinB);
			Result->SetBoolField(TEXT("success"), true);
		}
	}
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
        // Simple type info?
        NodeObj->SetStringField(TEXT("class"), Node->GetClass()->GetName());
        
        bool bIsExec = false;
        for (UEdGraphPin* Pin : Node->Pins) { if (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec) { bIsExec = true; break; } }
        NodeObj->SetBoolField(TEXT("isExec"), bIsExec);
        
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
    return nullptr;
}

TSharedPtr<FJsonObject> UUmgBlueprintFunctionSubsystem::AddVariable(UWidgetBlueprint* Blueprint, const TSharedPtr<FJsonObject>& Params)
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
    
    FBlueprintEditorUtils::AddMemberVariable(Blueprint, FName(*Name), PinType);
    
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    return Result;
}

TSharedPtr<FJsonObject> UUmgBlueprintFunctionSubsystem::DeleteVariable(UWidgetBlueprint* Blueprint, const TSharedPtr<FJsonObject>& Params)
{
    FString Name;
    Params->TryGetStringField(TEXT("name"), Name);
    if (Name.IsEmpty()) return nullptr;
    
    FBlueprintEditorUtils::RemoveMemberVariable(Blueprint, FName(*Name));
    
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    return Result;
}

TSharedPtr<FJsonObject> UUmgBlueprintFunctionSubsystem::GetVariables(UWidgetBlueprint* Blueprint)
{
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    TArray<TSharedPtr<FJsonValue>> VarsArray;
    
    for (const FBPVariableDescription& Var : Blueprint->NewVariables)
    {
        TSharedPtr<FJsonObject> VarObj = MakeShared<FJsonObject>();
        VarObj->SetStringField(TEXT("name"), Var.VarName.ToString());
        VarObj->SetStringField(TEXT("type"), Var.VarType.PinCategory.ToString());
        VarsArray.Add(MakeShared<FJsonValueObject>(VarObj));
    }
    
    Result->SetArrayField(TEXT("variables"), VarsArray);
    Result->SetBoolField(TEXT("success"), true);
    return Result;
}

TSharedPtr<FJsonObject> UUmgBlueprintFunctionSubsystem::FindFunctions(const TSharedPtr<FJsonObject>& Params, UWidgetBlueprint* WidgetBlueprint)
{
    FString Query, ClassName;
    Params->TryGetStringField(TEXT("query"), Query);
    Params->TryGetStringField(TEXT("className"), ClassName);
    
    TArray<UFunction*> Functions;
    
    // Naive search implementation
    TArray<UClass*> ListenClasses = { 
        WidgetBlueprint ? WidgetBlueprint->ParentClass : nullptr,
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


FString UUmgBlueprintFunctionSubsystem::EnsureFunctionExists(class UWidgetBlueprint* WidgetBlueprint, const FString& FunctionName, FString& OutStatus, const FString& ParametersJson)
{
     if (!WidgetBlueprint) return TEXT("");
     
     // 1. Check if Function Graph exists
     for (UEdGraph* Graph : WidgetBlueprint->FunctionGraphs)
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
     UEdGraph* EventGraph = FBlueprintEditorUtils::FindEventGraph(WidgetBlueprint);
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
             BoundEventNode->ComponentPropertyName = FName(*ComponentName);
             BoundEventNode->DelegatePropertyName = FName(*EventName);
             
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
