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
                "set_widget_scope",
                "animation_append_widget_tracks",
                "animation_append_time_slice",
                "animation_overview",
                "animation_widget_properties",
                "animation_time_properties",
                "get_all_animations",
                "animation_delete_widget_keys",
                "get_animation_keyframes",
                "get_animated_widgets",
                "get_animation_full_data",
                "get_widget_animation_data",
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
            await call_tool(session, "set_widget_scope", {"widget_name": widget_name}, attempts=6)

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

            append_tracks = await call_tool(
                session,
                "animation_append_widget_tracks",
                {
                    "widget_name": widget_name,
                    "animation_name": animation_name,
                    "tracks": [
                        {"property": "RenderOpacity", "keys": opacity_keys},
                        {"property": "RenderTransform.Scale.X", "keys": scale_keys},
                        {"property": "RenderTransform.Scale.Y", "keys": scale_keys},
                        {
                            "property": "RenderTransform.Translation",
                            "keys": [
                                {"time": 0.0, "value": {"x": 0.0, "y": 16.0}},
                                {"time": 0.5, "value": {"x": 0.0, "y": 0.0}},
                            ],
                        },
                    ],
                },
                attempts=6,
            )
            require(append_tracks.get("track_count") == 4, "append_widget_tracks should apply four tracks", append_tracks)
            track_types = {track.get("property"): track.get("track_type") for track in append_tracks.get("tracks", [])}
            require(track_types.get("RenderOpacity") == "float", "RenderOpacity should use a float track", append_tracks)
            require(track_types.get("RenderTransform.Scale.X") == "2d_transform", "Scale.X should use UMG 2D transform track", append_tracks)
            require(track_types.get("RenderTransform.Scale.Y") == "2d_transform", "Scale.Y should use UMG 2D transform track", append_tracks)
            require(track_types.get("RenderTransform.Translation") == "2d_transform", "Translation should use UMG 2D transform track", append_tracks)

            time_append = await call_tool(
                session,
                "animation_append_time_slice",
                {
                    "time": 0.75,
                    "animation_name": animation_name,
                    "widgets": [
                        {
                            "widget_name": widget_name,
                            "properties": {
                                "RenderOpacity": 0.85,
                                "RenderTransform.Angle": 6.0,
                            },
                        }
                    ],
                },
                attempts=6,
            )
            require(time_append.get("keys_total") == 2, "append_time_slice should apply two keys", time_append)

            animations = await call_tool(session, "get_all_animations", {}, attempts=6)
            animation_names = {item.get("name") for item in animations.get("animations", [])}
            require(animation_name in animation_names, "created animation missing from get_all_animations", animations)

            widgets = await call_tool(session, "get_animated_widgets", {"animation_name": animation_name}, attempts=6)
            animated_widget_names = {item.get("widget_name") for item in widgets.get("widgets", [])}
            require(widget_name in animated_widget_names, "animated widget missing from get_animated_widgets", widgets)

            overview = await call_tool(
                session,
                "animation_overview",
                {"animation_name": animation_name, "widget_name": widget_name},
                attempts=6,
            )
            changed = changed_property_map(overview)
            for prop in ("RenderOpacity", "RenderTransform.Scale.X", "RenderTransform.Scale.Y", "RenderTransform.Angle"):
                require(prop in changed, f"{prop} missing from animation overview", overview)
            require(changed["RenderTransform.Scale.X"].get("track_type") == "2d_transform", "Scale.X overview track type mismatch", overview)
            require(changed["RenderTransform.Scale.Y"].get("track_type") == "2d_transform", "Scale.Y overview track type mismatch", overview)

            keyframes = await call_tool(session, "get_animation_keyframes", {"animation_name": animation_name}, attempts=6)
            keyframe_props = {track.get("property_name") for track in keyframes.get("tracks", [])}
            require("RenderTransform.Scale.X" in keyframe_props, "Scale.X missing from keyframe read", keyframes)
            require("RenderTransform.Scale.Y" in keyframe_props, "Scale.Y missing from keyframe read", keyframes)
            require("RenderTransform.Translation.X" in keyframe_props, "Translation.X missing from keyframe read", keyframes)
            require("RenderTransform.Angle" in keyframe_props, "Angle missing from keyframe read", keyframes)

            full_data = await call_tool(session, "get_animation_full_data", {"animation_name": animation_name}, attempts=6)
            full_data_props = {track.get("property_name") for track in full_data.get("tracks", [])}
            require("RenderOpacity" in full_data_props, "RenderOpacity missing from full animation read", full_data)

            widget_timeline = await call_tool(
                session,
                "animation_widget_properties",
                {"animation_name": animation_name, "widget_name": widget_name},
                attempts=6,
            )
            require(widget_timeline.get("change_count", 0) >= 1, "widget timeline read returned no changes", widget_timeline)

            widget_data = await call_tool(
                session,
                "get_widget_animation_data",
                {"animation_name": animation_name, "widget_name": widget_name},
                attempts=6,
            )
            require(widget_data.get("change_count", 0) >= 1, "focused widget animation data returned no changes", widget_data)

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
                "widget_timeline_changes": widget_timeline.get("change_count"),
                "time_slice_values": values,
                "delete_result": delete,
            }, indent=2, ensure_ascii=False))


if __name__ == "__main__":
    if not os.path.exists(os.path.join(PYTHON_ROOT, "UmgMcpServer.py")):
        raise FileNotFoundError(os.path.join(PYTHON_ROOT, "UmgMcpServer.py"))
    asyncio.run(run_check())
