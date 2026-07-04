import argparse
import asyncio
import json
import os
import time
from pathlib import Path
from typing import Any, Dict, Iterable, List, Optional

from mcp import ClientSession, StdioServerParameters
from mcp.client.stdio import stdio_client


PYTHON_ROOT = Path(__file__).resolve().parents[1]
ARTIFACT_ROOT = Path(__file__).resolve().parent / "Artifacts"


def ok(response: Dict[str, Any]) -> bool:
    if response.get("success") is True:
        return True
    if response.get("status") == "success":
        return True
    if response.get("compiled") is True:
        return True
    return False


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


def compact_payload(value: Any, limit: int = 5000) -> Any:
    try:
        raw = json.dumps(value, ensure_ascii=False, default=str)
    except TypeError:
        raw = str(value)
    if len(raw) <= limit:
        return value
    if isinstance(value, dict):
        return {
            "_truncated": True,
            "keys": sorted(str(key) for key in value.keys()),
            "preview": raw[:limit],
        }
    return {"_truncated": True, "preview": raw[:limit]}


class TraceRecorder:
    def __init__(self, trace_path: Path):
        self.trace_path = trace_path
        self.trace_path.parent.mkdir(parents=True, exist_ok=True)
        self._handle = self.trace_path.open("w", encoding="utf-8")

    def close(self) -> None:
        self._handle.close()

    def write(self, item: Dict[str, Any]) -> None:
        self._handle.write(json.dumps(item, ensure_ascii=False, default=str) + "\n")
        self._handle.flush()


async def call_tool(
    session: ClientSession,
    trace: TraceRecorder,
    system: str,
    name: str,
    arguments: Optional[Dict[str, Any]] = None,
    attempts: int = 4,
    required: bool = True,
) -> Dict[str, Any]:
    arguments = arguments or {}
    last_response: Dict[str, Any] = {}

    for attempt in range(1, attempts + 1):
        started = time.perf_counter()
        result = await session.call_tool(name, arguments)
        elapsed_ms = round((time.perf_counter() - started) * 1000.0, 2)
        last_response = parse_tool_result(result)

        trace.write(
            {
                "system": system,
                "tool": name,
                "attempt": attempt,
                "elapsed_ms": elapsed_ms,
                "arguments": arguments,
                "response": compact_payload(last_response),
            }
        )

        if ok(last_response):
            return last_response

        error_text = str(last_response.get("error") or last_response.get("message") or "")
        if "Timeout" not in error_text and "busy" not in error_text:
            break
        await asyncio.sleep(2.0)

    if required:
        require(ok(last_response), f"MCP tool call failed: {name}", last_response)
    return last_response


async def create_widget(
    session: ClientSession,
    trace: TraceRecorder,
    widget_type: str,
    name: str,
    parent: str = "",
) -> Dict[str, Any]:
    payload = {"widget_type": widget_type, "new_widget_name": name}
    if parent:
        payload["parent_name"] = parent
    return await call_tool(session, trace, "widget", "create_widget", payload, attempts=6)


async def set_props(
    session: ClientSession,
    trace: TraceRecorder,
    widget_name: str,
    properties: Dict[str, Any],
) -> Dict[str, Any]:
    return await call_tool(
        session,
        trace,
        "widget",
        "set_widget_properties",
        {"widget_name": widget_name, "properties": properties},
        attempts=6,
    )


def color(r: float, g: float, b: float, a: float = 1.0) -> Dict[str, float]:
    return {"R": r, "G": g, "B": b, "A": a}


