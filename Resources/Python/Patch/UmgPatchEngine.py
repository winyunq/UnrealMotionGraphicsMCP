# UmgPatchEngine.py — JSON Patch orchestration (preview in Python, apply via C++ transaction)

from typing import Dict, Any, List, Optional


def parse_patch_path(path: str, op_type: str = "") -> Dict[str, Any]:
    """Parse patch paths with the same rules as UUmgPatchSubsystem::ParsePatchPath."""
    normalized = (path or "").strip()
    if not normalized.startswith("/widgets/"):
        return {"valid": False, "error": f"Invalid patch path (must start with /widgets/): {path}"}

    segments = [segment for segment in normalized[len("/widgets/") :].split("/") if segment]
    if not segments:
        return {"valid": False, "error": "Missing widget name in patch path."}

    info: Dict[str, Any] = {
        "valid": True,
        "widget_name": segments[0],
        "property_path": "",
        "parent_name": segments[0],
        "is_child_append": False,
    }

    op = (op_type or "").strip().lower()

    if op == "remove":
        if len(segments) != 1:
            return {
                "valid": False,
                "error": "remove op path must be exactly /widgets/{WidgetName} (elimination only).",
            }
        return info

    if len(segments) == 1:
        if op == "set":
            return {"valid": False, "error": "set op path must include /properties/{PropertyName}."}
        if op:
            return {"valid": False, "error": f"Unsupported single-segment path for op '{op_type}'."}
        return info

    if segments[1] == "properties":
        if op and op != "set":
            return {"valid": False, "error": f"properties path is only valid for set op, not '{op_type}'."}
        if len(segments) < 3:
            return {"valid": False, "error": "set op path must include a property name after /properties/."}
        info["property_path"] = ".".join(segments[2:])
        return info

    if segments[1] == "children":
        if op and op != "add":
            return {"valid": False, "error": f"children path is only valid for add op, not '{op_type}'."}
        if len(segments) >= 3 and segments[2] == "-":
            info["is_child_append"] = True
            return info
        return {"valid": False, "error": "add op path must be /widgets/{Parent}/children/-."}

    return {"valid": False, "error": f"Unsupported path segments in: {path}"}


def describe_op(op: Dict[str, Any]) -> Dict[str, Any]:
    op_type = (op.get("op") or "").strip().lower()
    path = op.get("path", "")
    parsed = parse_patch_path(path, op_type)
    summary = {
        "op": op_type,
        "path": path,
        "valid": parsed.get("valid", False),
    }

    if not parsed.get("valid"):
        summary["error"] = parsed.get("error")
        return summary

    if op_type == "set":
        summary["action"] = "set_widget_properties"
        summary["widget_name"] = parsed["widget_name"]
        summary["property"] = parsed.get("property_path")
        summary["value"] = op.get("value")
        if not summary["property"]:
            summary["valid"] = False
            summary["error"] = "set op path must include /properties/{PropertyName}."
    elif op_type == "add":
        summary["action"] = "create_widget"
        summary["parent_name"] = parsed["widget_name"]
        value = op.get("value") or {}
        summary["widget_type"] = value.get("type") or value.get("widget_type") or value.get("widget_class")
        summary["new_widget_name"] = value.get("name") or value.get("widget_name")
        if not summary["widget_type"] or not summary["new_widget_name"]:
            summary["valid"] = False
            summary["error"] = "add op value must include type and name."
    elif op_type == "remove":
        summary["action"] = "eliminate_widget"
        summary["widget_name"] = parsed["widget_name"]
    else:
        summary["valid"] = False
        summary["error"] = f"Unsupported op: {op_type or op.get('op', '')}"

    return summary


def patch_preview(patch: List[Dict[str, Any]], revision: Optional[int] = None) -> Dict[str, Any]:
    planned = [describe_op(op) for op in (patch or [])]
    invalid = [item for item in planned if not item.get("valid")]

    return {
        "status": "ok" if not invalid else "invalid",
        "revision": revision,
        "op_count": len(planned),
        "planned_ops": planned,
        "warnings": [item.get("error") for item in invalid if item.get("error")],
    }


class UmgPatchEngine:
    def __init__(self, client):
        self.client = client

    def get_patch_revision(self) -> Dict[str, Any]:
        return self.client.send_command("get_patch_revision", {})

    def patch_preview(self, patch: List[Dict[str, Any]]) -> Dict[str, Any]:
        revision_resp = self.get_patch_revision()
        revision = None
        if isinstance(revision_resp, dict):
            revision = revision_resp.get("revision")
            if revision is None and isinstance(revision_resp.get("result"), dict):
                revision = revision_resp["result"].get("revision")

        preview = patch_preview(patch, revision=revision)
        preview["success"] = preview["status"] == "ok"
        return preview

    def patch_apply(
        self,
        patch: List[Dict[str, Any]],
        expected_revision: Optional[int] = None,
    ) -> Dict[str, Any]:
        params: Dict[str, Any] = {"patch": patch}
        if expected_revision is not None:
            params["expected_revision"] = expected_revision
        return self.client.send_command("patch_apply", params)
