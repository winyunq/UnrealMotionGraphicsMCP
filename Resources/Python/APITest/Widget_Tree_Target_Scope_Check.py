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


async def call_tool_raw(session: ClientSession, name: str, arguments: Dict[str, Any]) -> Dict[str, Any]:
    result = await session.call_tool(name, arguments)
    return parse_tool_result(result)


async def expect_tool_failure(session: ClientSession, name: str, arguments: Dict[str, Any], message_part: str) -> Dict[str, Any]:
    response = await call_tool_raw(session, name, arguments)
    require(not ok(response), f"MCP tool call unexpectedly succeeded: {name}", response)
    error_text = str(response.get("error") or response.get("message") or "")
    require(message_part in error_text, f"MCP tool failure did not mention {message_part!r}: {name}", response)
    return response


async def run_check() -> None:
    server = StdioServerParameters(
        command="uv",
        args=["run", "--directory", PYTHON_ROOT, "UmgMcpServer.py"],
    )

    async with stdio_client(server) as (read, write):
        async with ClientSession(read, write) as session:
            await session.initialize()

            tool_names = {tool.name for tool in (await session.list_tools()).tools}
            for required_tool in ("set_target_umg_asset", "set_target_widget", "create_widget", "get_widget_tree", "delete_widget"):
                require(required_tool in tool_names, f"MCP tool missing: {required_tool}", sorted(tool_names))

            asset_path = f"/Game/MCPWidgetTests/TreeScope_{int(time.time())}"
            await call_tool(session, "set_target_umg_asset", {"asset_path": asset_path}, attempts=6)
            await call_tool(session, "create_widget", {"widget_type": "CanvasPanel", "new_widget_name": "RootCanvas"}, attempts=6)
            await call_tool(session, "create_widget", {"widget_type": "VerticalBox", "new_widget_name": "PanelA", "parent_name": "RootCanvas"}, attempts=6)
            await call_tool(session, "create_widget", {"widget_type": "Button", "new_widget_name": "ButtonA", "parent_name": "PanelA"}, attempts=6)
            await call_tool(session, "create_widget", {"widget_type": "VerticalBox", "new_widget_name": "PanelB", "parent_name": "RootCanvas"}, attempts=6)
            await call_tool(session, "create_widget", {"widget_type": "Button", "new_widget_name": "ButtonB", "parent_name": "PanelB"}, attempts=6)

            await call_tool(session, "set_target_widget", {"widget_name": "RootCanvas"}, attempts=6)
            root_tree = await call_tool(session, "get_widget_tree", {}, attempts=6)
            root_text = root_tree.get("widget_tree", "")
            require(root_tree.get("scope") == "target_widget", "RootCanvas read should report target_widget scope", root_tree)
            require("PanelA" in root_text and "PanelB" in root_text, "Root scoped tree should include both child panels", root_tree)

            await call_tool(session, "set_target_widget", {"widget_name": "PanelA"}, attempts=6)
            scoped_tree = await call_tool(session, "get_widget_tree", {}, attempts=6)
            scoped_text = scoped_tree.get("widget_tree", "")
            require(scoped_tree.get("root_widget") == "PanelA", "Scoped read should identify PanelA root", scoped_tree)
            require("PanelA" in scoped_text and "ButtonA" in scoped_text, "PanelA scoped tree should include PanelA branch", scoped_tree)
            require("PanelB" not in scoped_text and "ButtonB" not in scoped_text, "PanelA scoped tree should not include sibling branch", scoped_tree)

            delete_blocked = await expect_tool_failure(
                session,
                "delete_widget",
                {"widget_name": "ButtonB", "confirm_delete": False},
                "confirm_delete",
            )
            delete_result = await call_tool(
                session,
                "delete_widget",
                {"widget_name": "ButtonB", "confirm_delete": True},
                attempts=6,
            )
            require(delete_result.get("deleted_widget") == "ButtonB", "delete_widget did not delete the requested widget", delete_result)

            await call_tool(session, "set_target_widget", {"widget_name": "RootCanvas"}, attempts=6)
            after_delete_tree = await call_tool(session, "get_widget_tree", {}, attempts=6)
            after_delete_text = after_delete_tree.get("widget_tree", "")
            require("PanelB" in after_delete_text and "ButtonB" not in after_delete_text, "ButtonB should be removed while PanelB remains", after_delete_tree)

            print(json.dumps({
                "asset_path": asset_path,
                "root_scope": root_tree.get("scope"),
                "scoped_root": scoped_tree.get("root_widget"),
                "scoped_tree": scoped_text,
                "delete_blocked": delete_blocked,
                "delete_result": delete_result,
            }, indent=2, ensure_ascii=False))


if __name__ == "__main__":
    asyncio.run(run_check())