async def build_material(session: ClientSession, trace: TraceRecorder, material_path: str) -> None:
    hlsl = "\n".join(
        [
            "float2 p = UV - 0.5;",
            "float radial = length(p);",
            "float gridX = smoothstep(0.985, 1.0, frac(UV.x * 42.0));",
            "float gridY = smoothstep(0.985, 1.0, frac(UV.y * 24.0));",
            "float ring = 1.0 - smoothstep(0.004, 0.018, abs(radial - 0.32));",
            "float sweep = smoothstep(-0.02, 0.02, p.x + p.y * 0.35);",
            "float vignette = smoothstep(0.85, 0.18, radial);",
            "float pulse = 0.82 + 0.18 * saturate(Pulse);",
            "float3 base = float3(0.012, 0.018, 0.045);",
            "float3 cyan = float3(0.0, 0.62, 1.0);",
            "float3 amber = float3(1.0, 0.68, 0.22);",
            "float3 col = base + (gridX + gridY) * cyan * 0.09;",
            "col += ring * cyan * 0.85 * pulse;",
            "col += sweep * amber * 0.09;",
            "col *= vignette;",
            "return float4(col, 1.0);",
        ]
    )
    await call_tool(
        session,
        trace,
        "material",
        "hlsl_set_target",
        {"path": material_path, "create_if_not_found": True},
        attempts=6,
    )
    await call_tool(
        session,
        trace,
        "material",
        "hlsl_set",
        {"hlsl": hlsl, "parameters": [{"name": "Pulse", "kind": "Scalar"}]},
        attempts=6,
    )
    await call_tool(session, trace, "material", "hlsl_compile", {}, attempts=6)


