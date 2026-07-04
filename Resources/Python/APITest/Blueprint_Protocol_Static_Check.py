import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]


def require_text(path: str, needle: str) -> None:
    text = (ROOT / path).read_text(encoding="utf-8")
    if needle not in text:
        raise AssertionError(f"{needle!r} not found in {path}")


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
        "set_edit_function",
        "add_step",
        "prepare_value",
        "connect_data_to_pin",
        "add_variable",
        "get_variables",
        "get_function_nodes",
        "set_cursor_node",
        "search_function_library",
        "compile_blueprint",
    }
    support_tools = {
        "set_target_umg_asset",
        "get_target_umg_asset",
        "set_target_widget",
        "get_target_widget",
        "save_asset",
    }
    hidden_deletes = {
        "delete_node",
        "delete_variable",
    }

    missing_prompts = required_tools - enabled_tool_names
    missing_mode = (required_tools | support_tools) - allowed_tools
    if missing_prompts:
        raise AssertionError(f"Missing enabled Blueprint prompt tools: {sorted(missing_prompts)}")
    if missing_mode:
        raise AssertionError(f"Missing Blueprint mode tools: {sorted(missing_mode)}")

    for name in sorted(hidden_deletes):
        tool = tools.get(name)
        if tool is None:
            raise AssertionError(f"Hidden delete tool missing prompts entry: {name}")
        if tool.get("enabled", True):
            raise AssertionError(f"Blueprint delete tool should be disabled in prompts: {name}")
        if name in allowed_tools:
            raise AssertionError(f"Blueprint delete tool should not be in Blueprint mode: {name}")

    unknown_mode_tools = allowed_tools - set(tools)
    if unknown_mode_tools:
        raise AssertionError(f"Blueprint mode references tools not present in prompts: {sorted(unknown_mode_tools)}")

    require_text("Document/BlueprintBluecodeProtocol.md", "bluecode_apply")
    require_text("Document/BlueprintBluecodeProtocol.md", "confirm_delete=true")
    require_text("README.md", "Blueprint API Status (Transitional)")
    require_text("Readme_zh.md", "蓝图 (Blueprint) API 实现状态（过渡期）")
    require_text("Skills/umg-mcp-authoring/SKILL.md", "current Blueprint MCP as transitional")
    require_text("Source/UmgMcp/Private/Blueprint/UmgBlueprintFunctionSubsystem.cpp", "FBlueprintEditorUtils::RemoveNode")
    require_text("Source/UmgMcp/Private/Blueprint/UmgBlueprintFunctionSubsystem.cpp", "FBlueprintEditorUtils::RemoveMemberVariable")

    get_nodes_description = tools["get_function_nodes"]["description"]
    if "Properties, and Connections" in get_nodes_description:
        raise AssertionError("get_function_nodes prompt overstates the current readback detail")

    print("Blueprint protocol static check passed.")


if __name__ == "__main__":
    main()
