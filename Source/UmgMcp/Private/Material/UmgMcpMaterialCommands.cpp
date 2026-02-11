#include "Material/UmgMcpMaterialCommands.h"
#include "Material/UmgMcpMaterialSubsystem.h" // Required for Subsystem usage
#include "Editor.h"

FUmgMcpMaterialCommands::FUmgMcpMaterialCommands()
{
}

FUmgMcpMaterialCommands::~FUmgMcpMaterialCommands()
{
}

UUmgMcpMaterialSubsystem* FUmgMcpMaterialCommands::GetSubsystem() const
{
    if (GEditor)
    {
        return GEditor->GetEditorSubsystem<UUmgMcpMaterialSubsystem>();
    }
    return nullptr;
}

TSharedPtr<FJsonObject> FUmgMcpMaterialCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> ResultJson = MakeShareable(new FJsonObject);
    UUmgMcpMaterialSubsystem* Subsystem = GetSubsystem();

    if (!Subsystem)
    {
        ResultJson->SetStringField(TEXT("error"), TEXT("Material Subsystem not available"));
        ResultJson->SetBoolField(TEXT("success"), false);
        return ResultJson;
    }

    // --- P0: Context ---
    if (CommandType == TEXT("material_set_target"))
    {
        FString Path;
        if (Params->TryGetStringField(TEXT("path"), Path))
        {
            // Default to always trying to create/load (Context Anchor)
            FString Status = Subsystem->SetTargetMaterial(Path, true);
            
            if (Status.StartsWith(TEXT("错误")) || Status.StartsWith(TEXT("Error")))
            {
                 ResultJson->SetStringField(TEXT("error"), Status);
                 ResultJson->SetBoolField(TEXT("success"), false);
            }
            else
            {
                 ResultJson->SetStringField(TEXT("message"), Status);
                 ResultJson->SetBoolField(TEXT("success"), true);
            }
        }
        else
        {
            ResultJson->SetStringField(TEXT("error"), TEXT("Missing 'path' parameter"));
            ResultJson->SetBoolField(TEXT("success"), false);
        }
    }
    // --- P0: Context & Search ---
    
    // Command: material_get_pins
    // Params: { "handle": "Master" | "NodeHandle" }
    else if (CommandType == TEXT("material_get_pins"))
    {
        FString Handle;
        if (Params->TryGetStringField(TEXT("handle"), Handle))
        {
            FString InfoJsonStr = Subsystem->GetNodeInfo(Handle);
            
            // Embed the result. The ResultJson already has "success".
            // We parse InfoJsonStr and add it to ResultJson.
            TSharedPtr<FJsonObject> InfoObj;
            TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(InfoJsonStr);
            if (FJsonSerializer::Deserialize(Reader, InfoObj))
            {
                 // Relay Error if present
                 if (InfoObj->HasField(TEXT("error")))
                 {
                     ResultJson->SetStringField(TEXT("error"), InfoObj->GetStringField(TEXT("error")));
                     ResultJson->SetBoolField(TEXT("success"), false);
                 }
                 else
                 {
                     // Flatten: Add "pins", "connections" and "name" to the top level result
                     if (InfoObj->HasField(TEXT("pins"))) 
                         ResultJson->SetArrayField(TEXT("pins"), InfoObj->GetArrayField(TEXT("pins")));
                         
                     if (InfoObj->HasField(TEXT("connections")))
                         ResultJson->SetObjectField(TEXT("connections"), InfoObj->GetObjectField(TEXT("connections")));
                         
                     if (InfoObj->HasField(TEXT("name")))
                         ResultJson->SetStringField(TEXT("name"), InfoObj->GetStringField(TEXT("name")));
                     
                     ResultJson->SetBoolField(TEXT("success"), true);
                 }
            }
            else
            {
                ResultJson->SetStringField(TEXT("error"), TEXT("Failed to deserialize node info"));
                ResultJson->SetBoolField(TEXT("success"), false);
            }
        }
        else
        {
             ResultJson->SetStringField(TEXT("error"), TEXT("Missing 'handle' parameter"));
             ResultJson->SetBoolField(TEXT("success"), false);
        }
    }

    // --- P1: Input Definition ---
    else if (CommandType == TEXT("material_define_variable"))
    {
        FString Name, Type;
        if (Params->TryGetStringField(TEXT("name"), Name) && Params->TryGetStringField(TEXT("type"), Type))
        {
            FString Handle = Subsystem->DefineVariable(Name, Type);
            if (Handle.StartsWith(TEXT("Error")))
            {
                ResultJson->SetStringField(TEXT("error"), Handle);
                ResultJson->SetBoolField(TEXT("success"), false);
            }
            else
            {
                ResultJson->SetStringField(TEXT("handle"), Handle);
                ResultJson->SetBoolField(TEXT("success"), true);
            }
        }
        else
        {
            ResultJson->SetStringField(TEXT("error"), TEXT("Missing 'name' or 'type'"));
            ResultJson->SetBoolField(TEXT("success"), false);
        }
    }
    // --- P2: Node Management ---
    else if (CommandType == TEXT("material_add_node"))
    {
        FString Symbol, HandleName;
        Params->TryGetStringField(TEXT("symbol"), Symbol);
        Params->TryGetStringField(TEXT("handle"), HandleName); // Optional
        
        if (!Symbol.IsEmpty())
        {
            FString NewHandle = Subsystem->AddNode(Symbol, HandleName);
            if (NewHandle.StartsWith(TEXT("Error")))
            {
                ResultJson->SetStringField(TEXT("error"), NewHandle);
                ResultJson->SetBoolField(TEXT("success"), false);
            }
            else
            {
                ResultJson->SetStringField(TEXT("handle"), NewHandle);
                ResultJson->SetBoolField(TEXT("success"), true);
            }
        }
        else
        {
            ResultJson->SetStringField(TEXT("error"), TEXT("Missing 'symbol'"));
            ResultJson->SetBoolField(TEXT("success"), false);
        }
    }
    else if (CommandType == TEXT("material_delete"))
    {
        FString Handle;
        if (Params->TryGetStringField(TEXT("handle"), Handle))
        {
            bool bSuccess = Subsystem->DeleteNode(Handle);
            ResultJson->SetBoolField(TEXT("success"), bSuccess);
        }
        else
        {
             ResultJson->SetStringField(TEXT("error"), TEXT("Missing 'handle'"));
             ResultJson->SetBoolField(TEXT("success"), false);
        }
    }
    // --- P3: Connections ---
    else if (CommandType == TEXT("material_connect_nodes"))
    {
        FString From, To;
        if (Params->TryGetStringField(TEXT("from"), From) && Params->TryGetStringField(TEXT("to"), To))
        {
            bool bSuccess = Subsystem->ConnectNodes(From, To);
            ResultJson->SetBoolField(TEXT("success"), bSuccess);
            if (!bSuccess) ResultJson->SetStringField(TEXT("error"), TEXT("Failed to connect nodes. Check Handles."));
        }
        else
        {
             ResultJson->SetStringField(TEXT("error"), TEXT("Missing 'from' or 'to' parameters"));
             ResultJson->SetBoolField(TEXT("success"), false);
        }
    }
    else if (CommandType == TEXT("material_connect_pins"))
    {
        FString Source, Target, SourcePin, TargetPin;
        Params->TryGetStringField(TEXT("source"), Source);
        Params->TryGetStringField(TEXT("target"), Target);
        Params->TryGetStringField(TEXT("source_pin"), SourcePin); // Optional
        Params->TryGetStringField(TEXT("target_pin"), TargetPin);
        
        bool bSuccess = Subsystem->ConnectPins(Source, SourcePin, Target, TargetPin);
        ResultJson->SetBoolField(TEXT("success"), bSuccess);
        if (!bSuccess) ResultJson->SetStringField(TEXT("error"), TEXT("Failed to connect pins. Check Pin Names."));
    }
    // --- P5: Detail Injection ---
    else if (CommandType == TEXT("material_set_hlsl_node_io"))
    {
        FString Handle, Code;
        const TArray<TSharedPtr<FJsonValue>>* InputsArray;
        
        if (Params->TryGetStringField(TEXT("handle"), Handle) && 
            Params->TryGetStringField(TEXT("code"), Code) &&
            Params->TryGetArrayField(TEXT("inputs"), InputsArray))
        {
            TArray<FString> Inputs;
            for (auto Val : *InputsArray)
            {
                Inputs.Add(Val->AsString());
            }
            
            bool bSuccess = Subsystem->SetCustomNodeHLSL(Handle, Code, Inputs);
            ResultJson->SetBoolField(TEXT("success"), bSuccess);
        }
        else
        {
             ResultJson->SetStringField(TEXT("error"), TEXT("Missing parameters for HLSL injection"));
             ResultJson->SetBoolField(TEXT("success"), false);
        }
    }
    else if (CommandType == TEXT("material_set_node_properties"))
    {
        FString Handle;
        const TSharedPtr<FJsonObject>* Props;
        
        if (Params->TryGetStringField(TEXT("handle"), Handle) && 
            Params->TryGetObjectField(TEXT("properties"), Props))
        {
             bool bSuccess = Subsystem->SetNodeProperties(Handle, *Props);
             ResultJson->SetBoolField(TEXT("success"), bSuccess);
        }
        else
        {
             ResultJson->SetStringField(TEXT("error"), TEXT("Missing handle or properties"));
             ResultJson->SetBoolField(TEXT("success"), false);
             ResultJson->SetBoolField(TEXT("success"), false);
        }
    }
    // --- P6: Output ---
    else if (CommandType == TEXT("material_set_output_node"))
    {
        FString Handle;
        if (Params->TryGetStringField(TEXT("handle"), Handle))
        {
             bool bSuccess = Subsystem->SetOutputNode(Handle);
             ResultJson->SetBoolField(TEXT("success"), bSuccess);
             if (!bSuccess) ResultJson->SetStringField(TEXT("error"), TEXT("Failed to set output node. Check Handle or Material Mode."));
        }
        else
        {
             ResultJson->SetStringField(TEXT("error"), TEXT("Missing 'handle'"));
             ResultJson->SetBoolField(TEXT("success"), false);
        }
    }
    // --- Lifecycle ---
    else if (CommandType == TEXT("material_compile_asset"))
    {
        FString Status = Subsystem->CompileAsset();
        ResultJson->SetStringField(TEXT("message"), Status);
        ResultJson->SetBoolField(TEXT("success"), true);
    }
    else
    {
        ResultJson->SetStringField(TEXT("error"), FString::Printf(TEXT("Unknown Material Command: %s"), *CommandType));
        ResultJson->SetBoolField(TEXT("success"), false);
    }

    return ResultJson;
}