async def build_widgets(session: ClientSession, trace: TraceRecorder, material_path: str) -> None:
    await create_widget(session, trace, "CanvasPanel", "RootCanvas")
    await call_tool(session, trace, "target", "set_target_widget", {"widget_name": "RootCanvas"}, attempts=6)

    await create_widget(session, trace, "Image", "BackgroundMaterial", "RootCanvas")
    await set_props(
        session,
        trace,
        "BackgroundMaterial",
        {
            "Brush": {"ResourceObject": material_path, "DrawAs": "Image"},
            "Slot.Position": [0.0, 0.0],
            "Slot.Size": [1920.0, 1080.0],
        },
    )

    await create_widget(session, trace, "Image", "ScannerAccent", "RootCanvas")
    await set_props(
        session,
        trace,
        "ScannerAccent",
        {
            "Brush": {"ResourceObject": material_path, "DrawAs": "Image"},
            "RenderOpacity": 0.58,
            "Slot.Position": [126.0, 164.0],
            "Slot.Size": [600.0, 600.0],
        },
    )

    await create_widget(session, trace, "Overlay", "LoginCard", "RootCanvas")
    await set_props(
        session,
        trace,
        "LoginCard",
        {
            "RenderOpacity": 1.0,
            "RenderTransform.Pivot": {"X": 0.5, "Y": 0.5},
            "Slot.Position": [1180.0, 210.0],
            "Slot.Size": [520.0, 620.0],
        },
    )

    await create_widget(session, trace, "Border", "LoginCardBack", "LoginCard")
    await set_props(
        session,
        trace,
        "LoginCardBack",
        {
            "BrushColor": color(0.03, 0.045, 0.075, 0.94),
            "Padding": {"Left": 34.0, "Top": 34.0, "Right": 34.0, "Bottom": 34.0},
            "Slot": {"HorizontalAlignment": "HAlign_Fill", "VerticalAlignment": "VAlign_Fill"},
        },
    )

    await create_widget(session, trace, "VerticalBox", "LoginStack", "LoginCard")
    await set_props(
        session,
        trace,
        "LoginStack",
        {
            "Slot": {
                "HorizontalAlignment": "HAlign_Fill",
                "VerticalAlignment": "VAlign_Fill",
                "Padding": {"Left": 38.0, "Top": 36.0, "Right": 38.0, "Bottom": 36.0},
            }
        },
    )

    text_items = [
        ("LoginTitle", "NEXUS GATE", 38, color(0.88, 0.96, 1.0, 1.0)),
        ("LoginSubtitle", "Secure multiplayer access", 17, color(0.52, 0.76, 0.92, 1.0)),
        ("StatusText", "Status: waiting for credentials", 15, color(1.0, 0.74, 0.36, 1.0)),
    ]
    for name, text, size, tint in text_items[:2]:
        await create_widget(session, trace, "TextBlock", name, "LoginStack")
        await set_props(
            session,
            trace,
            name,
            {
                "Text": text,
                "Font": {"Size": size},
                "ColorAndOpacity": {"SpecifiedColor": tint},
                "Slot": {"Padding": {"Bottom": 8.0}},
            },
        )

    for overlay_name, border_name, edit_name, label in [
        ("UserField", "UserFieldBack", "PilotIdInput", "PILOT ID"),
        ("PassField", "PassFieldBack", "AccessKeyInput", "ACCESS KEY"),
    ]:
        await create_widget(session, trace, "Overlay", overlay_name, "LoginStack")
        await set_props(
            session,
            trace,
            overlay_name,
            {"Slot": {"Size": {"SizeRule": "Auto"}, "Padding": {"Top": 14.0, "Bottom": 4.0}}},
        )
        await create_widget(session, trace, "Border", border_name, overlay_name)
        await set_props(
            session,
            trace,
            border_name,
            {
                "BrushColor": color(0.015, 0.025, 0.04, 0.92),
                "Padding": {"Left": 16.0, "Top": 12.0, "Right": 16.0, "Bottom": 12.0},
                "Slot": {"HorizontalAlignment": "HAlign_Fill", "VerticalAlignment": "VAlign_Fill"},
            },
        )
        await create_widget(session, trace, "EditableTextBox", edit_name, overlay_name)
        await set_props(
            session,
            trace,
            edit_name,
            {
                "HintText": label,
                "Text": "",
                "Slot": {
                    "HorizontalAlignment": "HAlign_Fill",
                    "VerticalAlignment": "VAlign_Center",
                    "Padding": {"Left": 18.0, "Right": 18.0},
                },
            },
        )

    await create_widget(session, trace, "Overlay", "LoginButtonLayer", "LoginStack")
    await set_props(
        session,
        trace,
        "LoginButtonLayer",
        {"Slot": {"Size": {"SizeRule": "Auto"}, "Padding": {"Top": 26.0, "Bottom": 14.0}}},
    )
    await create_widget(session, trace, "Button", "LoginButton", "LoginButtonLayer")
    await set_props(
        session,
        trace,
        "LoginButton",
        {
            "RenderTransform.Pivot": {"X": 0.5, "Y": 0.5},
            "Slot": {"HorizontalAlignment": "HAlign_Fill", "VerticalAlignment": "VAlign_Fill"},
        },
    )
    await create_widget(session, trace, "TextBlock", "LoginButtonText", "LoginButtonLayer")
    await set_props(
        session,
        trace,
        "LoginButtonText",
        {
            "Text": "CONNECT",
            "Font": {"Size": 22},
            "ColorAndOpacity": {"SpecifiedColor": color(0.02, 0.06, 0.09, 1.0)},
            "Slot": {"HorizontalAlignment": "HAlign_Center", "VerticalAlignment": "VAlign_Center"},
        },
    )

    await create_widget(session, trace, "TextBlock", "StatusText", "LoginStack")
    await set_props(
        session,
        trace,
        "StatusText",
        {
            "Text": text_items[2][1],
            "Font": {"Size": text_items[2][2]},
            "ColorAndOpacity": {"SpecifiedColor": text_items[2][3]},
            "Slot": {"Padding": {"Top": 10.0}},
        },
    )

    await create_widget(session, trace, "HorizontalBox", "SystemStrip", "RootCanvas")
    await set_props(
        session,
        trace,
        "SystemStrip",
        {"Slot.Position": [120.0, 890.0], "Slot.Size": [1680.0, 70.0]},
    )

    systems = ["TARGET", "WIDGET", "MATERIAL", "SEQUENCER", "BLUEPRINT"]
    for index, label in enumerate(systems):
        overlay = f"Badge{index}_{label}"
        await create_widget(session, trace, "Overlay", overlay, "SystemStrip")
        await set_props(
            session,
            trace,
            overlay,
            {"Slot": {"Size": {"SizeRule": "Fill", "Value": 1.0}, "Padding": {"Right": 12.0}}},
        )
        await create_widget(session, trace, "Border", f"{overlay}_Back", overlay)
        await set_props(
            session,
            trace,
            f"{overlay}_Back",
            {
                "BrushColor": color(0.02, 0.09 + index * 0.015, 0.14, 0.86),
                "Slot": {"HorizontalAlignment": "HAlign_Fill", "VerticalAlignment": "VAlign_Fill"},
            },
        )
        await create_widget(session, trace, "TextBlock", f"{overlay}_Text", overlay)
        await set_props(
            session,
            trace,
            f"{overlay}_Text",
            {
                "Text": label,
                "Font": {"Size": 15},
                "ColorAndOpacity": {"SpecifiedColor": color(0.78, 0.93, 1.0, 1.0)},
                "Slot": {"HorizontalAlignment": "HAlign_Center", "VerticalAlignment": "VAlign_Center"},
            },
        )


