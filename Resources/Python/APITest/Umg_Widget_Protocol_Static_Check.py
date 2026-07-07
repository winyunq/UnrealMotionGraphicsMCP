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
    umg_mode = json.loads((ROOT / "Resources/ToolModes/UMG.json").read_text(encoding="utf-8"))
    allowed_tools = set(umg_mode.get("allowed_tools", []))

    missing_prompt_entries = [name for name in registered_tools() if name not in tools]
    if missing_prompt_entries:
        raise AssertionError(f"Registered tools missing prompts entries: {missing_prompt_entries}")

    required_tools = {
        "get_widget_schema",
        "get_creatable_widget_types",
        "get_target_umg_asset",
        "set_target_umg_asset",
        "get_target_widget",
        "set_target_widget",
        "get_last_edited_umg_asset",
        "get_recently_edited_umg_assets",
        "get_widget_tree",
        "query_widget_properties",
        "get_layout_data",
        "create_widget",
        "set_widget_properties",
        "delete_widget",
        "reorder_widget_tree",
        "reparent_widget",
        "apply_layout",
        "save_asset",
        "list_assets",
    }
    hidden_tools = {
        "check_widget_overlap",
        "export_umg_to_json",
        "apply_json_to_umg",
        "apply_html_to_umg",
    }

    missing_enabled = required_tools - enabled_tool_names
    missing_mode = required_tools - allowed_tools
    if missing_enabled:
        raise AssertionError(f"Missing enabled UMG prompt tools: {sorted(missing_enabled)}")
    if missing_mode:
        raise AssertionError(f"Missing UMG mode tools: {sorted(missing_mode)}")

    unknown_mode_tools = allowed_tools - set(tools)
    if unknown_mode_tools:
        raise AssertionError(f"UMG mode references tools not present in prompts: {sorted(unknown_mode_tools)}")

    for name in sorted(hidden_tools):
        tool = tools.get(name)
        if tool is None:
            raise AssertionError(f"Hidden UMG compatibility tool missing prompts entry: {name}")
        if tool.get("enabled", True):
            raise AssertionError(f"UMG compatibility tool should be disabled in prompts: {name}")
        if name in allowed_tools:
            raise AssertionError(f"UMG compatibility tool should not be in UMG mode: {name}")

    delete_description = tools["delete_widget"]["description"]
    if "confirm_delete=true" not in delete_description:
        raise AssertionError("delete_widget prompt must require confirm_delete=true")

    require_text("Resources/Python/UmgMcpServer.py", "async def delete_widget(widget_name: str, confirm_delete: bool = False)")
    require_text("Resources/Python/UmgMcpServer.py", "async def reorder_widget_tree")
    require_text("Resources/Python/Widget/UMGSet.py", '"confirm_delete": confirm_delete')
    require_text("Resources/Python/Widget/UMGSet.py", "reorder_widget_tree")
    require_text("Source/UmgMcp/Private/Widget/UmgMcpWidgetCommands.cpp", "Deletion hardened (Issue 15)")
    require_text("Source/UmgMcp/Private/Widget/UmgMcpWidgetCommands.cpp", "Command == TEXT(\"reorder_widget_tree\")")
    require_text("Source/UmgMcp/Private/Widget/UmgMcpWidgetCommands.cpp", "Command == TEXT(\"get_layout_data\")")
    require_text("Source/UmgMcp/Private/Widget/UmgSetSubsystem.cpp", "Removed stale source widget GUID")
    require_text("Source/UmgMcp/Private/Widget/UmgSetSubsystem.cpp", "WidgetVariableNameToGuidMap.Remove")
    require_text("Document/UmgWidgetMcpProtocol.md", "confirm_delete=true")
    require_text("Document/UmgWidgetMcpProtocol.md", "reorder_widget_tree")
    require_text("Document/UmgWidgetMcpProtocol.md", "set_widget_properties")
    require_text("README.md", "`delete_widget`")
    require_text("Readme_zh.md", "`delete_widget`")

    print("UMG widget protocol static check passed.")


if __name__ == "__main__":
    main()
