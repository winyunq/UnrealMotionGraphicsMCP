import asyncio
import json
import logging
import os
import re
import sys
from typing import Dict, Any, List, Optional

# Setup logging
logging.basicConfig(level=logging.INFO, stream=sys.stderr)
logger = logging.getLogger("UmgMcpSkills")

# Import mcp_config from parent dir
CURRENT_DIR = os.path.dirname(os.path.abspath(__file__))
sys.path.append(CURRENT_DIR)
try:
    from mcp_config import UNREAL_HOST, UNREAL_PORT
except ImportError:
    UNREAL_HOST = "127.0.0.1"
    UNREAL_PORT = 7999

class UnrealConnection:
    """Connection helper for Skill mode."""
    async def send_command(self, command: str, params: Dict[str, Any] = None) -> Dict[str, Any]:
        try:
            reader, writer = await asyncio.open_connection(UNREAL_HOST, UNREAL_PORT)
            payload = json.dumps({"command": command, "params": params or {}})
            writer.write(payload.encode('utf-8') + b'\0')
            await writer.drain()
            
            if writer.can_write_eof():
                writer.write_eof()
                
            data = await reader.read(1024 * 1024) # 1MB buffer
            response_str = data.decode('utf-8').split('\0')[0]
            writer.close()
            await writer.wait_closed()
            return json.loads(response_str)
        except Exception as e:
            return {"status": "error", "error": str(e)}

_conn = UnrealConnection()

def run_async(coro):
    """Helper to run async in sync skill context."""
    return asyncio.run(coro)


def _send(command: str, params: Dict[str, Any] = None) -> Dict[str, Any]:
    return run_async(_conn.send_command(command, params or {}))


def _ok(result: Any) -> bool:
    return isinstance(result, dict) and (result.get("success") is True or result.get("status") == "success")


_BLUECODE_WIDGET_EVENTS = {
    "Button": ["OnClicked", "OnPressed", "OnReleased", "OnHovered", "OnUnhovered"],
    "CheckBox": ["OnCheckStateChanged"],
    "ComboBoxString": ["OnSelectionChanged", "OnOpening"],
    "EditableTextBox": ["OnTextChanged", "OnTextCommitted"],
    "MultiLineEditableTextBox": ["OnTextChanged", "OnTextCommitted"],
    "Slider": ["OnValueChanged", "OnMouseCaptureBegin", "OnMouseCaptureEnd", "OnControllerCaptureBegin", "OnControllerCaptureEnd"],
    "SpinBox": ["OnValueChanged", "OnValueCommitted", "OnBeginSliderMovement", "OnEndSliderMovement"],
    "ExpandableArea": ["OnExpansionChanged"],
}


def _normalize_widget_class(class_name: str) -> str:
    normalized = (class_name or "").strip()
    if normalized.startswith("U") and len(normalized) > 1 and normalized[1].isupper():
        normalized = normalized[1:]
    if normalized.endswith("_C"):
        normalized = normalized[:-2]
    return normalized


def _parse_widget_tree(tree_text: str) -> List[Dict[str, str]]:
    widgets: List[Dict[str, str]] = []
    for raw_line in (tree_text or "").splitlines():
        match = re.match(r"^\s*(?:-\s*)?([A-Za-z_][A-Za-z0-9_]*)\s+\[([^\]]+)\]", raw_line.strip())
        if not match:
            continue
        widget_class = _normalize_widget_class(match.group(2))
        if widget_class == "WidgetBlueprint":
            continue
        widgets.append({"widget": match.group(1), "class": widget_class})
    return widgets


def _available_events(widgets: List[Dict[str, str]]) -> List[Dict[str, str]]:
    events: List[Dict[str, str]] = []
    for widget in widgets:
        for event_name in _BLUECODE_WIDGET_EVENTS.get(widget.get("class", ""), []):
            target = f"{widget.get('widget')}.{event_name}"
            events.append({
                "widget": widget.get("widget", ""),
                "class": widget.get("class", ""),
                "event": event_name,
                "target": target,
                "set_function": f'bluecode_set_function("{target}")',
            })
    return events


def _bound_events(nodes: List[Dict[str, Any]]) -> List[Dict[str, Any]]:
    events: List[Dict[str, Any]] = []
    seen = set()
    for node in nodes or []:
        kind = node.get("kind")
        if kind not in ("component_event", "custom_event", "event"):
            continue
        label = str(node.get("member") or node.get("title") or node.get("name") or "")
        event_name = label.replace(" ", "")
        widget_name = ""
        target = event_name
        label_match = re.match(r"^\s*(.*?)\s*\((.*?)\)\s*$", label)
        if kind == "component_event" and label_match:
            event_name = label_match.group(1).replace(" ", "")
            widget_name = label_match.group(2).strip()
            target = f"{widget_name}.{event_name}" if widget_name else event_name
        key = (kind, target, node.get("id"))
        if key in seen:
            continue
        seen.add(key)
        events.append({
            "kind": kind,
            "widget": widget_name,
            "event": event_name,
            "target": target,
            "node_id": node.get("id"),
            "title": node.get("title") or label,
        })
    return events