async def build_animation(session: ClientSession, trace: TraceRecorder, animation_name: str) -> Dict[str, Any]:
    await call_tool(session, trace, "sequencer", "create_animation", {"animation_name": animation_name}, attempts=6)
    await call_tool(session, trace, "sequencer", "set_animation_scope", {"animation_name": animation_name}, attempts=6)
    await call_tool(session, trace, "sequencer", "set_target_widget", {"widget_name": "LoginCard"}, attempts=6)

    await call_tool(
        session,
        trace,
        "sequencer",
        "animation_append_widget_tracks",
        {
            "widget_name": "LoginCard",
            "animation_name": animation_name,
            "tracks": [
                {
                    "property": "RenderOpacity",
                    "keys": [
                        {"time": 0.0, "value": 0.0},
                        {"time": 0.45, "value": 1.0},
                        {"time": 1.2, "value": 1.0},
                    ],
                },
                {
                    "property": "RenderTransform.Scale.X",
                    "keys": [
                        {"time": 0.0, "value": 0.92},
                        {"time": 0.45, "value": 1.025},
                        {"time": 0.8, "value": 1.0},
                    ],
                },
                {
                    "property": "RenderTransform.Scale.Y",
                    "keys": [
                        {"time": 0.0, "value": 0.92},
                        {"time": 0.45, "value": 1.025},
                        {"time": 0.8, "value": 1.0},
                    ],
                },
            ],
        },
        attempts=6,
    )

    await call_tool(session, trace, "sequencer", "set_target_widget", {"widget_name": "ScannerAccent"}, attempts=6)
    await call_tool(
        session,
        trace,
        "sequencer",
        "animation_append_widget_tracks",
        {
            "widget_name": "ScannerAccent",
            "animation_name": animation_name,
            "tracks": [
                {
                    "property": "RenderTransform.Angle",
                    "keys": [{"time": 0.0, "value": -8.0}, {"time": 1.2, "value": 8.0}],
                }
            ],
        },
        attempts=6,
    )

    overview = await call_tool(
        session,
        trace,
        "sequencer",
        "animation_overview",
        {"animation_name": animation_name, "widget_name": "LoginCard"},
        attempts=6,
    )
    properties = {item.get("property") for item in overview.get("changed_properties", [])}
    require("RenderOpacity" in properties, "LoginCard RenderOpacity missing from animation overview", overview)
    require("RenderTransform.Scale.X" in properties, "LoginCard Scale.X missing from animation overview", overview)
    return overview


async def build_blueprint(session: ClientSession, trace: TraceRecorder, asset_path: str) -> Dict[str, Any]:
    await call_tool(session, trace, "blueprint", "compile_blueprint", {"blueprint_name": asset_path}, attempts=6)
    event = await call_tool(
        session,
        trace,
        "blueprint",
        "set_edit_function",
        {"function_name": "LoginButton.OnClicked"},
        attempts=6,
    )
    require(event.get("cursor_node"), "Component event binding did not produce a cursor node", event)
    await call_tool(
        session,
        trace,
        "blueprint",
        "add_step",
        {"name": "PrintString", "args": ["Login demo connect pressed"], "comment": "Demo button event"},
        attempts=6,
    )
    nodes = await call_tool(session, trace, "blueprint", "get_function_nodes", {}, attempts=6)
    require(nodes.get("nodes"), "Blueprint readback returned no nodes", nodes)
    await call_tool(session, trace, "blueprint", "compile_blueprint", {"blueprint_name": asset_path}, attempts=6)
    return nodes


def require_tools(tool_names: Iterable[str], required: Iterable[str]) -> None:
    names = set(tool_names)
    missing = sorted(set(required) - names)
    require(not missing, f"MCP tools missing: {missing}", sorted(names))


