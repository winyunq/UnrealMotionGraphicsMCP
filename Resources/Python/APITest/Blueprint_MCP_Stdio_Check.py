import json
import os
import subprocess
import sys
import time
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SERVER_PATH = ROOT / "UmgMcpServer.py"


def encode_message(payload: dict) -> bytes:
    return (json.dumps(payload, separators=(",", ":"), ensure_ascii=False) + "\n").encode("utf-8")


def read_message(proc: subprocess.Popen, timeout: float = 15.0) -> dict:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        line = proc.stdout.readline()
        if line:
            return json.loads(line.decode("utf-8"))
        if proc.poll() is not None:
            raise AssertionError(f"MCP server exited with code {proc.returncode}")
        time.sleep(0.01)
    raise AssertionError("Timed out reading MCP message")


def send_message(proc: subprocess.Popen, payload: dict) -> None:
    proc.stdin.write(encode_message(payload))
    proc.stdin.flush()


def request(proc: subprocess.Popen, message_id: int, method: str, params: dict | None = None) -> dict:
    payload = {"jsonrpc": "2.0", "id": message_id, "method": method}
    if params is not None:
        payload["params"] = params
    send_message(proc, payload)

    deadline = time.monotonic() + 15.0
    while time.monotonic() < deadline:
        message = read_message(proc, timeout=max(0.1, deadline - time.monotonic()))
        if message.get("id") == message_id:
            if "error" in message:
                raise AssertionError(f"MCP request {method} failed: {message['error']}")
            return message
    raise AssertionError(f"Timed out waiting for MCP response id={message_id}")


def notify(proc: subprocess.Popen, method: str, params: dict | None = None) -> None:
    payload = {"jsonrpc": "2.0", "method": method}
    if params is not None:
        payload["params"] = params
    send_message(proc, payload)


def main() -> None:
    env = os.environ.copy()
    env["PYTHONUNBUFFERED"] = "1"
    proc = subprocess.Popen(
        [sys.executable, str(SERVER_PATH)],
        cwd=str(ROOT),
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        env=env,
    )

    try:
        init = request(proc, 1, "initialize", {
            "protocolVersion": "2024-11-05",
            "capabilities": {},
            "clientInfo": {"name": "bluecode-stdio-schema-check", "version": "1.0"},
        })
        if "capabilities" not in init.get("result", {}):
            raise AssertionError(f"Unexpected initialize result: {init}")

        notify(proc, "notifications/initialized")
        listed = request(proc, 2, "tools/list")
        tools = {tool["name"]: tool for tool in listed.get("result", {}).get("tools", [])}

        for name in ("bluecode_apply", "bluecode_search_nodes", "bluecode_connect", "bluecode_read_function", "bluecode_compile"):
            if name not in tools:
                raise AssertionError(f"Missing stdio MCP tool: {name}")

        apply_schema = tools["bluecode_apply"].get("inputSchema", {})
        apply_props = apply_schema.get("properties", {})
        for name in ("code", "anchor", "mode", "base_revision", "source_map", "dry_run", "member_classes", "action_handles", "action_hints", "action_hints_by_line", "node_aliases", "aliases", "node_properties"):
            if name not in apply_props:
                raise AssertionError(f"bluecode_apply stdio schema missing {name}")

        search_schema = tools["bluecode_search_nodes"].get("inputSchema", {})
        search_props = search_schema.get("properties", {})
        for name in ("query", "node_class_path", "include_pins", "context_sensitive", "exact"):
            if name not in search_props:
                raise AssertionError(f"bluecode_search_nodes stdio schema missing {name}")
        search_description = tools["bluecode_search_nodes"].get("description", "")
        for text in ("is_exec", "pin_counts", "include_pins"):
            if text not in search_description:
                raise AssertionError(f"bluecode_search_nodes stdio description missing {text}")

        connect_schema = tools["bluecode_connect"].get("inputSchema", {})
        connect_props = connect_schema.get("properties", {})
        for name in ("connects", "source", "target", "aliases"):
            if name not in connect_props:
                raise AssertionError(f"bluecode_connect stdio schema missing {name}")

        description = tools["bluecode_apply"].get("description", "")
        for text in ("node(\"Action Menu Name\"", "value(\"Action Menu Name\"", "union", "base_revision", "source_map", "dry_run", "aliases"):
            if text not in description:
                raise AssertionError(f"bluecode_apply stdio description missing {text}")

        read_description = tools["bluecode_read_function"].get("description", "")
        for text in ("semantic", "roundtrip", "source_map"):
            if text not in read_description:
                raise AssertionError(f"bluecode_read_function stdio description missing {text}")

        print("Blueprint MCP stdio check passed.")
    finally:
        if proc.poll() is None:
            proc.terminate()
            try:
                proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                proc.kill()


if __name__ == "__main__":
    main()
