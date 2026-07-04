import asyncio
import json
import os
import sys
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


def changed_property_map(overview: Dict[str, Any]) -> Dict[str, Dict[str, Any]]:
    result = {}
    for item in overview.get("changed_properties", []):
        prop = item.get("property")
        if prop:
            result[prop] = item
    return result


def values_at_time(time_response: Dict[str, Any], target_time: float) -> Dict[str, Any]:
    for item in time_response.get("slices", []):
        if abs(float(item.get("time", -1.0)) - target_time) < 0.0001:
            return {value.get("property"): value.get("value") for value in item.get("values", [])}
    return {}


async def run_check() -> None:
    server = StdioServerParameters(
        command="uv",
        args=["run", "--directory", PYTHON_ROOT, "UmgMcpServer.py"],
    )

    async with stdio_client(server) as (read, write):
        async with ClientSession(read, write) as session:
            await session.initialize()

            listed = await session.list_tools()
            tool_names = {tool.name for tool in listed.tools}
            for required_tool in (
                "set_target_umg_asset",
                "create_widget",
                "set_animation_scope",
                "set_target_widget",
                "set_property_keys",
                "animation_overview",
                "animation_time_properties",
                "animation_delete_widget_keys",
            ):
                require(required_tool in tool_names, f"MCP tool missing: {required_tool}", sorted(tool_names))

            asset_path = f"/Game/MCPSequencerTests/RenderTransformMcpCheck_{int(time.time())}"
            animation_name = "RT_MCP_Check"
            widget_name = "AnimBox"

            await call_tool(session, "set_target_umg_asset", {"asset_path": asset_path}, attempts=6)
            await call_tool(session, "create_widget", {"widget_type": "CanvasPanel", "new_widget_name": "RootCanvas", "parent_name": ""}, attempts=6)
            await call_tool(session, "create_widget", {"widget_type": "Border", "new_widget_name": widget_name, "parent_name": "RootCanvas"}, attempts=6)
            await call_tool(
                session,
                "set_widget_properties",
                {
                    "widget_name": widget_name,
                    "properties": {
                        "RenderOpacity": 1.0,
                        "RenderTransform.Pivot": {"X": 0.5, "Y": 0.5},
                        "Slot.CanvasSlot.Position": {"X": 640.0, "Y": 360.0},
                        "Slot.CanvasSlot.SizeX": 240.0,
                        "Slot.CanvasSlot.SizeY": 120.0,
                    },
                },
                attempts=6,
            )

            await call_tool(session, "create_animation", {"animation_name": animation_name}, attempts=6)
            await call_tool(session, "set_animation_scope", {"animation_name": animation_name}, attempts=6)
            await call_tool(session, "set_target_widget", {"widget_name": widget_name}, attempts=6)

            opacity_keys = [
                {"time": 0.0, "value": 0.0},
                {"time": 0.5, "value": 1.0},
                {"time": 1.0, "value": 1.0},
            ]
            scale_keys = [
                {"time": 0.0, "value": 1.0},
                {"time": 0.5, "value": 1.4},
                {"time": 1.0, "value": 1.0},
            ]

            opacity = await call_tool(session, "set_property_keys", {"property_name": "RenderOpacity", "keys": opacity_keys}, attempts=6)
            scale_x = await call_tool(session, "set_property_keys", {"property_name": "RenderTransform.Scale.X", "keys": scale_keys}, attempts=6)
            scale_y = await call_tool(session, "set_property_keys", {"property_name": "RenderTransform.Scale.Y", "keys": scale_keys}, attempts=6)

            require(opacity.get("track_type") == "float", "RenderOpacity should use a float track", opacity)
            require(scale_x.get("track_type") == "2d_transform", "Scale.X should use UMG 2D transform track", scale_x)
            require(scale_y.get("track_type") == "2d_transform", "Scale.Y should use UMG 2D transform track", scale_y)

            overview = await call_tool(
                session,
                "animation_overview",
                {"animation_name": animation_name, "widget_name": widget_name},
                attempts=6,
            )
            changed = changed_property_map(overview)
            for prop in ("RenderOpacity", "RenderTransform.Scale.X", "RenderTransform.Scale.Y"):
                require(prop in changed, f"{prop} missing from animation overview", overview)
            require(changed["RenderTransform.Scale.X"].get("track_type") == "2d_transform", "Scale.X overview track type mismatch", overview)
            require(changed["RenderTransform.Scale.Y"].get("track_type") == "2d_transform", "Scale.Y overview track type mismatch", overview)

            keyframes = await call_tool(session, "get_animation_keyframes", {"animation_name": animation_name}, attempts=6)
            keyframe_props = {track.get("property_name") for track in keyframes.get("tracks", [])}
            require("RenderTransform.Scale.X" in keyframe_props, "Scale.X missing from keyframe read", keyframes)
            require("RenderTransform.Scale.Y" in keyframe_props, "Scale.Y missing from keyframe read", keyframes)

            time_slice = await call_tool(
                session,
                "animation_time_properties",
                {"times": [0.5], "animation_name": animation_name, "widget_name": widget_name},
                attempts=6,
            )
            values = values_at_time(time_slice, 0.5)
            require(abs(float(values.get("RenderTransform.Scale.X", 0.0)) - 1.4) < 0.001, "Scale.X did not evaluate at t=0.5", time_slice)
            require(abs(float(values.get("RenderTransform.Scale.Y", 0.0)) - 1.4) < 0.001, "Scale.Y did not evaluate at t=0.5", time_slice)

            delete = await call_tool(
                session,
                "animation_delete_widget_keys",
                {
                    "property_name": "RenderTransform.Scale.X",
                    "times": [0.5],
                    "widget_name": widget_name,
                    "animation_name": animation_name,
                    "confirm_delete": True,
                },
                attempts=6,
            )
            require(delete.get("removed_keys", 0) >= 1, "Scale.X delete did not remove a key", delete)

            print(json.dumps({
                "asset_path": asset_path,
                "animation": animation_name,
                "mcp_tool_count": len(tool_names),
                "overview_track_count": overview.get("track_count"),
                "time_slice_values": values,
                "delete_result": delete,
            }, indent=2, ensure_ascii=False))


if __name__ == "__main__":
    if not os.path.exists(os.path.join(PYTHON_ROOT, "UmgMcpServer.py")):
        raise FileNotFoundError(os.path.join(PYTHON_ROOT, "UmgMcpServer.py"))
    asyncio.run(run_check())