_MATH_READBACK = {
    "Add_IntInt": "+",
    "Add_FloatFloat": "+",
    "Subtract_IntInt": "-",
    "Subtract_FloatFloat": "-",
    "Multiply_IntInt": "*",
    "Multiply_FloatFloat": "*",
    "Divide_IntInt": "/",
    "Divide_FloatFloat": "/",
}


def _pin_expr(pin: Dict[str, Any], by_id: Dict[str, Dict[str, Any]], seen: Optional[set] = None) -> str:
    linked_to = pin.get("linked_to") or []
    if linked_to:
        linked = linked_to[0]
        linked_id = linked.get("node_id")
        if linked_id and linked_id in by_id:
            return _node_expr(by_id[linked_id], by_id, seen)
        return str(linked.get("member") or linked.get("title") or linked.get("node") or linked_id or "")
    return str(pin.get("default") or "")


def _node_expr(node: Dict[str, Any], by_id: Dict[str, Dict[str, Any]], seen: Optional[set] = None) -> str:
    seen = seen or set()
    node_id = node.get("id")
    if node_id in seen:
        return str(node.get("member") or node.get("title") or node.get("name") or node_id)
    if node_id:
        seen.add(node_id)
    kind = node.get("kind")
    member = str(node.get("member") or node.get("title") or node.get("name") or "")
    if kind == "variable_get":
        return member
    args = []
    for pin in node.get("inputs", []):
        pin_name = str(pin.get("name", "")).lower()
        if pin.get("is_exec") or pin_name in ("execute", "then", "self", "worldcontextobject"):
            continue
        value = _pin_expr(pin, by_id, seen)
        if value:
            args.append(value)
    if kind == "call" and member in _MATH_READBACK and len(args) >= 2:
        return f"{args[0]} {_MATH_READBACK[member]} {args[1]}"
    if kind == "call" and member:
        return f"{member}({', '.join(args)})"
    return member


def _simple_bluecode_from_nodes(nodes: List[Dict[str, Any]]) -> str:
    by_id = {node.get("id"): node for node in nodes if node.get("id")}
    dependency_node_ids = {
        dep.get("node_id")
        for node in nodes
        for dep in (node.get("data_dependencies", []) or [])
        if dep.get("node_id")
    }
    lines = ["main()"]
    for node in nodes:
        if node.get("id") in dependency_node_ids and not node.get("isExec"):
            continue
        kind = node.get("kind")
        if kind in ("event", "component_event", "custom_event", "function_entry"):
            continue
        member = node.get("member") or node.get("title") or node.get("name")
        if member:
            if kind == "variable_set":
                value = ""
                for pin in node.get("inputs", []):
                    if pin.get("is_exec"):
                        continue
                    value = _pin_expr(pin, by_id)
                    if value:
                        break
                lines.append(f"  {member} = {value}".rstrip())
            elif kind == "branch":
                condition = ""
                for pin in node.get("inputs", []):
                    if str(pin.get("name", "")).lower() == "condition":
                        condition = _pin_expr(pin, by_id)
                        break
                lines.append(f"  if {condition or 'Condition'}:")
            else:
                lines.append(f"  {member}()")
    lines.append("  end")
    return "\n".join(lines)


def _is_raw_bluecode_node_dump_line(statement: str) -> bool:
    text = str(statement or "").strip()
    if text.endswith("()"):
        text = text[:-2].strip()
    return bool(re.match(r"^(K2Node|EdGraphNode|MaterialGraphNode|AnimGraphNode)_", text))


def _connections_from_bluecode_nodes(nodes: List[Dict[str, Any]]) -> List[Dict[str, Any]]:
    connections: List[Dict[str, Any]] = []
    seen = set()
    for node in nodes or []:
        source_node_id = node.get("id") or ""
        source_title = node.get("title") or node.get("member") or node.get("name") or ""
        for pin in node.get("outputs", []) or []:
            source_pin = pin.get("name") or ""
            source_endpoint = f"{source_node_id}:{source_pin}"
            kind = "exec" if pin.get("is_exec") else "data"
            for linked in pin.get("linked_to", []) or []:
                target_node_id = linked.get("node_id") or ""
                target_pin = linked.get("pin") or ""
                target_endpoint = f"{target_node_id}:{target_pin}"
                key = f"{source_endpoint}->{target_endpoint}"
                if key in seen:
                    continue
                seen.add(key)
                connections.append({
                    "source": source_endpoint,
                    "target": target_endpoint,
                    "source_node_id": source_node_id,
                    "source_title": source_title,
                    "source_pin": source_pin,
                    "target_node_id": target_node_id,
                    "target_title": linked.get("title") or linked.get("member") or linked.get("node") or "",
                    "target_pin": target_pin,
                    "kind": kind,
                    "connect": f"{source_endpoint} -> {target_endpoint}",
                    "delete_target": {
                        "kind": "connection",
                        "source": source_endpoint,
                        "target": target_endpoint,
                    },
                })
    return connections


