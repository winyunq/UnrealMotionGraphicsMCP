#include "Blueprint/UmgBlueprintGraphSubsystem.h"
#include "WidgetBlueprint.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Logging/LogMacros.h"
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

void UUmgBlueprintGraphSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	UE_LOG(LogUmgBlueprint, Log, TEXT("UmgBlueprintGraphSubsystem Initialized."));
}

void UUmgBlueprintGraphSubsystem::Deinitialize()
{
	UE_LOG(LogUmgBlueprint, Log, TEXT("UmgBlueprintGraphSubsystem Deinitialized."));
	Super::Deinitialize();
}

FString UUmgBlueprintGraphSubsystem::HandleBlueprintGraphAction(UWidgetBlueprint* WidgetBlueprint, const FString& Action, const FString& PayloadJson)
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
FString UUmgBlueprintGraphSubsystem::ExecuteGraphAction(UWidgetBlueprint* WidgetBlueprint, const TSharedPtr<FJsonObject>& Payload)
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
		// Inject default nodeType for function steps
		TSharedPtr<FJsonObject> NewPayload = MakeShared<FJsonObject>(*Payload);
		if (!NewPayload->HasField(TEXT("nodeType")))
		{
			NewPayload->SetStringField(TEXT("nodeType"), TEXT("CallFunction"));
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

UClass* UUmgBlueprintGraphSubsystem::ResolveUClass(const FString& ClassName)
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


TSharedPtr<FJsonObject> UUmgBlueprintGraphSubsystem::CreateNodeInstance(UEdGraph* TargetGraph, const TSharedPtr<FJsonObject>& Payload, UEdGraphNode*& OutNode)
{
    if (!TargetGraph || !Payload.IsValid()) return nullptr;
    
    FString NodeType;
    Payload->TryGetStringField(TEXT("nodeType"), NodeType);
    if (NodeType.IsEmpty()) Payload->TryGetStringField(TEXT("nodeName"), NodeType);
    
    const TArray<TSharedPtr<FJsonValue>>* InferenceArgs = nullptr;
    Payload->TryGetArrayField(TEXT("extraArgs"), InferenceArgs);

    UBlueprint* TargetBlueprint = Cast<UBlueprint>(TargetGraph->GetOuter());
    
    // Helper Setup Lambda
    auto SetupNode = [&](UEdGraphNode* Node) {
        if (!Node || !InferenceArgs) return;
        int32 ArgIndex = 0;
        
        for (UEdGraphPin* Pin : Node->Pins)
        {
            if (Pin->Direction == EGPD_Input && Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec && !Pin->bHidden)
            {
                if (ArgIndex < InferenceArgs->Num())
                {
                    FString ArgVal = (*InferenceArgs)[ArgIndex]->AsString();
                    
                    if (ArgVal.Equals("null", ESearchCase::IgnoreCase) || 
                        ArgVal.Equals("(null)", ESearchCase::IgnoreCase) ||
                        ArgVal.Equals("wait", ESearchCase::IgnoreCase) || 
                        ArgVal.Equals("(wait)", ESearchCase::IgnoreCase))
                    {
                        ArgIndex++;
                        continue; 
                    }

                    // --- SMART WIRING: Check if ArgVal is a Variable ---
                    bool bWired = false;
                    if (TargetBlueprint)
                    {
                         // Check for Member Variable
                         if (FindFProperty<FProperty>(TargetBlueprint->SkeletonGeneratedClass, *ArgVal))
                         {
                             FGraphNodeCreator<UK2Node_VariableGet> VarCreator(*TargetGraph);
                             UK2Node_VariableGet* GetNode = VarCreator.CreateNode(false);
                             GetNode->VariableReference.SetSelfMember(FName(*ArgVal));
                             GetNode->NodePosX = Node->NodePosX - 250;
                             GetNode->NodePosY = Node->NodePosY + (ArgIndex * 50); 
                             
                             VarCreator.Finalize();
                             
                             UEdGraphPin* OutPin = GetNode->GetValuePin();
                             if (OutPin)
                             {
                                 Node->GetSchema()->TryCreateConnection(OutPin, Pin);
                                 bWired = true;
                             }
                         }
                    }

                    if (!bWired && Pin->DefaultValue != ArgVal)
                    {
                        Pin->GetSchema()->TrySetDefaultValue(*Pin, ArgVal);
                    }
                    ArgIndex++;
                }
            }
        }
    };

    UEdGraphNode* NewNode = nullptr;

    if (NodeType == TEXT("CallFunction") || NodeType.StartsWith(TEXT("Call")))
    {
         FString MemberName;
		 Payload->TryGetStringField(TEXT("memberName"), MemberName);
         if (MemberName.IsEmpty()) Payload->TryGetStringField(TEXT("nodeName"), MemberName);
         if (MemberName.IsEmpty()) MemberName = NodeType; 
         
         FString MemberClass;
         Payload->TryGetStringField(TEXT("memberClass"), MemberClass);
         
         UFunction* FoundFunc = nullptr;
         UClass* FoundClass = nullptr;

         if (!MemberClass.IsEmpty())
         {
              UClass* Class = ResolveUClass(MemberClass);
              if (Class) FoundFunc = Class->FindFunctionByName(*MemberName);
         }
         else
         {
              // Smart Inference (Function Only)
              if (TargetBlueprint && TargetBlueprint->GeneratedClass)
              {
                  FoundFunc = TargetBlueprint->GeneratedClass->FindFunctionByName(*MemberName);
              }
              
              if (!FoundFunc)
              {
                  TArray<UClass*> Libs = { UKismetSystemLibrary::StaticClass(), UKismetMathLibrary::StaticClass(), UGameplayStatics::StaticClass() };
                  for (UClass* Lib : Libs) {
                      FoundFunc = Lib->FindFunctionByName(*MemberName);
                      if (FoundFunc) break; 
                  }
              }
         }

         if (FoundFunc)
         {
            FGraphNodeCreator<UK2Node_CallFunction> NodeCreator(*TargetGraph);
            UK2Node_CallFunction* CallFuncNode = NodeCreator.CreateNode(false);
            CallFuncNode->SetFromFunction(FoundFunc);
            NodeCreator.Finalize();
            SetupNode(CallFuncNode);
            NewNode = CallFuncNode;
         }
    }
    else if (NodeType == TEXT("ComponentBoundEvent"))
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
    else if (NodeType == TEXT("Get") || NodeType == TEXT("VariableGet"))
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
    else if (NodeType == TEXT("Set") || NodeType == TEXT("VariableSet"))
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
        OutNode = NewNode;
        TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
        Result->SetBoolField(TEXT("success"), true);
        Result->SetStringField(TEXT("nodeId"), NewNode->NodeGuid.ToString());
        Result->SetStringField(TEXT("nodeName"), NewNode->GetName());
        
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

TSharedPtr<FJsonObject> UUmgBlueprintGraphSubsystem::AddNode(UEdGraph* TargetGraph, const TSharedPtr<FJsonObject>& Payload)
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
    return Result;
}

TSharedPtr<FJsonObject> UUmgBlueprintGraphSubsystem::AddParam(UEdGraph* TargetGraph, const TSharedPtr<FJsonObject>& Payload)
{
    UEdGraphNode* NewNode = nullptr;
    TSharedPtr<FJsonObject> Result = CreateNodeInstance(TargetGraph, Payload, NewNode);
    if (!Result.IsValid()) return nullptr;

    // BACKWARD WIRING (Parameter)
    FString AutoConnectId;
     if (Payload->TryGetStringField(TEXT("autoConnectToNodeId"), AutoConnectId))
     {
          UEdGraphNode* ParentNode = FindNodeByIdOrName(TargetGraph, AutoConnectId);
          if (ParentNode)
          {
               // 1. Find the first available Input Pin on Parent (Target)
               UEdGraphPin* TargetInputPin = nullptr;
               for (UEdGraphPin* Pin : ParentNode->Pins)
               {
                   if (Pin->Direction == EGPD_Input && Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec && Pin->LinkedTo.Num() == 0 && !Pin->bHidden)
                   {
                       TargetInputPin = Pin;
                       break; // Found the hole to fill
                   }
               }
               
               // 2. Find the compatible Output Pin on NewNode (Source)
               UEdGraphPin* SourceOutputPin = nullptr;
               if (TargetInputPin)
               {
                   for (UEdGraphPin* Pin : NewNode->Pins)
                   {
                       if (Pin->Direction == EGPD_Output && Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec && !Pin->bHidden)
                       {
                           if (TargetGraph->GetSchema()->CanCreateConnection(Pin, TargetInputPin).Response != CONNECT_RESPONSE_DISALLOW)
                           {
                               SourceOutputPin = Pin;
                               break;
                           }
                       }
                   }
               }
               
               // 3. Connect
               if (TargetInputPin && SourceOutputPin)
               {
                   TargetGraph->GetSchema()->TryCreateConnection(SourceOutputPin, TargetInputPin);
                   
                   // Adjust Position: Place Parameter Node to the LEFT of Parent
                   NewNode->NodePosX = ParentNode->NodePosX - 250.0f;
                   NewNode->NodePosY = ParentNode->NodePosY + (ParentNode->Pins.Find(TargetInputPin) * 50.0f);
               }
          }
     }
     return Result;
}

UEdGraphNode* UUmgBlueprintGraphSubsystem::FindNodeByIdOrName(UEdGraph* Graph, const FString& Id)
{
    if (!Graph || Id.IsEmpty()) return nullptr;
    
    for (UEdGraphNode* Node : Graph->Nodes)
    {
        if (Node->NodeGuid.ToString() == Id || Node->GetName() == Id)
        {
            return Node;
        }
    }
    return nullptr;
}

TSharedPtr<FJsonObject> UUmgBlueprintGraphSubsystem::ConnectPins(UEdGraph* Graph, const TSharedPtr<FJsonObject>& Payload)
{
	FString FromNodeId, FromPinName, ToNodeId, ToPinName;
	Payload->TryGetStringField(TEXT("fromNodeId"), FromNodeId);
	Payload->TryGetStringField(TEXT("fromPinName"), FromPinName);
	Payload->TryGetStringField(TEXT("toNodeId"), ToNodeId);
	Payload->TryGetStringField(TEXT("toPinName"), ToPinName);

    UEdGraphNode* FromNode = FindNodeByIdOrName(Graph, FromNodeId);
    UEdGraphNode* ToNode = FindNodeByIdOrName(Graph, ToNodeId);
    
    if (FromNode && ToNode)
    {
        UEdGraphPin* FromPin = FromNode->FindPin(*FromPinName);
        UEdGraphPin* ToPin = ToNode->FindPin(*ToPinName);
        
        if (FromPin && ToPin)
        {
            Graph->GetSchema()->TryCreateConnection(FromPin, ToPin);
            
            TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
            Result->SetBoolField(TEXT("success"), true);
            return Result; 
        }
    }
    return nullptr;
}


TSharedPtr<FJsonObject> UUmgBlueprintGraphSubsystem::SetNodeProperty(UBlueprint* Blueprint, UEdGraph* Graph, const TSharedPtr<FJsonObject>& Payload)
{
     FString NodeId;
    Payload->TryGetStringField(TEXT("nodeId"), NodeId);
    FString PropertyName;
    Payload->TryGetStringField(TEXT("propertyName"), PropertyName);
    FString Value;
    Payload->TryGetStringField(TEXT("value"), Value);

    UEdGraphNode *TargetNode = FindNodeByIdOrName(Graph, NodeId);
    if(TargetNode)
    {
        TargetNode->Modify();
        if (PropertyName.Equals(TEXT("Comment"), ESearchCase::IgnoreCase)) {
            TargetNode->NodeComment = Value;
        }
        else if (PropertyName.Equals(TEXT("X"), ESearchCase::IgnoreCase)) {
            TargetNode->NodePosX = FCString::Atof(*Value);
        }
        else if (PropertyName.Equals(TEXT("Y"), ESearchCase::IgnoreCase)) {
            TargetNode->NodePosY = FCString::Atof(*Value);
        }
        
        TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
        Result->SetBoolField(TEXT("success"), true);
        return Result; 
    }
    return nullptr;
}

TSharedPtr<FJsonObject> UUmgBlueprintGraphSubsystem::DeleteNode(UBlueprint* Blueprint, UEdGraph* Graph, const TSharedPtr<FJsonObject>& Payload)
{
    // Simple implementation
     FString NodeId;
    Payload->TryGetStringField(TEXT("nodeId"), NodeId);
    
    UEdGraphNode *TargetNode = FindNodeByIdOrName(Graph, NodeId);
    if(TargetNode)
    {
        // Smart Delete: Attempt to bridge connections (Exec only)
        UEdGraphPin* PromoExecIn = nullptr;
        UEdGraphPin* PromoExecOut = nullptr;

        // 1. Find the Input Exec (Where we came from)
        for (UEdGraphPin* Pin : TargetNode->Pins)
        {
            if (Pin->Direction == EGPD_Input && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
            {
                 if (Pin->LinkedTo.Num() > 0)
                 {
                     PromoExecIn = Pin->LinkedTo[0]; // The Output Pin of the Previous Node
                     break; 
                 }
            }
        }

        // 2. Find the Output Exec (Where we go to)
        for (UEdGraphPin* Pin : TargetNode->Pins)
        {
            if (Pin->Direction == EGPD_Output && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
            {
                 // Check if connected
                 if (Pin->LinkedTo.Num() > 0)
                 {
                     PromoExecOut = Pin->LinkedTo[0]; // The Input Pin of the Next Node
                     break; 
                 }
            }
        }

        // 3. Remove Node (Breaking connections)
        FBlueprintEditorUtils::RemoveNode(Blueprint, TargetNode, true);
        
        // 4. Reconnect Bridge
        if (PromoExecIn && PromoExecOut)
        {
             Graph->GetSchema()->TryCreateConnection(PromoExecIn, PromoExecOut);
        }

        TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
        Result->SetBoolField(TEXT("success"), true);
        
        if (PromoExecIn)
        {
            // Suggest Backtrack to Previous Node
            Result->SetStringField(TEXT("newCursorNode"), PromoExecIn->GetOwningNode()->NodeGuid.ToString());
        }
        
        return Result; 
    }
    return nullptr;
}


TSharedPtr<FJsonObject> UUmgBlueprintGraphSubsystem::FindFunctions(const TSharedPtr<FJsonObject>& Payload, UWidgetBlueprint* WidgetBlueprint)
{
    FString Query;
    Payload->TryGetStringField(TEXT("query"), Query);
    FString ReturnTypeFilter; // e.g., "String", "Vector", "Bool"
    Payload->TryGetStringField(TEXT("returnType"), ReturnTypeFilter);
    
    // Classes to search
    TArray<UClass*> SearchClasses = {
        UKismetSystemLibrary::StaticClass(),
        UKismetMathLibrary::StaticClass(),
        UGameplayStatics::StaticClass()
    };
    
    // Add the Blueprint's Own Class (User Functions)
    if (WidgetBlueprint && WidgetBlueprint->GeneratedClass)
    {
        SearchClasses.Add(WidgetBlueprint->GeneratedClass);
    }
    
    TArray<TSharedPtr<FJsonValue>> Matches;
    
    for (UClass* Class : SearchClasses)
    {
        for (TFieldIterator<UFunction> It(Class); It; ++It)
        {
            UFunction* Func = *It;
            if (Func->HasAnyFunctionFlags(FUNC_BlueprintCallable | FUNC_BlueprintPure))
            {
                // Name Check
                if (!Query.IsEmpty() && !Func->GetName().Contains(Query))
                {
                    continue;
                }
                
                // Return Type Check
                FProperty* RetProp = Func->GetReturnProperty();
                if (!ReturnTypeFilter.IsEmpty())
                {
                    if (!RetProp) continue; // Void function
                    
                    FString PropType = RetProp->GetCPPType(); // "FString", "bool", etc
                    // Simple heuristic mapping
                     if (ReturnTypeFilter == "String" && PropType != "FString") continue;
                     if (ReturnTypeFilter == "Bool" && PropType != "bool") continue;
                     if (ReturnTypeFilter == "Float" && PropType != "float" && PropType != "double") continue;
                     if (ReturnTypeFilter == "Vector" && PropType != "FVector") continue;
                }
                
                // Build Result
                TSharedPtr<FJsonObject> FuncObj = MakeShared<FJsonObject>();
                FuncObj->SetStringField(TEXT("name"), Func->GetName());
                FuncObj->SetStringField(TEXT("class"), Class->GetName());
                
                // Params
                 TArray<TSharedPtr<FJsonValue>> ParamsArray;
                 for (TFieldIterator<FProperty> ParamIt(Func); ParamIt; ++ParamIt)
                 {
                     FProperty* Param = *ParamIt;
                     if (Param->HasAnyPropertyFlags(CPF_Parm) && !Param->HasAnyPropertyFlags(CPF_ReturnParm))
                     {
                         TSharedPtr<FJsonObject> PObj = MakeShared<FJsonObject>();
                         PObj->SetStringField(TEXT("name"), Param->GetName());
                         PObj->SetStringField(TEXT("type"), Param->GetCPPType());
                         ParamsArray.Add(MakeShared<FJsonValueObject>(PObj));
                     }
                 }
                 FuncObj->SetArrayField(TEXT("parameters"), ParamsArray);
                 
                 if (RetProp)
                 {
                     FuncObj->SetStringField(TEXT("returnType"), RetProp->GetCPPType());
                 }
                 else
                 {
                     FuncObj->SetStringField(TEXT("returnType"), TEXT("void"));
                 }
                 
                 Matches.Add(MakeShared<FJsonValueObject>(FuncObj));
                 
                 if (Matches.Num() >= 20) break; // Limit results
            }
        }
        if (Matches.Num() >= 20) break;
    }
    
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetArrayField(TEXT("functions"), Matches);
    return Result;
}


// ------------------------------------------------------------------------
// Function Creation Support
// ------------------------------------------------------------------------
FString UUmgBlueprintGraphSubsystem::EnsureFunctionExists(UWidgetBlueprint* WidgetBlueprint, const FString& FunctionName, FString& OutStatus, const FString& ParametersJson)
{
#if WITH_EDITOR
    if (!WidgetBlueprint) return TEXT("");

    UEdGraph* TargetGraph = nullptr;

    // Check existing function graphs
    for (UEdGraph* Graph : WidgetBlueprint->FunctionGraphs)
    {
        if (Graph->GetName() == FunctionName)
        {
            TargetGraph = Graph;
            break;
        }
    }
    // Check Ubergraph pages (if not Function)
    if (!TargetGraph)
    {
        for (UEdGraph* Graph : WidgetBlueprint->UbergraphPages)
        {
             if (Graph->GetName() == FunctionName)
             {
                 TargetGraph = Graph;
                 break;
             }
        }
    }

    if (TargetGraph)
    {
        OutStatus = TEXT("Found");
        
        // Find Entry Node
        UEdGraphNode* EntryNode = nullptr;
        for (UEdGraphNode* Node : TargetGraph->Nodes)
        {
             if (Node->IsA(UK2Node_FunctionEntry::StaticClass()))
             {
                 EntryNode = Node;
                 break;
             }
        }

        if (EntryNode)
        {
             // Traverse to find the last node
             UEdGraphNode* LastNode = EntryNode;
             int32 LoopGuard = 0;
             while (LoopGuard < 100) // Safety
             {
                 UEdGraphPin* ExecOut = nullptr;
                 for (UEdGraphPin* Pin : LastNode->Pins)
                 {
                     if (Pin->Direction == EGPD_Output && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
                     {
                         ExecOut = Pin;
                         break;
                     }
                 }

                 if (ExecOut && ExecOut->LinkedTo.Num() > 0)
                 {
                     LastNode = ExecOut->LinkedTo[0]->GetOwningNode();
                     LoopGuard++;
                 }
                 else
                 {
                     break;
                 }
             }
             return LastNode->NodeGuid.ToString();
        }
        return TEXT(""); // Found graph but no entry?
    }
    else
    {
        // CREATE FUNCTION
        UE_LOG(LogTemp, Display, TEXT("Creating new function: %s"), *FunctionName);
        
        // Use standard utility
        UEdGraph* NewGraph = nullptr;
        // Try FKismetEditorUtilities if possible, otherwise use FBlueprintEditorUtils with NULL template hack
        // Since we had template error, let's use explicit nullptr
        
        NewGraph = FBlueprintEditorUtils::CreateNewGraph(WidgetBlueprint, FName(*FunctionName), UEdGraphSchema_K2::StaticClass(), UEdGraphSchema_K2::StaticClass());
        // Add to function graphs
        FBlueprintEditorUtils::AddFunctionGraph<UClass>(WidgetBlueprint, NewGraph, true, nullptr);
        
        TargetGraph = NewGraph;
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBlueprint);
        
        OutStatus = TEXT("Created");

        // Find existing Entry node (created by Utils)
        for (UEdGraphNode* Node : TargetGraph->Nodes)
        {
             if (Node->IsA(UK2Node_FunctionEntry::StaticClass()))
             {
                 return Node->NodeGuid.ToString();
             }
        }
        return TEXT("");
    }
#else
    return TEXT("");
#endif
}

FString UUmgBlueprintGraphSubsystem::EnsureComponentEventExists(UWidgetBlueprint* WidgetBlueprint, const FString& ComponentName, const FString& EventName, FString& OutStatus)
{
#if WITH_EDITOR
    if (!WidgetBlueprint) return TEXT("");
    
    // Logic lives in EventGraph
    UEdGraph* EventGraph = FBlueprintEditorUtils::FindEventGraph(WidgetBlueprint);
    if (!EventGraph) return TEXT("");

    // 1. Search for existing bound event
    for (UEdGraphNode* Node : EventGraph->Nodes)
    {
        if (UK2Node_ComponentBoundEvent* BoundNode = Cast<UK2Node_ComponentBoundEvent>(Node))
        {
            if (BoundNode->ComponentPropertyName.ToString() == ComponentName && 
                BoundNode->DelegatePropertyName.ToString() == EventName)
            {
                OutStatus = TEXT("Found");
                // Traverse to find last node
                 UEdGraphNode* LastNode = BoundNode;
                 int32 LoopGuard = 0;
                 while (LoopGuard < 100) 
                 {
                     UEdGraphPin* ExecOut = nullptr;
                     for (UEdGraphPin* Pin : LastNode->Pins)
                     {
                         if (Pin->Direction == EGPD_Output && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
                         {
                             ExecOut = Pin;
                             break;
                         }
                     }

                     if (ExecOut && ExecOut->LinkedTo.Num() > 0)
                     {
                         LastNode = ExecOut->LinkedTo[0]->GetOwningNode();
                         LoopGuard++;
                     }
                     else
                     {
                         break;
                     }
                 }
                 return LastNode->NodeGuid.ToString();
            }
        }
    }

    // 2. Create if missing
    OutStatus = TEXT("Created");
    FObjectProperty* ComponentProp = FindFProperty<FObjectProperty>(WidgetBlueprint->SkeletonGeneratedClass, *ComponentName);
    if (!ComponentProp) return TEXT("");

    FMulticastDelegateProperty* DelegateProp = FindFProperty<FMulticastDelegateProperty>(ComponentProp->PropertyClass, *EventName);
    if (!DelegateProp) return TEXT("");

    FGraphNodeCreator<UK2Node_ComponentBoundEvent> NodeCreator(*EventGraph);
    UK2Node_ComponentBoundEvent* BoundEventNode = NodeCreator.CreateNode(false);
    BoundEventNode->ComponentPropertyName = FName(*ComponentName);
    BoundEventNode->DelegatePropertyName = FName(*EventName);
    
    // Calculate position (simple stack logic or offset)
    BoundEventNode->NodePosX = 0;
    BoundEventNode->NodePosY = EventGraph->Nodes.Num() * 200; // Stack vertically

    NodeCreator.Finalize();
    
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBlueprint);
    return BoundEventNode->NodeGuid.ToString();

#else
    return TEXT("");
#endif
}

// Helper needed? Or assume MatchPin exists? I need to implement MatchPin local helper or use iteration.
// Implementing MatchPin Lambda or helper inline for safety.


TSharedPtr<FJsonObject> UUmgBlueprintGraphSubsystem::AddVariable(class UWidgetBlueprint* Blueprint, const TSharedPtr<FJsonObject>& Payload)
{
    FString VarName;
    Payload->TryGetStringField(TEXT("name"), VarName);
    FString VarType;
    Payload->TryGetStringField(TEXT("type"), VarType); // e.g., "Boolean", "Float", "String"
    
    if (VarName.IsEmpty() || VarType.IsEmpty()) return nullptr;
    
    FEdGraphPinType PinType;
    PinType.PinCategory = UEdGraphSchema_K2::PC_Boolean; // Default
    
    if (VarType == "Boolean") PinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
    else if (VarType == "Float" || VarType == "Double") PinType.PinCategory = UEdGraphSchema_K2::PC_Real;
    else if (VarType == "Integer") PinType.PinCategory = UEdGraphSchema_K2::PC_Int;
    else if (VarType == "String") PinType.PinCategory = UEdGraphSchema_K2::PC_String;
    else if (VarType == "Text") PinType.PinCategory = UEdGraphSchema_K2::PC_Text;
    else if (VarType == "Vector") 
    {
         PinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
         PinType.PinSubCategoryObject = TBaseStructure<FVector>::Get();
    }
    else if (VarType == "Object") 
    {
         PinType.PinCategory = UEdGraphSchema_K2::PC_Object;
         FString SubType;
         if (Payload->TryGetStringField(TEXT("subType"), SubType))
         {
             PinType.PinSubCategoryObject = ResolveUClass(SubType);
         }
    }
    
    FBlueprintEditorUtils::AddMemberVariable(Blueprint, FName(*VarName), PinType);
    
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("name"), VarName);
    return Result;
}

TSharedPtr<FJsonObject> UUmgBlueprintGraphSubsystem::DeleteVariable(class UWidgetBlueprint* Blueprint, const TSharedPtr<FJsonObject>& Payload)
{
     FString VarName;
     Payload->TryGetStringField(TEXT("name"), VarName);
     if (VarName.IsEmpty()) Payload->TryGetStringField(TEXT("variableName"), VarName);

     if (!VarName.IsEmpty())
     {
         FBlueprintEditorUtils::RemoveMemberVariable(Blueprint, FName(*VarName));
         
         TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
         Result->SetBoolField(TEXT("success"), true);
         return Result;
     }
     return nullptr;
}

TSharedPtr<FJsonObject> UUmgBlueprintGraphSubsystem::GetVariables(class UWidgetBlueprint* Blueprint)
{
    TArray<TSharedPtr<FJsonValue>> VarsArray;
    
    for (FBPVariableDescription& VarDesc : Blueprint->NewVariables)
    {
        TSharedPtr<FJsonObject> VarObj = MakeShared<FJsonObject>();
        VarObj->SetStringField(TEXT("name"), VarDesc.VarName.ToString());
        VarObj->SetStringField(TEXT("type"), VarDesc.VarType.PinCategory.ToString());
        VarsArray.Add(MakeShared<FJsonValueObject>(VarObj));
    }
    
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetArrayField(TEXT("variables"), VarsArray);
    return Result;
}

TSharedPtr<FJsonObject> UUmgBlueprintGraphSubsystem::GetNodes(UEdGraph* Graph)
{
    if (!Graph) return nullptr;

    TArray<TSharedPtr<FJsonValue>> NodesArray;

    for (UEdGraphNode* Node : Graph->Nodes)
    {
        TSharedPtr<FJsonObject> NodeObj = MakeShared<FJsonObject>();
        NodeObj->SetStringField(TEXT("nodeId"), Node->NodeGuid.ToString());
        NodeObj->SetStringField(TEXT("name"), Node->GetNodeTitle(ENodeTitleType::ListView).ToString());
        NodeObj->SetStringField(TEXT("class"), Node->GetClass()->GetName());
        NodeObj->SetNumberField(TEXT("x"), Node->NodePosX);
        NodeObj->SetNumberField(TEXT("y"), Node->NodePosY);

        TArray<TSharedPtr<FJsonValue>> PinsArray;
        for (UEdGraphPin* Pin : Node->Pins)
        {
            TSharedPtr<FJsonObject> PinObj = MakeShared<FJsonObject>();
            PinObj->SetStringField(TEXT("name"), Pin->PinName.ToString());
            PinObj->SetStringField(TEXT("direction"), Pin->Direction == EGPD_Input ? TEXT("input") : TEXT("output"));
            PinObj->SetStringField(TEXT("type"), Pin->PinType.PinCategory.ToString());
            
            // Connected To?
            TArray<TSharedPtr<FJsonValue>> LinkedArray;
            for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
            {
                if (LinkedPin)
                {
                    TSharedPtr<FJsonObject> LinkObj = MakeShared<FJsonObject>();
                    LinkObj->SetStringField(TEXT("nodeId"), LinkedPin->GetOwningNode()->NodeGuid.ToString());
                    LinkObj->SetStringField(TEXT("pin"), LinkedPin->PinName.ToString());
                    LinkedArray.Add(MakeShared<FJsonValueObject>(LinkObj));
                }
            }
            PinObj->SetArrayField(TEXT("linkedTo"), LinkedArray);
            
            // Default Value?
            if (!Pin->DefaultValue.IsEmpty())
            {
                PinObj->SetStringField(TEXT("defaultValue"), Pin->DefaultValue);
            }
            
            PinsArray.Add(MakeShared<FJsonValueObject>(PinObj));
        }
        NodeObj->SetArrayField(TEXT("pins"), PinsArray);

        NodesArray.Add(MakeShared<FJsonValueObject>(NodeObj));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetArrayField(TEXT("nodes"), NodesArray);
    return Result;
}

#endif
