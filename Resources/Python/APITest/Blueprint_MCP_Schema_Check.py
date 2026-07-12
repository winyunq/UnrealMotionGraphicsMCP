import asyncio
import importlib.util
import logging
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SERVER_PATH = ROOT / "UmgMcpServer.py"


def load_server_module():
    logging.disable(logging.CRITICAL)
    sys.path.insert(0, str(ROOT))
    spec = importlib.util.spec_from_file_location("UmgMcpServer_schema_check", SERVER_PATH)
    if spec is None or spec.loader is None:
        raise AssertionError(f"Unable to load {SERVER_PATH}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


async def main() -> None:
    server = load_server_module()
    tools = {tool.name: tool for tool in await server.mcp.list_tools()}

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
    }
    missing = sorted(required_tools - set(tools))
    if missing:
        raise AssertionError(f"Missing BlueCode MCP tools: {missing}")

    apply_schema = tools["bluecode_apply"].inputSchema
    apply_props = apply_schema.get("properties", {})
    for name in ("code", "anchor", "mode", "member_classes", "action_handles", "action_hints", "expression_hints", "action_hints_by_line", "node_aliases", "aliases", "node_properties"):
        if name not in apply_props:
            raise AssertionError(f"bluecode_apply missing MCP input: {name}")

    description = tools["bluecode_apply"].description or ""
    for text in ("node(\"Action Menu Name\"", "value(\"Action Menu Name\"", "action_handles", "action_hints", "expression_hints", "node_properties", "signature", "alias"):
        if text not in description:
            raise AssertionError(f"bluecode_apply description missing: {text}")

    read_description = tools["bluecode_read_function"].description or ""
    read_props = tools["bluecode_read_function"].inputSchema.get("properties", {})
    for name in ("detail", "include_connections"):
        if name not in read_props:
            raise AssertionError(f"bluecode_read_function missing MCP input: {name}")
    for text in ("action_hints", "round-trip", "include_connections", "edge"):
        if text not in read_description:
            raise AssertionError(f"bluecode_read_function description missing: {text}")

    search_schema = tools["bluecode_search_nodes"].inputSchema
    search_props = search_schema.get("properties", {})
    for name in ("query", "category", "node_class", "node_class_path", "max_count", "include_pins", "context_sensitive", "exact"):
        if name not in search_props:
            raise AssertionError(f"bluecode_search_nodes missing MCP input: {name}")

    if search_props["context_sensitive"].get("default") is not True:
        raise AssertionError("bluecode_search_nodes context_sensitive should default true")
    if search_props["max_count"].get("default") != 50:
        raise AssertionError("bluecode_search_nodes max_count should default 50")
    search_description = tools["bluecode_search_nodes"].description or ""
    for text in ("is_exec", "pin_counts", "include_pins"):
        if text not in search_description:
            raise AssertionError(f"bluecode_search_nodes description missing: {text}")

    connect_schema = tools["bluecode_connect"].inputSchema
    connect_props = connect_schema.get("properties", {})
    for name in ("connects", "source", "target", "aliases"):
        if name not in connect_props:
            raise AssertionError(f"bluecode_connect missing MCP input: {name}")
    connect_description = tools["bluecode_connect"].description or ""
    for text in ("escape hatch", "Union-only", "aliases"):
        if text not in connect_description:
            raise AssertionError(f"bluecode_connect description missing: {text}")

    delete_schema = tools["bluecode_delete"].inputSchema
    delete_props = delete_schema.get("properties", {})
    for name in ("targets", "confirm_delete", "aliases"):
        if name not in delete_props:
            raise AssertionError(f"bluecode_delete missing MCP input: {name}")
    delete_description = tools["bluecode_delete"].description or ""
    for text in ("nodes", "variables", "connections", "confirm_delete"):
        if text not in delete_description:
            raise AssertionError(f"bluecode_delete description missing: {text}")

    print("Blueprint MCP schema check passed.")


if __name__ == "__main__":
    asyncio.run(main())