def bluecode_set_function(function_name: str) -> Dict[str, Any]:
    return _send("set_edit_function", {"function_name": function_name})


def bluecode_read_function(detail: str = "", include_connections: bool = False) -> Dict[str, Any]:
    backend_payload: Dict[str, Any] = {"subAction": "bluecode_read_function"}
    if detail:
        backend_payload["detail"] = detail
    if include_connections:
        backend_payload["include_connections"] = True
    backend_result = _send("manage_blueprint_graph", backend_payload)
    if isinstance(backend_result, dict) and _ok(backend_result) and backend_result.get("code"):
        return backend_result

    graph = _send("get_target_graph", {})
    raw = _send("manage_blueprint_graph", {"subAction": "get_nodes"})
    nodes = raw.get("nodes", []) if isinstance(raw, dict) else []
    code = _simple_bluecode_from_nodes(nodes)
    degraded_lines = [
        line.strip()
        for line in code.splitlines()
        if _is_raw_bluecode_node_dump_line(line.strip())
    ]
    semantic_read_degraded = bool(degraded_lines)
    result = {
        "success": _ok(raw) and not semantic_read_degraded,
        "function": graph.get("target_graph", "EventGraph") if isinstance(graph, dict) else "EventGraph",
        "entry": "main",
        "exit": "end",
        "code": code,
        "debug_nodes_available": True,
        "fallback_used": True,
    }
    connections = _connections_from_bluecode_nodes(nodes)
    result["connection_count"] = len(connections)
    result["connections_available"] = True
    if include_connections or detail in ("debug", "roundtrip"):
        result["connections"] = connections
        result["connections_usage"] = "Use bluecode_connect to add missing links and bluecode_delete(kind='connection', confirm_delete=true) to remove specific existing links."
    if semantic_read_degraded:
        result["semantic_read_degraded"] = True
        result["error"] = "Backend bluecode_read_function did not return semantic code; legacy get_nodes fallback only produced raw node names."
        result["raw_node_lines"] = degraded_lines
    if isinstance(backend_result, dict) and not _ok(backend_result):
        result["backend_error"] = backend_result
    elif isinstance(backend_result, dict):
        result["backend_warning"] = backend_result
    if isinstance(raw, dict) and not _ok(raw):
        result["raw_error"] = raw
    if detail == "debug":
        result["nodes"] = nodes
    return result


def bluecode_read_variables() -> Dict[str, Any]:
    return _send("manage_blueprint_graph", {"subAction": "get_variables"})


def bluecode_read_events() -> Dict[str, Any]:
    tree_result = _send("get_widget_tree", {})
    payload: Dict[str, Any] = {"subAction": "get_events"}
    if isinstance(tree_result, dict) and tree_result.get("scope") == "target_widget" and tree_result.get("root_widget"):
        payload["scope_widget"] = tree_result.get("root_widget")
    events_result = _send("manage_blueprint_graph", payload)
    if isinstance(events_result, dict) and _ok(events_result) and (
        "available_events" in events_result or "bound_events" in events_result
    ):
        return events_result

    graph = _send("get_target_graph", {})
    raw = _send("manage_blueprint_graph", {"subAction": "get_nodes"})
    widgets = _parse_widget_tree(tree_result.get("widget_tree", "") if isinstance(tree_result, dict) else "")
    nodes = raw.get("nodes", []) if isinstance(raw, dict) else []
    return {
        "success": bool((tree_result or {}).get("success", True)) if isinstance(tree_result, dict) else False,
        "bound_scope": graph.get("target_graph", "EventGraph") if isinstance(graph, dict) else "EventGraph",
        "widgets": widgets,
        "available_events": _available_events(widgets),
        "bound_events": _bound_events(nodes),
        "notes": [
            "available_events are inferred from common UMG widget delegate names",
            "bound_events are read from the current bluecode graph scope, not a full side-effect-free EventGraph scan",
        ],
    }