async def run_demo(args: argparse.Namespace) -> None:
    stamp = args.stamp or time.strftime("%Y%m%d_%H%M%S")
    asset_path = args.asset_path or f"/Game/UMGMCP/Demos/W_LoginFiveSystems_{stamp}"
    material_path = args.material_path or f"/Game/UMGMCP/Demos/M_LoginBackground_{stamp}"
    animation_name = args.animation_name or "LoginIntro"
    trace_path = Path(args.trace_path) if args.trace_path else ARTIFACT_ROOT / f"login_demo_five_systems_{stamp}.jsonl"

    server = StdioServerParameters(
        command=args.server_command,
        args=args.server_args or ["run", "--directory", str(PYTHON_ROOT), "UmgMcpServer.py"],
    )

    trace = TraceRecorder(trace_path)
    try:
        async with stdio_client(server) as (read, write):
            async with ClientSession(read, write) as session:
                await session.initialize()
                listed = await session.list_tools()
                tool_names = {tool.name for tool in listed.tools}
                require_tools(
                    tool_names,
                    [
                        "set_target_umg_asset",
                        "set_target_widget",
                        "create_widget",
                        "set_widget_properties",
                        "get_widget_tree",
                        "query_widget_properties",
                        "hlsl_set_target",
                        "hlsl_set",
                        "hlsl_compile",
                        "create_animation",
                        "set_animation_scope",
                        "animation_append_widget_tracks",
                        "animation_overview",
                        "set_edit_function",
                        "add_step",
                        "get_function_nodes",
                        "compile_blueprint",
                        "save_asset",
                    ],
                )
                trace.write({"system": "mcp", "tool": "list_tools", "tool_count": len(tool_names)})

                await build_material(session, trace, material_path)

                await call_tool(
                    session,
                    trace,
                    "target",
                    "set_target_umg_asset",
                    {"asset_path": asset_path},
                    attempts=6,
                )
                await build_widgets(session, trace, material_path)

                await call_tool(session, trace, "target", "set_target_widget", {"widget_name": "RootCanvas"}, attempts=6)
                tree = await call_tool(session, trace, "readback", "get_widget_tree", {}, attempts=6)
                tree_text = json.dumps(tree, ensure_ascii=False)
                for widget_name in ("RootCanvas", "LoginCard", "LoginButton", "SystemStrip"):
                    require(widget_name in tree_text, f"{widget_name} missing from widget tree", tree)

                sequencer_overview = await build_animation(session, trace, animation_name)
                blueprint_nodes = await build_blueprint(session, trace, asset_path)

                queried = await call_tool(
                    session,
                    trace,
                    "readback",
                    "query_widget_properties",
                    {"widget_name": "LoginCard", "properties": ["RenderOpacity", "Slot.Position", "Slot.Size"]},
                    attempts=6,
                )
                await call_tool(session, trace, "target", "save_asset", {}, attempts=6)

                summary = {
                    "asset_path": asset_path,
                    "material_path": material_path,
                    "animation_name": animation_name,
                    "trace_path": str(trace_path),
                    "mcp_tool_count": len(tool_names),
                    "sequencer_track_count": sequencer_overview.get("track_count"),
                    "blueprint_node_count": len(blueprint_nodes.get("nodes", [])),
                    "query_keys": sorted(queried.keys()),
                }
                trace.write({"system": "summary", "result": summary})
                print(json.dumps(summary, indent=2, ensure_ascii=False))
    finally:
        trace.close()


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Build a five-system UMG login demo through stdio MCP.")
    parser.add_argument("--asset-path", default="", help="UMG asset path. Defaults to a timestamped /Game/UMGMCP/Demos asset.")
    parser.add_argument("--material-path", default="", help="Material asset path. Defaults to a timestamped /Game/UMGMCP/Demos asset.")
    parser.add_argument("--animation-name", default="LoginIntro", help="Widget animation name.")
    parser.add_argument("--stamp", default="", help="Optional deterministic suffix for default asset names.")
    parser.add_argument("--trace-path", default="", help="JSONL operation trace path.")
    parser.add_argument("--server-command", default="uv", help="Command used to launch the MCP server.")
    parser.add_argument(
        "--server-args",
        nargs=argparse.REMAINDER,
        help="Arguments used after --server-command. Defaults to: run --directory <Resources/Python> UmgMcpServer.py",
    )
    return parser.parse_args()


if __name__ == "__main__":
    if not (PYTHON_ROOT / "UmgMcpServer.py").exists():
        raise FileNotFoundError(PYTHON_ROOT / "UmgMcpServer.py")
    asyncio.run(run_demo(parse_args()))
