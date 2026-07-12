import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]


def require_text(path: str, needle: str) -> None:
    text = (ROOT / path).read_text(encoding="utf-8")
    if needle not in text:
        raise AssertionError(f"{needle!r} not found in {path}")


def forbid_text(path: str, needle: str) -> None:
    text = (ROOT / path).read_text(encoding="utf-8")
    if needle in text:
        raise AssertionError(f"{needle!r} should not remain in {path}")


def registered_tools() -> list[str]:
    server_text = (ROOT / "Resources/Python/UmgMcpServer.py").read_text(encoding="utf-8")
    names: list[str] = []
    for raw_line in server_text.splitlines():
        line = raw_line.strip()
        if line.startswith("@register_tool("):
            parts = line.split('"')
            if len(parts) >= 2:
                names.append(parts[1])
    return names


def main() -> None:
    prompts = json.loads((ROOT / "Resources/prompts.json").read_text(encoding="utf-8"))
    tools = {tool["name"]: tool for tool in prompts.get("tools", [])}
    enabled_tool_names = {name for name, tool in tools.items() if tool.get("enabled", True)}
    blueprint_mode = json.loads((ROOT / "Resources/ToolModes/Blueprint.json").read_text(encoding="utf-8"))
    allowed_tools = set(blueprint_mode.get("allowed_tools", []))

    missing_prompt_entries = [name for name in registered_tools() if name not in tools]
    if missing_prompt_entries:
        raise AssertionError(f"Registered tools missing prompts entries: {missing_prompt_entries}")

    required_tools = {
        "bluecode_set_function",
        "bluecode_read_function",
        "bluecode_read_variables",
        "bluecode_read_events",
        "bluecode_search_nodes",
        "bluecode_apply",
        "bluecode_apply_variables",
        "bluecode_connect",
        "bluecode_delete",
        "bluecode_compile",
        "search_function_library",
    }
    support_tools = {
        "set_target_umg_asset",
        "get_target_umg_asset",
        "set_target_widget",
        "get_target_widget",
        "save_asset",
    }
    hidden_compat_tools = {
        "set_edit_function",
        "add_step",
        "prepare_value",
        "connect_data_to_pin",
        "add_variable",
        "get_variables",
        "get_function_nodes",
        "set_cursor_node",
        "compile_blueprint",
        "delete_node",
        "delete_variable",
    }

    missing_prompts = required_tools - enabled_tool_names
    missing_mode = (required_tools | support_tools) - allowed_tools
    if missing_prompts:
        raise AssertionError(f"Missing enabled Blueprint prompt tools: {sorted(missing_prompts)}")
    if missing_mode:
        raise AssertionError(f"Missing Blueprint mode tools: {sorted(missing_mode)}")

    for name in sorted(hidden_compat_tools):
        tool = tools.get(name)
        if tool is None:
            raise AssertionError(f"Hidden compatibility tool missing prompts entry: {name}")
        if tool.get("enabled", True):
            raise AssertionError(f"Blueprint compatibility tool should be disabled in prompts: {name}")
        if name in allowed_tools:
            raise AssertionError(f"Blueprint compatibility tool should not be in Blueprint mode: {name}")

    unknown_mode_tools = allowed_tools - set(tools)
    if unknown_mode_tools:
        raise AssertionError(f"Blueprint mode references tools not present in prompts: {sorted(unknown_mode_tools)}")

    require_text("Document/BlueprintBluecodeProtocol.md", "bluecode_apply")
    require_text("Document/BlueprintBluecodeProtocol.md", "confirm_delete=true")
    require_text("Document/BlueprintBluecodeProtocol.md", "node(\"Action Menu Name\"")
    require_text("Document/BlueprintBluecodeProtocol.md", "value(\"Action Menu Name\"")
    require_text("Document/BlueprintBluecodeProtocol.md", "搜索结果对象")
    require_text("Document/BlueprintBluecodeProtocol.md", "`signature`")
    require_text("Document/BlueprintBluecodeProtocol.md", "不能退回到显示名猜测")
    require_text("Document/BlueprintBluecodeProtocol.md", "bluecode_read_function` 同时返回 `action_hints`")
    require_text("Document/BlueprintBluecodeProtocol.md", "action_hints_by_line")
    require_text("Document/BlueprintBluecodeProtocol.md", "include_connections=true")
    require_text("Document/BlueprintBluecodeProtocol.md", "`connections`")
    require_text("Document/BlueprintBluecodeProtocol.md", "`delete_target`")
    require_text("Document/BlueprintBluecodeProtocol.md", "`target_node_id`")
    require_text("Document/BlueprintBluecodeProtocol.md", "更新该既有节点")
    require_text("Document/BlueprintBluecodeProtocol.md", "Branch 节点")
    require_text("Document/BlueprintBluecodeProtocol.md", "Pin=A+B")
    require_text("Document/BlueprintBluecodeProtocol.md", "内层节点同样可以使用")
    require_text("Document/BlueprintBluecodeProtocol.md", "expression_hints")
    require_text("Document/BlueprintBluecodeProtocol.md", "bluecode_apply(action_hints=..., expression_hints=...)")
    require_text("Document/BlueprintBluecodeProtocol.md", "bluecode_connect")
    require_text("Document/BlueprintBluecodeProtocol.md", "IsValid:Return Value -> Branch:Condition")
    require_text("Document/BlueprintBluecodeProtocol.md", "\"NodeA:Then -> NodeB:Execute\"")
    require_text("Document/BlueprintBluecodeProtocol.md", "source_pin")
    require_text("Document/BlueprintBluecodeProtocol.md", "alias=Name")
    require_text("Document/BlueprintBluecodeProtocol.md", "aliases={Name: NodeId}")
    require_text("Document/BlueprintBluecodeProtocol.md", "operations[].result")
    require_text("Document/BlueprintBluecodeProtocol.md", "`inputs`、`outputs`")
    require_text("Document/BlueprintBluecodeProtocol.md", "`is_exec`")
    require_text("Document/BlueprintBluecodeProtocol.md", "`pin_counts`")
    require_text("Document/BlueprintBluecodeProtocol.md", "`node_properties`")
    require_text("Document/BlueprintBluecodeProtocol.md", "node_properties.failures")
    require_text("Document/BlueprintBluecodeProtocol.md", "要求 break/replace")
    require_text("Document/BlueprintBluecodeProtocol.md", "Score = A + B")
    require_text("Document/BlueprintBluecodeProtocol.md", "内联成表达式")
    require_text("Document/BlueprintBluecodeProtocol.md", "沿 exec flow 输出语句")
    require_text("Resources/Python/UmgMcpServer.py", "action_handles")
    require_text("Resources/Python/UmgMcpServer.py", "action_hints")
    require_text("Resources/Python/UmgMcpServer.py", "expression_hints")
    require_text("Resources/Python/UmgMcpServer.py", "action_hints_by_line")
    require_text("Resources/Python/UmgMcpServer.py", "include_connections")
    require_text("Resources/Python/UmgMcpServer.py", "_bluecode_connections_from_nodes")
    require_text("Resources/Python/UmgMcpServer.py", "\"subAction\": \"bluecode_apply\"")
    require_text("Resources/Python/UmgMcpServer.py", "return await conn.send_command(\"manage_blueprint_graph\", backend_payload)")
    require_text("Resources/Python/UmgMcpServer.py", "{\"subAction\": \"bluecode_read_function\"}")
    require_text("Resources/Python/UmgMcpServer.py", "\"raw_error\"")
    require_text("Resources/Python/UmgMcpServer.py", "@register_tool(\"bluecode_search_nodes\"")
    require_text("Resources/Python/UmgMcpServer.py", "@register_tool(\"bluecode_connect\"")
    require_text("Resources/Python/UmgMcpServer.py", "node_aliases")
    require_text("Resources/Python/UmgMcpServer.py", "node_properties")
    forbid_text("Resources/Python/UmgMcpServer.py", "existing = await _bluecode_read_payload(conn)")
    forbid_text("Resources/Python/UmgMcpServer.py", "used_line_hint_indices = set()")
    require_text("Resources/Python/UmgMcpSkills.py", "def bluecode_search_nodes")
    require_text("Resources/Python/UmgMcpSkills.py", "def bluecode_connect")
    require_text("Resources/Python/UmgMcpSkills.py", "{\"subAction\": \"bluecode_read_function\"}")
    require_text("Resources/Python/UmgMcpSkills.py", "action_hints_by_line")
    require_text("Resources/Python/UmgMcpSkills.py", "expression_hints")
    require_text("Resources/Python/UmgMcpSkills.py", "include_connections")
    require_text("Resources/Python/UmgMcpSkills.py", "_connections_from_bluecode_nodes")
    require_text("Resources/Python/UmgMcpSkills.py", "\"subAction\": \"bluecode_apply\"")
    require_text("Resources/Python/UmgMcpSkills.py", "return _send(\"manage_blueprint_graph\", payload)")
    require_text("Resources/Python/UmgMcpSkills.py", "\"raw_error\"")
    require_text("Resources/Python/UmgMcpSkills.py", "node_aliases")
    require_text("Resources/Python/UmgMcpSkills.py", "node_properties")
    forbid_text("Resources/Python/UmgMcpSkills.py", "used_line_hint_indices = set()")
    forbid_text("Resources/Python/UmgMcpSkills.py", "statements = _bluecode_statements(code)")
    require_text("Resources/Python/APITest/Blueprint_BlueCode_Parser_Check.py", "Blueprint BlueCode payload forwarding check passed.")
    require_text("Resources/Python/APITest/Blueprint_BlueCode_Parser_Check.py", "include_connections")
    require_text("Resources/Python/APITest/Blueprint_BlueCode_Parser_Check.py", "expression_hints")
    require_text("Resources/Python/APITest/Blueprint_BlueCode_Parser_Check.py", "settimerbyevent")
    require_text("Resources/Python/APITest/Blueprint_BlueCode_Parser_Check.py", "alias=TimerNode")
    require_text("Resources/Python/APITest/Blueprint_BlueCode_Parser_Check.py", "bpact:search-result-handle")
    require_text("Resources/Python/APITest/Blueprint_BlueCode_Parser_Check.py", "CustomClass.DoThing")
    require_text("Resources/Python/APITest/Blueprint_BlueCode_Parser_Check.py", "node_properties")
    require_text("Resources/Python/APITest/Blueprint_MCP_Schema_Check.py", "server.mcp.list_tools")
    require_text("Resources/Python/APITest/Blueprint_MCP_Schema_Check.py", "bluecode_search_nodes")
    require_text("Resources/Python/APITest/Blueprint_MCP_Schema_Check.py", "action_hints")
    require_text("Resources/Python/APITest/Blueprint_MCP_Schema_Check.py", "expression_hints")
    require_text("Resources/Python/APITest/Blueprint_MCP_Schema_Check.py", "include_connections")
    require_text("Resources/Python/APITest/Blueprint_MCP_Schema_Check.py", "node_aliases")
    require_text("Resources/Python/APITest/Blueprint_MCP_Schema_Check.py", "node_properties")
    require_text("Resources/Python/APITest/Blueprint_MCP_Stdio_Check.py", "tools/list")
    require_text("Resources/Python/APITest/Blueprint_MCP_Stdio_Check.py", "notifications/initialized")
    require_text("Resources/Python/APITest/Blueprint_MCP_Stdio_Check.py", "Blueprint MCP stdio check passed.")
    require_text("Resources/Python/APITest/Blueprint_MCP_Server_BlueCode_Payload_Check.py", "include_connections")
    require_text("Resources/Python/APITest/Blueprint_MCP_Server_BlueCode_Payload_Check.py", "expression_hints")
    require_text("Resources/Python/APITest/Blueprint_MCP_Server_BlueCode_Payload_Check.py", "ExistingNodeA")
    require_text("Resources/Python/APITest/Blueprint_MCP_Server_BlueCode_Payload_Check.py", "\"subAction\": \"bluecode_apply\"")
    require_text("Resources/Python/APITest/Blueprint_MCP_Server_BlueCode_Payload_Check.py", "bluecode_connect")
    require_text("Resources/Python/APITest/Blueprint_MCP_Server_BlueCode_Payload_Check.py", "bluecode_delete")
    require_text("Resources/Python/APITest/Blueprint_MCP_Server_BlueCode_Payload_Check.py", "confirm_delete")
    require_text("Resources/Python/APITest/Blueprint_MCP_Server_BlueCode_Payload_Check.py", "CustomClass.DoThing")
    require_text("Resources/Python/APITest/Blueprint_MCP_Server_BlueCode_Payload_Check.py", "bpact:duplicate-one")
    require_text("Resources/Python/APITest/Blueprint_MCP_Server_BlueCode_Payload_Check.py", "node_properties")
    require_text("README.md", "Blueprint API Status (Transitional)")
    require_text("Readme_zh.md", "蓝图 (Blueprint) API 实现状态（过渡期）")
    require_text("Skills/umg-mcp-authoring/SKILL.md", "BlueCode as the default Blueprint MCP direction")
    require_text("Skills/umg-mcp-authoring/SKILL.md", "node(\"Action Menu Name\"")
    require_text("Skills/umg-mcp-authoring/SKILL.md", "alias=Name")
    require_text("Skills/umg-mcp-authoring/SKILL.md", "NodeA:Then -> NodeB:Execute")
    require_text("Skills/umg-mcp-authoring/SKILL.md", "delete_target")
    require_text("Skills/umg-mcp-authoring/SKILL.md", "`node_id`")
    require_text("Skills/umg-mcp-authoring/SKILL.md", "operations[].result")
    require_text("Skills/umg-mcp-authoring/SKILL.md", "`is_exec`")
    require_text("Skills/umg-mcp-authoring/SKILL.md", "`pin_counts`")
    require_text("Skills/umg-mcp-authoring/SKILL.md", "variable-set and Branch")
    require_text("Skills/umg-mcp-authoring/SKILL.md", "Pin=A+B")
    require_text("Skills/umg-mcp-authoring/SKILL.md", "inner expression")
    require_text("Skills/umg-mcp-authoring/SKILL.md", "expression_hints")
    require_text("Skills/umg-mcp-authoring/SKILL.md", "signature/category/node_class/node_class_path")
    require_text("Skills/umg-mcp-authoring/SKILL.md", "operations[].result.node_properties.failures")
    require_text("Resources/ToolModes/Blueprint.json", "value(\\\"Action Menu Name\\\"")
    require_text("Resources/ToolModes/Blueprint.json", "读函数返回的 action_hints")
    require_text("Resources/ToolModes/Blueprint.json", "expression_hints")
    require_text("Resources/ToolModes/Blueprint.json", "include_connections")
    require_text("Resources/ToolModes/Blueprint.json", "node_id")
    require_text("Resources/ToolModes/Blueprint.json", "is_exec")
    require_text("Resources/ToolModes/Blueprint.json", "pin_counts")
    require_text("Resources/ToolModes/Blueprint.json", "operation result")
    require_text("Resources/ToolModes/Blueprint.json", "bluecode_connect")
    require_text("Resources/ToolModes/Blueprint.json", "alias=Name")
    require_text("Resources/ToolModes/Blueprint.json", "node_properties")
    require_text("Resources/prompts.json", "node(\\\"Action Menu Name\\\"")
    require_text("Resources/prompts.json", "\"name\": \"bluecode_connect\"")
    require_text("Resources/prompts.json", "round-trip reconstruction")
    require_text("Resources/prompts.json", "include_connections")
    require_text("Resources/prompts.json", "node_id")
    require_text("Resources/prompts.json", "is_exec")
    require_text("Resources/prompts.json", "pin_counts")
    require_text("Resources/prompts.json", "node_aliases/aliases")
    require_text("Resources/prompts.json", "inputs/outputs pin evidence")
    require_text("Resources/prompts.json", "action_hints_by_line")
    require_text("Resources/prompts.json", "expression_hints")
    require_text("Resources/prompts.json", "node_class/node_class_path/signature")
    require_text("Resources/prompts.json", "node_properties")
    require_text("Source/UmgMcp/Private/Blueprint/UmgBlueprintFunctionSubsystem.cpp", "FBlueprintEditorUtils::RemoveNode")
    require_text("Source/UmgMcp/Private/Blueprint/UmgBlueprintFunctionSubsystem.cpp", "FBlueprintEditorUtils::RemoveMemberVariable")
    require_text("Source/UmgMcp/Private/Blueprint/UmgBlueprintFunctionSubsystem.cpp", "TrySpawnBlueprintActionNode")
    require_text("Source/UmgMcp/Private/Blueprint/UmgBlueprintFunctionSubsystem.cpp", "ApplyBluecodeActionHints")
    require_text("Source/UmgMcp/Private/Blueprint/UmgBlueprintFunctionSubsystem.cpp", "TryGetArrayField(TEXT(\"actions\")")
    require_text("Source/UmgMcp/Private/Blueprint/UmgBlueprintFunctionSubsystem.cpp", "TryGetBluecodeMemberClassHint")
    require_text("Source/UmgMcp/Private/Blueprint/UmgBlueprintFunctionSubsystem.cpp", "TryGetObjectField(TEXT(\"member_classes\")")
    require_text("Source/UmgMcp/Private/Blueprint/UmgBlueprintFunctionSubsystem.cpp", "OutNodeName = MemberName")
    require_text("Source/UmgMcp/Private/Blueprint/UmgBlueprintFunctionSubsystem.cpp", "SetStringField(TEXT(\"action_handle\")")
    require_text("Source/UmgMcp/Private/Blueprint/UmgBlueprintFunctionSubsystem.cpp", "TryGetStringField(TEXT(\"handle\"), Handle)")
    require_text("Source/UmgMcp/Private/Blueprint/UmgBlueprintFunctionSubsystem.cpp", "TryGetStringField(TEXT(\"signature\"), Signature)")
    require_text("Source/UmgMcp/Private/Blueprint/UmgBlueprintFunctionSubsystem.cpp", "Payload->HasField(TEXT(\"signature\"))")
    require_text("Source/UmgMcp/Private/Blueprint/UmgBlueprintFunctionSubsystem.cpp", "NodeClassPath")
    require_text("Source/UmgMcp/Private/Blueprint/UmgBlueprintFunctionSubsystem.cpp", "!Signature.IsEmpty() || !Category.IsEmpty() || !NodeClass.IsEmpty() || !NodeClassPath.IsEmpty()")
    require_text("Source/UmgMcp/Private/Blueprint/UmgBlueprintFunctionSubsystem.cpp", "ActionSignature")
    require_text("Source/UmgMcp/Private/Blueprint/UmgBlueprintFunctionSubsystem.cpp", "NormalizeBluecodeStatement(ActionSignature)")
    require_text("Source/UmgMcp/Private/Blueprint/UmgBlueprintFunctionSubsystem.cpp", "MakeBluecodeRoundtripHint")
    require_text("Source/UmgMcp/Private/Blueprint/UmgBlueprintFunctionSubsystem.cpp", "MakeBluecodePinHintJson")
    require_text("Source/UmgMcp/Private/Blueprint/UmgBlueprintFunctionSubsystem.cpp", "AddBluecodeNodeResultFields")
    require_text("Source/UmgMcp/Private/Blueprint/UmgBlueprintFunctionSubsystem.cpp", "MakePinCounts")
    require_text("Source/UmgMcp/Private/Blueprint/UmgBlueprintFunctionSubsystem.cpp", "SetObjectField(TEXT(\"pin_counts\")")
    require_text("Source/UmgMcp/Private/Blueprint/UmgBlueprintFunctionSubsystem.cpp", "SetBoolField(TEXT(\"is_exec\")")
    require_text("Source/UmgMcp/Private/Blueprint/UmgBlueprintFunctionSubsystem.cpp", "SetStringField(TEXT(\"node_id\")")
    require_text("Source/UmgMcp/Private/Blueprint/UmgBlueprintFunctionSubsystem.cpp", "CollectBluecodeConnections")
    require_text("Source/UmgMcp/Private/Blueprint/UmgBlueprintFunctionSubsystem.cpp", "SetObjectField(TEXT(\"delete_target\")")
    require_text("Source/UmgMcp/Private/Blueprint/UmgBlueprintFunctionSubsystem.cpp", "ApplyPayloadToExistingNode")
    require_text("Source/UmgMcp/Private/Blueprint/UmgBlueprintFunctionSubsystem.cpp", "TryWireUpdateExpressionNode")
    require_text("Source/UmgMcp/Private/Blueprint/UmgBlueprintFunctionSubsystem.cpp", "CopyBluecodeHintContext")
    require_text("Source/UmgMcp/Private/Blueprint/UmgBlueprintFunctionSubsystem.cpp", "ApplyBluecodeActionHints(Expression, ExprCallName")
    require_text("Source/UmgMcp/Private/Blueprint/UmgBlueprintFunctionSubsystem.cpp", "ExpressionHints")
    require_text("Source/UmgMcp/Private/Blueprint/UmgBlueprintFunctionSubsystem.cpp", "SetObjectField(TEXT(\"expression_hints\")")
    require_text("Source/UmgMcp/Private/Blueprint/UmgBlueprintFunctionSubsystem.cpp", "TryGetObjectField(TEXT(\"expression_hints\")")
    require_text("Source/UmgMcp/Private/Blueprint/UmgBlueprintFunctionSubsystem.cpp", "expression value node")
    require_text("Source/UmgMcp/Private/Blueprint/UmgBlueprintFunctionSubsystem.cpp", "SetStringField(TEXT(\"target_node_id\")")
    require_text("Source/UmgMcp/Private/Blueprint/UmgBlueprintFunctionSubsystem.cpp", "ApplyBluecodeActionHints(Statement, AssignmentName")
    require_text("Source/UmgMcp/Private/Blueprint/UmgBlueprintFunctionSubsystem.cpp", "ApplyBluecodeActionHints(Statement, TEXT(\"if\")")
    require_text("Source/UmgMcp/Private/Blueprint/UmgBlueprintFunctionSubsystem.cpp", "updated_existing")
    require_text("Source/UmgMcp/Private/Blueprint/UmgBlueprintFunctionSubsystem.cpp", "SetObjectField(TEXT(\"action_hints\")")
    require_text("Source/UmgMcp/Private/Blueprint/UmgBlueprintFunctionSubsystem.cpp", "SetArrayField(TEXT(\"action_hints_by_line\")")
    require_text("Source/UmgMcp/Private/Blueprint/UmgBlueprintFunctionSubsystem.cpp", "action_hints_usage")
    require_text("Source/UmgMcp/Private/Blueprint/UmgBlueprintFunctionSubsystem.cpp", "ApplyBluecodeConnect")
    require_text("Source/UmgMcp/Private/Blueprint/UmgBlueprintFunctionSubsystem.cpp", "SubAction == TEXT(\"bluecode_connect\")")
    require_text("Source/UmgMcp/Private/Blueprint/UmgBlueprintFunctionSubsystem.cpp", "SubAction == TEXT(\"bluecode_delete\")")
    require_text("Source/UmgMcp/Private/Blueprint/UmgBlueprintFunctionSubsystem.cpp", "BreakSinglePinLink")
    require_text("Source/UmgMcp/Private/Blueprint/UmgBlueprintFunctionSubsystem.cpp", "Connection delete target is ambiguous")
    require_text("Source/UmgMcp/Private/Blueprint/UmgBlueprintFunctionSubsystem.cpp", "CONNECT_RESPONSE_MAKE_WITH_CONVERSION_NODE")
    require_text("Source/UmgMcp/Private/Blueprint/UmgBlueprintFunctionSubsystem.cpp", "replace or break existing links")
    require_text("Source/UmgMcp/Private/Blueprint/UmgBlueprintFunctionSubsystem.cpp", "bluecode_connect is union-only")
    require_text("Source/UmgMcp/Private/Blueprint/UmgBlueprintFunctionSubsystem.cpp", "ApplyBluecodeArgsToPayload")
    require_text("Source/UmgMcp/Private/Blueprint/UmgBlueprintFunctionSubsystem.cpp", "TrySplitBluecodeNamedArg")
    require_text("Source/UmgMcp/Private/Blueprint/UmgBlueprintFunctionSubsystem.cpp", "ExtractBluecodeAliasArg")
    require_text("Source/UmgMcp/Private/Blueprint/UmgBlueprintFunctionSubsystem.cpp", "ApplyBluecodeAliasHints")
    require_text("Source/UmgMcp/Private/Blueprint/UmgBlueprintFunctionSubsystem.cpp", "SetObjectField(TEXT(\"aliases\")")
    require_text("Source/UmgMcp/Private/Blueprint/UmgBlueprintFunctionSubsystem.cpp", "SetArrayField(TEXT(\"failures\")")
    require_text("Source/UmgMcp/Private/Blueprint/UmgBlueprintFunctionSubsystem.cpp", "SetBoolField(TEXT(\"useActionMenu\"), true)")
    require_text("Source/UmgMcp/Private/Blueprint/UmgBlueprintFunctionSubsystem.cpp", "QuoteBluecodeString")
    require_text("Source/UmgMcp/Private/Blueprint/UmgBlueprintFunctionSubsystem.cpp", "BluecodeExpressionFromNode")
    require_text("Source/UmgMcp/Private/Blueprint/UmgBlueprintFunctionSubsystem.cpp", "CollectBluecodeDependencyNodes")
    require_text("Source/UmgMcp/Private/Blueprint/UmgBlueprintFunctionSubsystem.cpp", "BluecodeMathOperatorFromFunctionName")
    require_text("Source/UmgMcp/Private/Blueprint/UmgBlueprintFunctionSubsystem.cpp", "BuildBluecodeReadOrder")
    require_text("Source/UmgMcp/Private/Blueprint/UmgBlueprintFunctionSubsystem.cpp", "VisitBluecodeExecFlowNode")
    require_text("Source/UmgMcp/Private/Blueprint/UmgBlueprintFunctionSubsystem.cpp", "value_node")
    require_text("Source/UmgMcp/Private/Blueprint/UmgBlueprintFunctionSubsystem.cpp", "SubAction == TEXT(\"bluecode_read_function\")")
    require_text("Source/UmgMcp/Private/Blueprint/UmgBlueprintFunctionSubsystem.cpp", "assignment_expression")
    require_text("Source/UmgMcp/Private/Blueprint/UmgBlueprintFunctionSubsystem.cpp", "TSet<FString> ExistingKeys")
    require_text("Source/UmgMcp/Private/Blueprint/UmgBlueprintFunctionSubsystem.cpp", "TrySetBluecodeObjectProperty")
    require_text("Source/UmgMcp/Private/Blueprint/UmgBlueprintFunctionSubsystem.cpp", "ApplyBluecodeNodeProperties")
    require_text("Source/UmgMcp/Private/Blueprint/UmgBlueprintFunctionSubsystem.cpp", "TryGetObjectField(TEXT(\"node_properties\")")
    require_text("Source/UmgMcp/Private/Blueprint/UmgBlueprintFunctionSubsystem.cpp", "SetObjectField(TEXT(\"node_properties\")")
    require_text("Source/UmgMcp/Private/FabServer/FabServerHttpClient.cpp", "ToolName == TEXT(\"bluecode_search_nodes\")")
    require_text("Source/UmgMcp/Private/FabServer/FabServerHttpClient.cpp", "ToolName == TEXT(\"bluecode_connect\")")
    require_text("Source/UmgMcp/Private/FabServer/FabServerHttpClient.cpp", "FinalArgs->SetStringField(TEXT(\"subAction\"), TEXT(\"bluecode_read_function\"))")
    require_text("Source/UmgMcp/Private/FabServer/Tools/UmgMcpToolSchemaBuilder.cpp", "Tools.Add(TEXT(\"bluecode_search_nodes\")")
    require_text("Source/UmgMcp/Private/FabServer/Tools/UmgMcpToolSchemaBuilder.cpp", "Tools.Add(TEXT(\"bluecode_connect\")")
    require_text("Source/UmgMcp/Private/FabServer/Tools/UmgMcpToolSchemaBuilder.cpp", "include_connections")
    require_text("Source/UmgMcp/Private/FabServer/Tools/UmgMcpToolSchemaBuilder.cpp", "AddOptionalParam(T, TEXT(\"context_sensitive\")")
    require_text("Source/UmgMcp/Private/FabServer/Tools/UmgMcpToolSchemaBuilder.cpp", "AddOptionalParam(T, TEXT(\"exact\")")
    require_text("Source/UmgMcp/Private/FabServer/Tools/UmgMcpToolSchemaBuilder.cpp", "AddOptionalParam(T, TEXT(\"anchor\")")
    require_text("Source/UmgMcp/Private/FabServer/Tools/UmgMcpToolSchemaBuilder.cpp", "AddOptionalParam(T, TEXT(\"member_classes\")")
    require_text("Source/UmgMcp/Private/FabServer/Tools/UmgMcpToolSchemaBuilder.cpp", "AddOptionalParam(T, TEXT(\"action_handles\")")
    require_text("Source/UmgMcp/Private/FabServer/Tools/UmgMcpToolSchemaBuilder.cpp", "AddOptionalParam(T, TEXT(\"action_hints_by_line\")")
    require_text("Source/UmgMcp/Private/FabServer/Tools/UmgMcpToolSchemaBuilder.cpp", "AddOptionalParam(T, TEXT(\"expression_hints\")")
    require_text("Source/UmgMcp/Private/FabServer/Tools/UmgMcpToolSchemaBuilder.cpp", "AddOptionalParam(T, TEXT(\"node_aliases\")")
    require_text("Source/UmgMcp/Private/FabServer/Tools/UmgMcpToolSchemaBuilder.cpp", "AddOptionalParam(T, TEXT(\"aliases\")")
    require_text("Source/UmgMcp/Private/FabServer/Tools/UmgMcpToolSchemaBuilder.cpp", "AddOptionalParam(T, TEXT(\"node_properties\")")
    require_text("Source/UmgMcp/Private/FabServer/Tools/UmgMcpToolSchemaBuilder.cpp", "AddOptionalParam(T, TEXT(\"connects\")")
    require_text("Source/UmgMcp/Private/FabServer/Tools/UmgMcpToolSchemaBuilder.cpp", "kind node/variable/connection")
    require_text("Source/UmgMcp/Private/FabServer/Tools/UmgMcpToolSchemaBuilder.cpp", "signature")

    get_nodes_description = tools["get_function_nodes"]["description"]
    if "Properties, and Connections" in get_nodes_description:
        raise AssertionError("get_function_nodes prompt overstates the current readback detail")

    print("Blueprint protocol static check passed.")


if __name__ == "__main__":
    main()
