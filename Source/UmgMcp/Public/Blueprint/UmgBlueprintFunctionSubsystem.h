#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "Dom/JsonObject.h"
#include "UmgBlueprintFunctionSubsystem.generated.h"

class UWidgetBlueprint;

/**
 * @brief Provides stateless low-level Blueprint Graph manipulation capabilities.
 *
 * This subsystem acts as the "Hand" of the AI. It executes atomic graph operations
 * such as creating nodes, connecting pins, and deleting nodes. It does NOT maintain
 * state about the current graph or editing context; that is handled by the "Brain"
 * (UmgAttentionSubsystem).
 */
UCLASS()
class UMGMCP_API UUmgBlueprintFunctionSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/**
	 * Main entry point for Blueprint Graph automation actions.
	 * 
	 * @param WidgetBlueprint The target Blueprint asset (passed from Attention system).
	 * @param Action The specific action to perform (e.g., "manage_blueprint_graph").
	 * @param Payload The JSON payload containing commands like "create_node", "connect_pins".
	 * @return A JSON object containing the result or error details.
	 */
	UFUNCTION(BlueprintCallable, Category = "UMG MCP|Blueprint")
	FString HandleBlueprintGraphAction(class UWidgetBlueprint* WidgetBlueprint, const FString& Action, const FString& PayloadJson);

    /**
     * @brief Checks if a Function/Graph exists, and creates it if missing.
     * @param WidgetBlueprint The Blueprint to check.
     * @param FunctionName The name of the function/graph.
     * @param Parameters Optional: Definitions for Input/Output parameters if creating new.
     * @return True if exists or created successfully.
     */
    /**
     * @brief Checks if a Function/Graph exists, and creates it if missing. Returns the Start Node ID (Entry).
     * @param OutStatus Returns "Created", "Found", or "Inherited".
     * @return The GUID of the Cursor Node (Entry or Tail).
     */
    UFUNCTION(BlueprintCallable, Category = "UMG MCP|Blueprint")
    FString EnsureFunctionExists(class UWidgetBlueprint* WidgetBlueprint, const FString& FunctionName, FString& OutStatus, const FString& ParametersJson = TEXT("{}"));

    /**
     * @brief Checks/Creates a ComponentBoundEvent in the EventGraph.
     * @param OutStatus Returns "Created" or "Found".
     * @return The GUID of the Event node.
     */
    UFUNCTION(BlueprintCallable, Category = "UMG MCP|Blueprint")
    FString EnsureComponentEventExists(class UWidgetBlueprint* WidgetBlueprint, const FString& ComponentName, const FString& EventName, FString& OutStatus);

private:
#if WITH_EDITOR
	// Helpers ported from external reference
	TSharedPtr<FJsonObject> AddNode(UEdGraph* Graph, const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> AddParam(UEdGraph* Graph, const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> CreateNodeInstance(UEdGraph* Graph, const TSharedPtr<FJsonObject>& Params, class UEdGraphNode*& OutNode);
	TSharedPtr<FJsonObject> ConnectPins(UEdGraph* Graph, const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> GetNodes(UEdGraph* Graph);
	TSharedPtr<FJsonObject> DeleteNode(class UBlueprint* Blueprint, UEdGraph* Graph, const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> SetNodeProperty(class UBlueprint* Blueprint, UEdGraph* Graph, const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> AddVariable(class UWidgetBlueprint* Blueprint, const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> DeleteVariable(class UWidgetBlueprint* Blueprint, const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> GetVariables(class UWidgetBlueprint* Blueprint);
	TSharedPtr<FJsonObject> FindFunctions(const TSharedPtr<FJsonObject>& Params, UWidgetBlueprint* WidgetBlueprint);

    // Helpers
	class UEdGraphNode* FindNodeByIdOrName(UEdGraph* Graph, const FString& Id);
    UClass* ResolveUClass(const FString& ClassName);

    // Internal execution methods
	FString ExecuteGraphAction(UWidgetBlueprint* WidgetBlueprint, const TSharedPtr<FJsonObject>& Payload);

    // Fuzzy search helper
    TArray<FString> GetFuzzySuggestions(const FString& SearchName, UClass* WidgetClass);
#endif
};
