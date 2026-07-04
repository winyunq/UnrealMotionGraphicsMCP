import asyncio
import json
import os
import time
from typing import Any, Dict

from mcp import ClientSession, StdioServerParameters
from mcp.client.stdio import stdio_client


PYTHON_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))


def ok(response: Dict[str, Any]) -> bool:
    return response.get("success") is True or response.get("status") == "success"


def require(condition: bool, message: str, payload: Any = None) -> None:
    if condition:
        return
    detail = f"\n{json.dumps(payload, indent=2, ensure_ascii=False)}" if payload is not None else ""
    raise AssertionError(f"{message}{detail}")


def parse_tool_result(result: Any) -> Dict[str, Any]:
    structured = getattr(result, "structuredContent", None)
    if isinstance(structured, dict):
        payload = structured.get("result", structured)
        if isinstance(payload, dict):
            return payload

    content = getattr(result, "content", None) or []
    if content:
        text = getattr(content[0], "text", "")
        if text:
            return json.loads(text)

    return {}


async def call_tool(session: ClientSession, name: str, arguments: Dict[str, Any], attempts: int = 4) -> Dict[str, Any]:
    last_response: Dict[str, Any] = {}
    for _ in range(attempts):
        result = await session.call_tool(name, arguments)
        last_response = parse_tool_result(result)
        if ok(last_response):
            return last_response

        error_text = str(last_response.get("error") or last_response.get("message") or "")
        if "Timeout" not in error_text and "busy" not in error_text:
            break
        await asyncio.sleep(2.0)

    require(ok(last_response), f"MCP tool call failed: {name}", last_response)
    return last_response


async def run_check() -> None:
    server = StdioServerParameters(
        command="uv",
        args=["run", "--directory", PYTHON_ROOT, "UmgMcpServer.py"],
    )

    async with stdio_client(server) as (read, write):
        async with ClientSession(read, write) as session:
            await session.initialize()

            tool_names = {tool.name for tool in (await session.list_tools()).tools}
            for required_tool in (
                "set_target_umg_asset",
                "create_widget",
                "set_edit_function",
                "add_step",
                "get_function_nodes",
                "compile_blueprint",
            ):
                require(required_tool in tool_names, f"MCP tool missing: {required_tool}", sorted(tool_names))

            asset_path = f"/Game/MCPBlueprintTests/WidgetReferenceInputWires_{int(time.time())}"

            await call_tool(session, "set_target_umg_asset", {"asset_path": asset_path}, attempts=6)
            await call_tool(session, "create_widget", {"widget_type": "CanvasPanel", "new_widget_name": "RootCanvas"}, attempts=6)
            await call_tool(session, "create_widget", {"widget_type": "WidgetSwitcher", "new_widget_name": "TestSwitcher", "parent_name": "RootCanvas"}, attempts=6)
            await call_tool(session, "create_widget", {"widget_type": "Button", "new_widget_name": "TestBtn", "parent_name": "RootCanvas"}, attempts=6)
            await call_tool(session, "compile_blueprint", {"blueprint_name": asset_path}, attempts=6)

            event = await call_tool(session, "set_edit_function", {"function_name": "TestBtn.OnClicked"}, attempts=6)
            require(event.get("cursor_node"), "Component event binding did not produce a cursor node", event)

            added = await call_tool(
                session,
                "add_step",
                {
                    "name": "SetActiveWidgetIndex",
                    "member_class": "/Script/UMG.WidgetSwitcher",
                    "input_wires": {"Target": "TestSwitcher"},
                    "args": [0],
                    "comment": "Issue 28 regression: widget tree reference through input_wires",
                },
                attempts=6,
            )

            nodes = await call_tool(session, "get_function_nodes", {}, attempts=6)
            await call_tool(session, "compile_blueprint", {"blueprint_name": asset_path}, attempts=6)

            print(json.dumps({
                "asset_path": asset_path,
                "add_step": added,
                "node_count": nodes.get("node_count") or len(nodes.get("nodes", [])),
            }, indent=2, ensure_ascii=False))


if __name__ == "__main__":
    asyncio.run(run_check())
