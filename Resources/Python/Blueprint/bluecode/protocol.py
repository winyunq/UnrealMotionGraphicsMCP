import re
from typing import Any, Dict, List, Optional, Set, Tuple


class BluecodeProtocol:
    """
    Bluecode: append-first blueprint protocol (parallel to legacy blueprint APIs).
    """

    TERMINATORS = {"end", "return"}

    @staticmethod
    async def set_function(connection, function_name: str) -> Dict[str, Any]:
        return await connection.send_command("set_edit_function", {"function_name": function_name})

    @staticmethod
    async def read_function(connection, include_connect_list: bool = False) -> Dict[str, Any]:
        raw = await connection.send_command(
            "manage_blueprint_graph",
            {"subAction": "get_nodes", "includeConnectList": include_connect_list},
        )
        if raw.get("status") == "error" or raw.get("success") is False:
            return raw

        nodes = raw.get("nodes", [])
        node_map: Dict[str, Dict[str, Any]] = {n.get("id", ""): n for n in nodes if n.get("id")}
        incoming_exec: Dict[str, int] = {}
        for node in nodes:
            for edge in node.get("exec_outputs", []):
                to_id = edge.get("to_id")
                if to_id:
                    incoming_exec[to_id] = incoming_exec.get(to_id, 0) + 1

        start_id = BluecodeProtocol._pick_start_node(nodes, incoming_exec)
        main: List[str] = []
        branches: Dict[str, List[str]] = {}
        visited_main: Set[str] = set()

        current_id = start_id
        step_guard = 0
        while current_id and step_guard < 128 and current_id in node_map:
            step_guard += 1
            if current_id in visited_main:
                main.append("return")
                break
            visited_main.add(current_id)

            node = node_map[current_id]
            main.append(BluecodeProtocol._node_token(node))
            exec_edges = node.get("exec_outputs", [])

            if not exec_edges:
                main.append("end")
                break

            if len(exec_edges) == 1:
                current_id = exec_edges[0].get("to_id", "")
                continue

            # Multi-branch/loop node
            branch_anchor = current_id
            for edge in exec_edges:
                pin_name = edge.get("from_pin", "next")
                branch_key = f"{branch_anchor}.{pin_name}"
                branch_start = edge.get("to_id", "")
                branch_seq = BluecodeProtocol._walk_branch(branch_start, node_map, visited_main, {current_id})
                branches[branch_key] = branch_seq

            # continue main from first branch edge
            current_id = exec_edges[0].get("to_id", "")

        result: Dict[str, Any] = {
            "success": True,
            "protocol": "bluecode",
            "main": main if main else ["main", "end"],
            "branches": branches,
            "floating_nodes": BluecodeProtocol._find_floating_nodes(nodes, incoming_exec, visited_main),
            "notes": [
                "connect_list 默认为关闭；设置 include_connect_list=true 时返回底层连线。",
                "main/branches 的字符串数组顺序即执行连接顺序。",
                "end 表示连空；return 表示回到主路径下一节点。",
            ],
        }
        if include_connect_list:
            result["connect_list"] = raw.get("connect_list", [])
        return result

    @staticmethod
    async def write_function(connection, bluecode: Dict[str, Any], append_to_end: bool = True) -> Dict[str, Any]:
        """
        Append-first writer: never deletes nodes, only appends/relinks best-effort.
        """
        if not isinstance(bluecode, dict):
            return {"success": False, "error": "bluecode must be an object"}

        main = bluecode.get("main", [])
        if not isinstance(main, list):
            return {"success": False, "error": "bluecode.main must be a string array"}

        existing = await connection.send_command("manage_blueprint_graph", {"subAction": "get_nodes"})
        existing_titles = {
            BluecodeProtocol._normalize_name(n.get("title", "") or n.get("name", ""))
            for n in existing.get("nodes", [])
        }

        created: List[str] = []
        reused: List[str] = []
        warnings: List[str] = []
        anchor_node_id: Optional[str] = None

        # main chain
        for token in main:
            parsed = BluecodeProtocol._parse_token(token)
            if not parsed:
                continue
            if parsed in BluecodeProtocol.TERMINATORS or parsed == "main":
                continue

            normalized = BluecodeProtocol._normalize_name(parsed)
            if normalized in existing_titles:
                reused.append(parsed)
                continue

            payload: Dict[str, Any] = {"subAction": "add_function_step", "nodeName": parsed}
            if not append_to_end and anchor_node_id:
                payload["autoConnectToNodeId"] = anchor_node_id
            response = await connection.send_command("manage_blueprint_graph", payload)
            if response.get("success"):
                created.append(parsed)
                anchor_node_id = response.get("nodeId", anchor_node_id)
            else:
                warnings.append(f"main token create failed: {parsed}")

        # branch chains
        branch_map = bluecode.get("branches", {})
        if isinstance(branch_map, dict):
            for branch_key, sequence in branch_map.items():
                if not isinstance(sequence, list):
                    continue
                branch_anchor, branch_pin = BluecodeProtocol._parse_branch_key(branch_key)
                first_node_id: Optional[str] = None
                previous_node_id: Optional[str] = None

                for token in sequence:
                    parsed = BluecodeProtocol._parse_token(token)
                    if not parsed or parsed in BluecodeProtocol.TERMINATORS or parsed == "main":
                        continue

                    normalized = BluecodeProtocol._normalize_name(parsed)
                    if normalized in existing_titles:
                        reused.append(parsed)
                        continue

                    payload = {"subAction": "create_node", "nodeName": parsed}
                    if previous_node_id:
                        payload["autoConnectToNodeId"] = previous_node_id

                    node_res = await connection.send_command("manage_blueprint_graph", payload)
                    if not node_res.get("success"):
                        warnings.append(f"branch token create failed: {parsed}")
                        continue

                    created.append(parsed)
                    previous_node_id = node_res.get("nodeId", previous_node_id)
                    if not first_node_id:
                        first_node_id = previous_node_id

                if branch_anchor and branch_pin and first_node_id:
                    conn_res = await connection.send_command(
                        "manage_blueprint_graph",
                        {
                            "subAction": "connect_pins",
                            "nodeIdA": branch_anchor,
                            "pinNameA": branch_pin,
                            "nodeIdB": first_node_id,
                            "pinNameB": "Execute",
                        },
                    )
                    if not conn_res.get("success"):
                        warnings.append(f"branch connect failed: {branch_key}")

        return {
            "success": True,
            "protocol": "bluecode",
            "created_nodes": created,
            "reused_nodes": reused,
            "warnings": warnings,
            "policy": "append_only_no_delete",
        }

    @staticmethod
    async def compile(connection) -> Dict[str, Any]:
        return await connection.send_command("compile_blueprint", {})

    @staticmethod
    def demo_cases() -> Dict[str, Any]:
        return {
            "protocol": "bluecode",
            "rules": {
                "main": "字符串数组，顺序就是链接顺序",
                "branches": "对象，value 为字符串数组；key 格式 anchor.pin",
                "end": "连空",
                "return": "连接到主路径下一节点",
                "connect_list_default": "默认不使用不返回，read 时可请求 include_connect_list=true",
            },
            "cases": [
                {
                    "name": "if 双分支",
                    "bluecode": {
                        "main": ["main", "Branch(IsEven(i))", "PrintString(\"AfterIf\")", "end"],
                        "branches": {
                            "A1.Then": ["PrintString(\"hi\")", "return"],
                            "A1.Else": ["PrintString(\"fail\")", "end"],
                        },
                    },
                },
                {
                    "name": "循环 + 多分支",
                    "bluecode": {
                        "main": ["main", "ForLoop(0, Count)", "PrintString(\"Done\")", "return"],
                        "branches": {
                            "B3.LoopBody": ["Branch(IsBoss(index))", "return"],
                            "B3.Completed": ["PrintString(\"Completed\")", "end"],
                            "C9.Then": ["PrintString(\"Boss\")", "return"],
                            "C9.Else": ["PrintString(\"Minion\")", "return"],
                        },
                        "floating_nodes": ["MakeLiteralString(\"debug\")"],
                    },
                },
            ],
        }

    @staticmethod
    def _pick_start_node(nodes: List[Dict[str, Any]], incoming_exec: Dict[str, int]) -> str:
        for node in nodes:
            klass = node.get("class", "")
            if "FunctionEntry" in klass or "CustomEvent" in klass or "K2Node_Event" in klass:
                return node.get("id", "")
        for node in nodes:
            node_id = node.get("id", "")
            if node_id and incoming_exec.get(node_id, 0) == 0 and node.get("isExec"):
                return node_id
        return nodes[0].get("id", "") if nodes else ""

    @staticmethod
    def _walk_branch(
        start_id: str,
        node_map: Dict[str, Dict[str, Any]],
        main_visited: Set[str],
        local_visited: Set[str],
    ) -> List[str]:
        branch: List[str] = []
        node_id = start_id
        guard = 0
        while node_id and guard < 64 and node_id in node_map:
            guard += 1
            if node_id in local_visited:
                branch.append("return")
                break
            local_visited.add(node_id)

            node = node_map[node_id]
            branch.append(BluecodeProtocol._node_token(node))
            if node_id in main_visited:
                branch.append("return")
                break

            edges = node.get("exec_outputs", [])
            if not edges:
                branch.append("end")
                break
            if len(edges) > 1:
                branch.append("return")
                break
            node_id = edges[0].get("to_id", "")

        if not branch:
            return ["end"]
        return branch

    @staticmethod
    def _find_floating_nodes(
        nodes: List[Dict[str, Any]],
        incoming_exec: Dict[str, int],
        main_visited: Set[str],
    ) -> List[str]:
        floating: List[str] = []
        for node in nodes:
            node_id = node.get("id", "")
            if not node_id or node_id in main_visited:
                continue
            if incoming_exec.get(node_id, 0) == 0 and not node.get("exec_outputs"):
                floating.append(BluecodeProtocol._node_token(node))
        return floating

    @staticmethod
    def _short_id(node_id: str) -> str:
        return node_id[:8] if node_id else ""

    @staticmethod
    def _node_token(node: Dict[str, Any]) -> str:
        title = node.get("title") or node.get("name") or "Node"
        return f"{title}#{BluecodeProtocol._short_id(node.get('id', ''))}"

    @staticmethod
    def _normalize_name(token: str) -> str:
        token = BluecodeProtocol._parse_token(token)
        token = re.sub(r"\(.*\)$", "", token).strip()
        return token.lower()

    @staticmethod
    def _parse_token(token: Any) -> str:
        if not isinstance(token, str):
            return ""
        value = token.strip()
        if not value:
            return ""
        if ":" in value:
            _, rhs = value.split(":", 1)
            value = rhs.strip()
        value = value.split("#", 1)[0].strip()
        return value

    @staticmethod
    def _parse_branch_key(key: str) -> Tuple[str, str]:
        if not isinstance(key, str) or "." not in key:
            return "", ""
        anchor, pin = key.split(".", 1)
        pin_map = {
            "true": "Then",
            "false": "Else",
            "then": "Then",
            "else": "Else",
        }
        return anchor.strip(), pin_map.get(pin.strip().lower(), pin.strip())
