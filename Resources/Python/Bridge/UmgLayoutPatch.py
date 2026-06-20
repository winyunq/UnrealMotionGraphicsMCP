"""Patch-mode orchestration for apply_constrained_layout (slot-only updates)."""

from __future__ import annotations

from typing import Any, Callable, Dict, List, Type

from Bridge.UmgLayoutCompiler import UmgLayoutCompiler

LAYOUT_DSL_VERSION = "2025-06-20-unified-semantics-v1"


def is_patch_dsl(dsl_node: Dict[str, Any]) -> bool:
    if not isinstance(dsl_node, dict):
        return False
    if dsl_node.get("patch") is True:
        return True
    if str(dsl_node.get("mode", "")).lower() == "patch":
        return True
    if str(dsl_node.get("layout_type", "")).lower() == "patch":
        return True
    return False


async def apply_layout_patch(
    dsl_node: Dict[str, Any],
    *,
    get_tree: Callable[[], Any],
    set_widget_properties: Callable[[str, Dict[str, Any]], Any],
) -> Dict[str, Any]:
    """Resolve parent containers, compile slot patches, apply via set_widget_properties."""
    tree_resp = await get_tree()
    if not tree_resp or tree_resp.get("success") is False:
        return {
            "status": "error",
            "mode": "patch",
            "layout_dsl_version": LAYOUT_DSL_VERSION,
            "error": tree_resp.get("error") if isinstance(tree_resp, dict) else "get_widget_tree failed.",
        }

    parent_map = UmgLayoutCompiler.parse_widget_tree_parents(tree_resp.get("widget_tree", ""))
    patches, lint = UmgLayoutCompiler.compile_patch(dsl_node, parent_map)

    if not lint.ok:
        return {
            "status": "error",
            "mode": "patch",
            "layout_dsl_version": LAYOUT_DSL_VERSION,
            "error": "Layout patch lint failed.",
            "lint": lint.to_dict(),
            "patches_preview": patches,
        }

    if not patches:
        return {
            "status": "error",
            "mode": "patch",
            "layout_dsl_version": LAYOUT_DSL_VERSION,
            "error": "No valid patch entries to apply.",
            "lint": lint.to_dict(),
        }

    applied: List[Dict[str, Any]] = []
    errors: List[str] = []
    for patch in patches:
        widget_name = patch["widget_name"]
        slot_props = patch["slot"]
        result = await set_widget_properties(widget_name, {"Slot": slot_props})
        entry = {
            "widget_name": widget_name,
            "parent_container": patch.get("parent_container"),
            "slot": slot_props,
            "result": result,
        }
        applied.append(entry)
        if not result or result.get("success") is False:
            errors.append(result.get("error") or f"Failed to patch '{widget_name}'.")

    if errors:
        return {
            "status": "error",
            "mode": "patch",
            "layout_dsl_version": LAYOUT_DSL_VERSION,
            "applied": False,
            "error": "; ".join(errors),
            "lint": lint.to_dict(),
            "patches": applied,
        }

    return {
        "status": "success",
        "mode": "patch",
        "layout_dsl_version": LAYOUT_DSL_VERSION,
        "applied": True,
        "lint": lint.to_dict(),
        "patches": applied,
    }