def bluecode_search_nodes(query: str = "", category: str = "", node_class: str = "", node_class_path: str = "", max_count: int = 50, include_pins: bool = False, context_sensitive: bool = True, exact: bool = False) -> Dict[str, Any]:
    payload: Dict[str, Any] = {
        "subAction": "bluecode_search_nodes",
        "query": query,
        "max_count": max_count,
        "include_pins": include_pins,
        "context_sensitive": context_sensitive,
        "exact": exact,
    }
    if category:
        payload["category"] = category
    if node_class:
        payload["node_class"] = node_class
    if node_class_path:
        payload["node_class_path"] = node_class_path
    return _send("manage_blueprint_graph", payload)


def bluecode_apply(code: str, anchor: str = "end", mode: str = "append", member_classes: Dict[str, str] = None, action_handles: Dict[str, str] = None, action_hints: Dict[str, Dict[str, Any]] = None, expression_hints: Dict[str, Dict[str, Any]] = None, action_hints_by_line: List[Dict[str, Any]] = None, node_aliases: Dict[str, str] = None, aliases: Dict[str, str] = None, node_properties: Dict[str, Dict[str, Any]] = None) -> Dict[str, Any]:
    if mode not in ("append", "upsert"):
        return {"success": False, "error": "bluecode_apply supports mode='append' or mode='upsert' only"}
    payload: Dict[str, Any] = {
        "subAction": "bluecode_apply",
        "code": code,
        "anchor": anchor,
        "mode": mode,
    }
    for key, value in (
        ("member_classes", member_classes),
        ("action_handles", action_handles),
        ("action_hints", action_hints),
        ("expression_hints", expression_hints),
        ("action_hints_by_line", action_hints_by_line),
        ("node_aliases", node_aliases),
        ("aliases", aliases),
        ("node_properties", node_properties),
    ):
        if value is not None:
            payload[key] = value
    return _send("manage_blueprint_graph", payload)


def bluecode_apply_variables(variables: List[Dict[str, Any]]) -> Dict[str, Any]:
    operations = []
    for variable in variables or []:
        payload = {"subAction": "add_variable", "name": variable.get("name"), "type": variable.get("type")}
        if variable.get("subType"):
            payload["subType"] = variable.get("subType")
        operations.append({"variable": variable, "result": _send("manage_blueprint_graph", payload)})
    return {
        "success": all(_ok(op.get("result")) for op in operations),
        "applied_count": sum(1 for op in operations if _ok(op.get("result"))),
        "operations": operations,
    }


def bluecode_connect(connects: List[Any] = None, source: str = "", target: str = "", aliases: Dict[str, str] = None) -> Dict[str, Any]:
    payload: Dict[str, Any] = {"subAction": "bluecode_connect"}
    if connects is not None:
        payload["connects"] = connects
    elif source or target:
        payload["connects"] = [{"source": source, "target": target}]
    else:
        payload["connects"] = []
    if aliases:
        payload["aliases"] = aliases
    return _send("manage_blueprint_graph", payload)


def bluecode_delete(targets: List[Any], confirm_delete: bool = False, aliases: Dict[str, str] = None) -> Dict[str, Any]:
    if not confirm_delete:
        return {"success": False, "error": "bluecode_delete requires confirm_delete=true"}
    payload: Dict[str, Any] = {
        "subAction": "bluecode_delete",
        "targets": targets or [],
        "confirm_delete": True,
    }
    if aliases:
        payload["aliases"] = aliases
    return _send("manage_blueprint_graph", payload)


def bluecode_compile(blueprint_name: str = None) -> Dict[str, Any]:
    return _send("compile_blueprint", {"blueprint_name": blueprint_name})

# =============================================================================
#  Dynamic Skill Loading from prompts.json
# =============================================================================

def load_skills():
    """Reads prompts.json and registers functions to the global namespace."""
    json_path = os.path.abspath(os.path.join(CURRENT_DIR, "..", "prompts.json"))
    if not os.path.exists(json_path):
        logger.warning(f"prompts.json not found at {json_path}")
        return

    try:
        with open(json_path, 'r', encoding='utf-8') as f:
            data = json.load(f)
            tools = data.get("tools", [])
            
            for tool in tools:
                if not tool.get("enabled", True):
                    continue
                
                name = tool["name"]
                desc = tool.get("description", "No description provided.")
                
                if name in globals() and callable(globals()[name]):
                    func = globals()[name]
                else:
                    # Create a wrapper function. We use a closure to capture the correct tool name.
                    def create_wrapper(command_name):
                        def skill_wrapper(**kwargs):
                            return run_async(_conn.send_command(command_name, kwargs))
                        return skill_wrapper

                    func = create_wrapper(name)
                func.__name__ = name
                func.__doc__ = desc
                
                # Register in globals so Gemini CLI can see it as an export
                globals()[name] = func
                logger.info(f"Registered Skill: {name}")

    except Exception as e:
        logger.error(f"Error loading skills: {e}")

# Load all skills on module import
load_skills()
