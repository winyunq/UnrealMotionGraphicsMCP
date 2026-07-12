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
    spec = importlib.util.spec_from_file_location("UmgMcpServer_bluecode_payload_check", SERVER_PATH)
    if spec is None or spec.loader is None:
        raise AssertionError(f"Unable to load {SERVER_PATH}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class FakeConnection:
    def __init__(self):
        self.calls = []
        self.fail_writes = False
        self.fail_read = False
        self.semantic_backend_unavailable = False

    async def send_command(self, command, params=None):
        payload = params or {}
        self.calls.append({"command": command, "params": payload})
        if payload.get("subAction") == "get_variables":
            return {"success": True, "variables": []}
        if payload.get("subAction") == "bluecode_read_function":
            if self.fail_read:
                return {"status": "error", "error": "simulated backend read failure"}
            if self.semantic_backend_unavailable:
                return {"success": True, "warning": "simulated old backend without semantic code"}
            return {
                "success": True,
                "code": "main()\n  PrintString(\"Read\")\n  end",
                "action_hints": {"PrintString(\"Read\")": {"action_handle": "bpact:read"}},
                "expression_hints": {"value(\"ReadExpr\")": {"action_handle": "bpact:read-expr"}},
            }
        if payload.get("subAction") == "get_nodes" and self.fail_read:
            return {"status": "error", "error": "simulated raw read failure"}
        if payload.get("subAction") == "get_nodes" and self.semantic_backend_unavailable:
            return {"success": True, "nodes": [{"id": "RawNode", "name": "K2Node_CallFunction_0", "class": "K2Node_CallFunction", "isExec": True}]}
        if self.fail_writes and payload.get("subAction") in ("add_function_step", "create_node"):
            return {"success": False, "error": "simulated node creation failure"}
        return {"success": True, "nodeId": "NodeA", "isExec": True}


async def main() -> None:
    server = load_server_module()
    fake = FakeConnection()
    server.get_unreal_connection = lambda: fake

    read_result = await server.bluecode_read_function(detail="debug")
    if "action_hints" not in read_result:
        raise AssertionError(f"bluecode_read_function did not return backend action_hints: {read_result}")
    read_payloads = [
        call["params"]
        for call in fake.calls
        if call["command"] == "manage_blueprint_graph"
        and call["params"].get("subAction") == "bluecode_read_function"
    ]
    expected_read = [{"subAction": "bluecode_read_function", "detail": "debug"}]
    if read_payloads != expected_read:
        raise AssertionError(f"Unexpected bluecode_read_function payloads:\nexpected={expected_read}\nactual={read_payloads}")
    fake.calls.clear()

    read_result = await server.bluecode_read_function(detail="roundtrip", include_connections=True)
    if "action_hints" not in read_result:
        raise AssertionError(f"bluecode_read_function roundtrip read did not return backend action_hints: {read_result}")
    read_payloads = [
        call["params"]
        for call in fake.calls
        if call["command"] == "manage_blueprint_graph"
        and call["params"].get("subAction") == "bluecode_read_function"
    ]
    expected_read = [{"subAction": "bluecode_read_function", "detail": "roundtrip", "include_connections": True}]
    if read_payloads != expected_read:
        raise AssertionError(f"Unexpected roundtrip bluecode_read_function payloads:\nexpected={expected_read}\nactual={read_payloads}")
    fake.calls.clear()

    fake.fail_read = True
    read_result = await server.bluecode_read_function(detail="debug")
    if read_result.get("success") is not False or "raw_error" not in read_result:
        raise AssertionError(f"Expected failed bluecode_read_function to expose raw_error and success=false: {read_result}")
    fake.fail_read = False
    fake.calls.clear()

    fake.semantic_backend_unavailable = True
    read_result = await server.bluecode_read_function(detail="debug")
    if read_result.get("success") is not False or not read_result.get("semantic_read_degraded"):
        raise AssertionError(f"Expected raw-node fallback to fail semantic read: {read_result}")
    fake.semantic_backend_unavailable = False
    fake.calls.clear()

    result = await server.bluecode_apply(
        'node("Duplicate Custom", Pin=Changed)',
        mode="append",
        member_classes={"CustomClass.DoThing": "/Script/Test.CustomClass"},
        action_handles={"Duplicate Custom": "bpact:wrong-map-handle"},
        action_hints={"Duplicate Custom": {"handle": "bpact:wrong-map-handle", "signature": "WrongMap"}},
        expression_hints={'value("Duplicate Pure", Pin=Changed)': {"handle": "bpact:pure-one", "signature": "PureOne"}},
        node_properties={"Duplicate Custom": {"bAdvanced": True, "Mode": "Demo"}},
        action_hints_by_line=[{
            "line": 2,
            "statement": 'node("Duplicate Custom", Pin=Old)',
            "hint": {
                "handle": "bpact:duplicate-one",
                "signature": "DuplicateOne",
                "node_id": "ExistingNodeA",
            },
        }],
        node_aliases={'node("Duplicate Custom", Pin=Changed)': "UpdatedNode"},
    )
    if not result.get("success"):
        raise AssertionError(f"Unexpected bluecode_apply failure: {result}")
    apply_payloads = [
        call["params"]
        for call in fake.calls
        if call["command"] == "manage_blueprint_graph"
        and call["params"].get("subAction") == "bluecode_apply"
    ]
    expected_apply = [{
        "subAction": "bluecode_apply",
        "code": 'node("Duplicate Custom", Pin=Changed)',
        "anchor": "end",
        "mode": "append",
        "member_classes": {"CustomClass.DoThing": "/Script/Test.CustomClass"},
        "action_handles": {"Duplicate Custom": "bpact:wrong-map-handle"},
        "action_hints": {"Duplicate Custom": {"handle": "bpact:wrong-map-handle", "signature": "WrongMap"}},
        "expression_hints": {'value("Duplicate Pure", Pin=Changed)': {"handle": "bpact:pure-one", "signature": "PureOne"}},
        "node_properties": {"Duplicate Custom": {"bAdvanced": True, "Mode": "Demo"}},
        "action_hints_by_line": [{
            "line": 2,
            "statement": 'node("Duplicate Custom", Pin=Old)',
            "hint": {
                "handle": "bpact:duplicate-one",
                "signature": "DuplicateOne",
                "node_id": "ExistingNodeA",
            },
        }],
        "node_aliases": {'node("Duplicate Custom", Pin=Changed)': "UpdatedNode"},
    }]
    if apply_payloads != expected_apply:
        raise AssertionError(f"Unexpected bluecode_apply payloads:\nexpected={expected_apply}\nactual={apply_payloads}")

    fake.calls.clear()
    result = await server.bluecode_connect([
        {"source": "SelectTheme:Return Value", "target": "ApplyTheme:Theme"},
        "IsValid:Return Value -> Branch:Condition",
    ], aliases={"SelectTheme": "NodeA", "ApplyTheme": "NodeB"})
    if not result.get("success"):
        raise AssertionError(f"Unexpected bluecode_connect failure: {result}")
    connect_payloads = [
        call["params"]
        for call in fake.calls
        if call["command"] == "manage_blueprint_graph"
        and call["params"].get("subAction") == "bluecode_connect"
    ]
    expected_connect = [{
        "subAction": "bluecode_connect",
        "connects": [
            {"source": "SelectTheme:Return Value", "target": "ApplyTheme:Theme"},
            "IsValid:Return Value -> Branch:Condition",
        ],
        "aliases": {"SelectTheme": "NodeA", "ApplyTheme": "NodeB"},
    }]
    if connect_payloads != expected_connect:
        raise AssertionError(f"Unexpected bluecode_connect payloads:\nexpected={expected_connect}\nactual={connect_payloads}")

    fake.calls.clear()
    result = await server.bluecode_delete(
        [{"kind": "connection", "source": "SelectTheme:Return Value", "target": "ApplyTheme:Theme"}],
        confirm_delete=True,
        aliases={"SelectTheme": "NodeA", "ApplyTheme": "NodeB"},
    )
    if not result.get("success"):
        raise AssertionError(f"Unexpected bluecode_delete failure: {result}")
    delete_payloads = [
        call["params"]
        for call in fake.calls
        if call["command"] == "manage_blueprint_graph"
        and call["params"].get("subAction") == "bluecode_delete"
    ]
    expected_delete = [{
        "subAction": "bluecode_delete",
        "targets": [{"kind": "connection", "source": "SelectTheme:Return Value", "target": "ApplyTheme:Theme"}],
        "confirm_delete": True,
        "aliases": {"SelectTheme": "NodeA", "ApplyTheme": "NodeB"},
    }]
    if delete_payloads != expected_delete:
        raise AssertionError(f"Unexpected bluecode_delete payloads:\nexpected={expected_delete}\nactual={delete_payloads}")

    print("Blueprint MCP server bluecode payload check passed.")


if __name__ == "__main__":
    asyncio.run(main())
