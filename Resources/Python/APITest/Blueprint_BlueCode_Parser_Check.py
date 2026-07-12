import importlib.util
import logging
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SKILLS_PATH = ROOT / "UmgMcpSkills.py"


def load_skills_module():
    logging.disable(logging.CRITICAL)
    spec = importlib.util.spec_from_file_location("UmgMcpSkills_payload_check", SKILLS_PATH)
    if spec is None or spec.loader is None:
        raise AssertionError(f"Unable to load {SKILLS_PATH}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def main() -> None:
    skills = load_skills_module()
    captured = []

    def fake_send(command, params=None):
        captured.append({"command": command, "params": params or {}})
        if params and params.get("subAction") == "bluecode_read_function":
            return {
                "success": True,
                "code": "main()\n  PrintString(\"Read\")\n  end",
                "action_hints": {"PrintString(\"Read\")": {"action_handle": "bpact:read"}},
                "expression_hints": {"value(\"ReadExpr\")": {"action_handle": "bpact:read-expr"}},
            }
        return {"success": True, "nodeId": "NodeA", "isExec": True}

    skills._send = fake_send

    read_result = skills.bluecode_read_function(detail="debug", include_connections=True)
    assert "action_hints" in read_result
    assert "expression_hints" in read_result
    read_payloads = [
        entry["params"]
        for entry in captured
        if entry["command"] == "manage_blueprint_graph"
        and entry["params"].get("subAction") == "bluecode_read_function"
    ]
    assert read_payloads == [{
        "subAction": "bluecode_read_function",
        "detail": "debug",
        "include_connections": True,
    }]

    captured.clear()
    result = skills.bluecode_apply(
        'node("Set Timer by Event", Time=2.0, Event=TickEvent, alias=TimerNode)',
        action_handles={"settimerbyevent": "bpact:test-handle"},
        action_hints={"Set Timer by Event": {"handle": "bpact:search-result-handle"}},
        expression_hints={'value("Select", Index=ActiveIndex)': {"handle": "bpact:select"}},
        node_properties={"Set Timer by Event": {"bLooping": True, "InitialStartDelay": 0.25}},
        action_hints_by_line=[{
            "line": 2,
            "statement": 'node("Set Timer by Event", Time=1.0, Event=TickEvent, alias=TimerNode)',
            "hint": {
                "handle": "bpact:search-result-handle",
                "node_id": "ExistingTimerNode",
            },
        }],
        node_aliases={"Set Timer by Event": "TimerNode"},
        member_classes={"CustomClass.DoThing": "/Script/Test.CustomClass"},
    )
    assert result["success"] is True
    write_payloads = [
        entry["params"]
        for entry in captured
        if entry["command"] == "manage_blueprint_graph"
        and entry["params"].get("subAction") == "bluecode_apply"
    ]
    assert write_payloads == [{
        "subAction": "bluecode_apply",
        "code": 'node("Set Timer by Event", Time=2.0, Event=TickEvent, alias=TimerNode)',
        "anchor": "end",
        "mode": "append",
        "member_classes": {"CustomClass.DoThing": "/Script/Test.CustomClass"},
        "action_handles": {"settimerbyevent": "bpact:test-handle"},
        "action_hints": {"Set Timer by Event": {"handle": "bpact:search-result-handle"}},
        "expression_hints": {'value("Select", Index=ActiveIndex)': {"handle": "bpact:select"}},
        "node_properties": {"Set Timer by Event": {"bLooping": True, "InitialStartDelay": 0.25}},
        "action_hints_by_line": [{
            "line": 2,
            "statement": 'node("Set Timer by Event", Time=1.0, Event=TickEvent, alias=TimerNode)',
            "hint": {
                "handle": "bpact:search-result-handle",
                "node_id": "ExistingTimerNode",
            },
        }],
        "node_aliases": {"Set Timer by Event": "TimerNode"},
    }]

    captured.clear()
    result = skills.bluecode_connect([
        {"source": "SelectTheme:Return Value", "target": "ApplyTheme:Theme"},
        "IsValid:Return Value -> Branch:Condition",
    ], aliases={"SelectTheme": "NodeA", "ApplyTheme": "NodeB"})
    assert result["success"] is True
    connect_payloads = [
        entry["params"]
        for entry in captured
        if entry["command"] == "manage_blueprint_graph"
        and entry["params"].get("subAction") == "bluecode_connect"
    ]
    assert connect_payloads == [{
        "subAction": "bluecode_connect",
        "connects": [
            {"source": "SelectTheme:Return Value", "target": "ApplyTheme:Theme"},
            "IsValid:Return Value -> Branch:Condition",
        ],
        "aliases": {"SelectTheme": "NodeA", "ApplyTheme": "NodeB"},
    }]

    print("Blueprint BlueCode payload forwarding check passed.")


if __name__ == "__main__":
    main()
