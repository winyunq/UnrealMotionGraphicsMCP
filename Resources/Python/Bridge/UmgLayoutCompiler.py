"""
UmgLayoutCompiler — CSS-like Layout DSL → UMG apply_layout JSON

Compiles high-level flex/grid/absolute layout constraints into the verbose
widget tree JSON consumed by UpsertWidgetFromJsonAppendOnly.

Unified layout semantics (AI-facing, no raw Slot JSON):
  size: "fill" | "auto" | "100%"     → Box Fill/Auto or Canvas stretch
  fill_weight / flex_grow             → Fill weight (default 1)
  h_align / v_align                   → Left|Center|Right|Fill / Top|Center|Bottom|Fill
  align_items / justify_content       → container cross/main axis (flex)
  width / height: "100%"              → size fill on box children
  position                            → canvas preset (absolute containers only)

Patch mode (partial screen updates):
  {"patch": true, "updates": [{"name": "WidgetA", "size": "fill", ...}]}
  or single widget: {"patch": true, "name": "WidgetA", "size": "fill"}
"""

from __future__ import annotations

import logging
import re
from typing import Any, Dict, List, Optional, Set, Tuple

logger = logging.getLogger("UmgLayoutCompiler")

LAYOUT_DSL_VERSION = "2025-06-20-unified-semantics-v1"

# Widgets that accept exactly one child in UMG
SINGLE_CHILD_PANELS = frozenset({
    "Button", "Border", "SizeBox", "ScaleBox",
    "RetainerBox", "InvalidationBox", "CheckBox",
})

# Panels that accept multiple children
MULTI_CHILD_PANELS = frozenset({
    "CanvasPanel", "VerticalBox", "HorizontalBox", "GridPanel",
    "Overlay", "WrapBox", "ScrollBox", "UniformGridPanel",
    "WidgetSwitcher", "SafeZone",
})

SLOT_SCHEMA_BY_PARENT: Dict[str, frozenset] = {
    "VerticalBox": frozenset({"Size", "Padding", "HorizontalAlignment", "VerticalAlignment"}),
    "HorizontalBox": frozenset({"Size", "Padding", "HorizontalAlignment", "VerticalAlignment"}),
    "CanvasPanel": frozenset({"LayoutData", "Alignment", "AutoSize", "ZOrder"}),
    "GridPanel": frozenset({"Row", "Column", "RowSpan", "ColumnSpan", "HorizontalAlignment", "VerticalAlignment", "Padding"}),
    "Overlay": frozenset({"HorizontalAlignment", "VerticalAlignment", "Padding"}),
    "ScrollBox": frozenset({"Padding", "HorizontalAlignment", "VerticalAlignment"}),
    "WrapBox": frozenset({"Padding", "FillEmptySpace", "HorizontalAlignment", "VerticalAlignment"}),
}

CANVAS_ONLY_SLOT_KEYS = frozenset({"Anchors", "LayoutData", "Alignment", "Position", "AutoSize", "ZOrder"})

SIZE_SEMANTIC_KEYS = frozenset({"size", "fill_weight", "flex_grow", "flex", "width", "height"})
ALIGN_SEMANTIC_KEYS = frozenset({"h_align", "v_align", "align_items", "align", "justify_content", "justify"})
CANVAS_SEMANTIC_KEYS = frozenset({"position", "anchors", "offsets", "margin", "alignment"})

WIDGET_CLASS_MAP: Dict[str, str] = {
    "Text": "/Script/UMG.TextBlock",
    "TextBlock": "/Script/UMG.TextBlock",
    "Image": "/Script/UMG.Image",
    "Button": "/Script/UMG.Button",
    "Border": "/Script/UMG.Border",
    "CanvasPanel": "/Script/UMG.CanvasPanel",
    "VerticalBox": "/Script/UMG.VerticalBox",
    "HorizontalBox": "/Script/UMG.HorizontalBox",
    "Overlay": "/Script/UMG.Overlay",
    "GridPanel": "/Script/UMG.GridPanel",
    "UniformGridPanel": "/Script/UMG.UniformGridPanel",
    "WrapBox": "/Script/UMG.WrapBox",
    "ScrollBox": "/Script/UMG.ScrollBox",
    "SizeBox": "/Script/UMG.SizeBox",
    "ScaleBox": "/Script/UMG.ScaleBox",
    "Spacer": "/Script/UMG.Spacer",
    "ProgressBar": "/Script/UMG.ProgressBar",
    "EditableTextBox": "/Script/UMG.EditableTextBox",
    "Slider": "/Script/UMG.Slider",
    "CheckBox": "/Script/UMG.CheckBox",
    "WidgetSwitcher": "/Script/UMG.WidgetSwitcher",
    "SafeZone": "/Script/UMG.SafeZone",
}

# CSS align → UE box-slot alignment (direction-aware)
FLEX_ALIGN_MAP = {
    "start": ("Left", "Top"),
    "center": ("Center", "Center"),
    "end": ("Right", "Bottom"),
    "stretch": ("Fill", "Fill"),
    "fill": ("Fill", "Fill"),
}

UE_H_ALIGN = {
    "left": "HAlign_Left", "center": "HAlign_Center", "right": "HAlign_Right",
    "fill": "HAlign_Fill", "stretch": "HAlign_Fill", "start": "HAlign_Left", "end": "HAlign_Right",
}

UE_V_ALIGN = {
    "top": "VAlign_Top", "center": "VAlign_Center", "bottom": "VAlign_Bottom",
    "fill": "VAlign_Fill", "stretch": "VAlign_Fill", "start": "VAlign_Top", "end": "VAlign_Bottom",
}

# Named absolute positions → (anchor_min, anchor_max, alignment)
POSITION_PRESETS: Dict[str, Tuple[List[float], List[float], List[float]]] = {
    "center": ([0.5, 0.5], [0.5, 0.5], [0.5, 0.5]),
    "top-left": ([0.0, 0.0], [0.0, 0.0], [0.0, 0.0]),
    "top-center": ([0.5, 0.0], [0.5, 0.0], [0.5, 0.0]),
    "top-right": ([1.0, 0.0], [1.0, 0.0], [1.0, 0.0]),
    "bottom-left": ([0.0, 1.0], [0.0, 1.0], [0.0, 1.0]),
    "bottom-center": ([0.5, 1.0], [0.5, 1.0], [0.5, 1.0]),
    "bottom-right": ([1.0, 1.0], [1.0, 1.0], [1.0, 1.0]),
    "left-center": ([0.0, 0.5], [0.0, 0.5], [0.0, 0.5]),
    "right-center": ([1.0, 0.5], [1.0, 0.5], [1.0, 0.5]),
    "fill": ([0.0, 0.0], [1.0, 1.0], [0.5, 0.5]),
    "stretch": ([0.0, 0.0], [1.0, 1.0], [0.5, 0.5]),
}

_TREE_LINE_RE = re.compile(r"^(\s*)-?\s*(\w+)\s+\[(\w+)\]\s*$")


class LayoutLint:
    """Collects lint warnings and blocking errors during compilation."""

    def __init__(self) -> None:
        self.warnings: List[str] = []
        self.errors: List[str] = []
        self.issues: List[Dict[str, Any]] = []

    def _append_issue(
        self,
        severity: str,
        message: str,
        rule_id: str,
        widget_path: Optional[str] = None,
        fix_suggestion: Optional[Dict[str, Any]] = None,
    ) -> None:
        issue: Dict[str, Any] = {
            "ruleId": rule_id,
            "severity": severity,
            "message": message,
            "widgetPath": widget_path or "",
            "source": "compile-time",
        }
        if fix_suggestion:
            issue["fixSuggestion"] = fix_suggestion
        self.issues.append(issue)

    def warn(
        self,
        message: str,
        *,
        rule_id: str = "layout-warning",
        widget_path: Optional[str] = None,
    ) -> None:
        self.warnings.append(message)
        self._append_issue("warning", message, rule_id, widget_path)
        logger.warning("Layout lint: %s", message)

    def error(
        self,
        message: str,
        *,
        rule_id: str = "layout-error",
        widget_path: Optional[str] = None,
    ) -> None:
        self.errors.append(message)
        self._append_issue("error", message, rule_id, widget_path)
        logger.error("Layout lint: %s", message)

    @property
    def ok(self) -> bool:
        return len(self.errors) == 0

    def to_dict(self) -> Dict[str, Any]:
        error_count = sum(1 for i in self.issues if i.get("severity") == "error")
        warning_count = sum(1 for i in self.issues if i.get("severity") == "warning")
        info_count = sum(1 for i in self.issues if i.get("severity") == "info")
        return {
            "ok": self.ok,
            "issues": self.issues,
            "warnings": self.warnings,
            "errors": self.errors,
            "summary": {
                "error": error_count,
                "warning": warning_count,
                "info": info_count,
            },
        }


class UmgLayoutCompiler:
    """Compiles CSS-like layout DSL nodes into UMG apply_layout JSON."""

    _counter = 0

    @classmethod
    def is_patch_dsl(cls, dsl_node: Dict[str, Any]) -> bool:
        from Bridge.UmgLayoutPatch import is_patch_dsl

        return is_patch_dsl(dsl_node)

    @classmethod
    def compile(cls, dsl_node: Dict[str, Any], lint: Optional[LayoutLint] = None) -> Tuple[Dict[str, Any], LayoutLint]:
        lint = lint or LayoutLint()
        if cls.is_patch_dsl(dsl_node):
            lint.error(
                "Patch DSL cannot be compiled as a full layout tree. "
                "Use apply_constrained_layout with \"patch\": true (or mode/layout_type: \"patch\").",
                rule_id="patch-misrouted",
            )
            return {}, lint
        return cls._compile_node(dsl_node, parent_container=None, lint=lint), lint

    @classmethod
    def compile_patch(
        cls,
        dsl_node: Dict[str, Any],
        parent_map: Dict[str, str],
        lint: Optional[LayoutLint] = None,
    ) -> Tuple[List[Dict[str, Any]], LayoutLint]:
        """Compile slot-only patches for existing widgets (by name)."""
        lint = lint or LayoutLint()
        updates = cls._normalize_patch_updates(dsl_node, lint)
        patches: List[Dict[str, Any]] = []

        for update in updates:
            name = update.get("name") or update.get("widget_name")
            if not name:
                lint.error("Patch entry missing 'name' or 'widget_name'.", rule_id="patch-missing-name")
                continue

            parent = parent_map.get(str(name))
            if not parent:
                lint.warn(
                    f"Widget '{name}': parent type unknown; assuming VerticalBox slot semantics.",
                    rule_id="patch-unknown-parent",
                    widget_path=str(name),
                )
                parent = "VerticalBox"

            slot = cls.compile_slot_for_widget(update, parent, lint, widget_name=str(name), partial=True)
            if slot:
                patches.append({"widget_name": str(name), "parent_container": parent, "slot": slot})

        return patches, lint

    @classmethod
    def parse_widget_tree_parents(cls, tree_text: str) -> Dict[str, str]:
        """Parse get_widget_tree text → {child_name: parent_panel_class}."""
        parent_map: Dict[str, str] = {}
        stack: List[Tuple[int, str, str]] = []

        for line in tree_text.splitlines():
            if "[WidgetBlueprint]" in line:
                continue
            match = _TREE_LINE_RE.match(line.rstrip())
            if not match:
                continue

            indent, name, widget_class = match.group(1), match.group(2), match.group(3)
            depth = len(indent) // 2
            if depth == 0 and not indent:
                depth = 1

            while stack and stack[-1][0] >= depth:
                stack.pop()

            if stack:
                parent_map[name] = stack[-1][2]

            stack.append((depth, name, widget_class))

        return parent_map

    @classmethod
    def _normalize_patch_updates(cls, dsl_node: Dict[str, Any], lint: LayoutLint) -> List[Dict[str, Any]]:
        raw_updates = dsl_node.get("updates")
        if isinstance(raw_updates, list) and raw_updates:
            return [u for u in raw_updates if isinstance(u, dict)]

        single = {k: v for k, v in dsl_node.items() if k not in ("patch", "updates", "mode")}
        if single.get("name") or single.get("widget_name"):
            return [single]

        lint.error("Patch DSL requires 'updates' array or a single 'name' entry.", rule_id="patch-empty")
        return []

    @classmethod
    def _next_name(cls, prefix: str) -> str:
        cls._counter += 1
        return f"{prefix}_{cls._counter}"

    @classmethod
    def _resolve_widget_class(cls, node: Dict[str, Any]) -> Tuple[str, str]:
        """Returns (short_name, full_class_path)."""
        if "widget_class" in node:
            full = node["widget_class"]
            short = full.rsplit(".", 1)[-1]
            return short, full
        widget_type = node.get("type") or node.get("widget_type") or "VerticalBox"
        full = WIDGET_CLASS_MAP.get(widget_type, f"/Script/UMG.{widget_type}")
        short = widget_type
        return short, full

    @classmethod
    def _to_ue_box_h_align(cls, value: str) -> str:
        if value.startswith("HAlign_"):
            return value
        mapped = UE_H_ALIGN.get(str(value).strip().lower())
        return mapped or f"HAlign_{value}"

    @classmethod
    def _to_ue_box_v_align(cls, value: str) -> str:
        if value.startswith("VAlign_"):
            return value
        mapped = UE_V_ALIGN.get(str(value).strip().lower())
        return mapped or f"VAlign_{value}"

    @classmethod
    def _parse_ue_align(cls, value: Any, axis: str) -> Optional[str]:
        if value is None:
            return None
        key = str(value).strip().lower().replace("_", "-")
        mapping = UE_H_ALIGN if axis == "h" else UE_V_ALIGN
        return mapping.get(key)

    @classmethod
    def _resolve_size_rule(cls, dsl_node: Dict[str, Any]) -> Tuple[bool, float]:
        """Returns (is_fill, fill_weight)."""
        size = dsl_node.get("size")
        if isinstance(size, str):
            normalized = size.strip().lower()
            if normalized in ("fill", "100%", "stretch"):
                weight = float(
                    dsl_node.get("fill_weight", dsl_node.get("flex_grow", dsl_node.get("flex", 1))) or 1
                )
                return True, weight
            if normalized in ("auto", "content", "fit-content"):
                return False, 1.0

        for key in ("width", "height"):
            dim = dsl_node.get(key)
            if isinstance(dim, str) and dim.strip().lower() in ("100%", "fill", "stretch"):
                weight = float(
                    dsl_node.get("fill_weight", dsl_node.get("flex_grow", dsl_node.get("flex", 1))) or 1
                )
                return True, weight

        flex_grow = dsl_node.get("flex_grow", dsl_node.get("flex"))
        if flex_grow is not None:
            grow = float(flex_grow)
            return grow > 0, grow if grow > 0 else 1.0

        return False, 1.0

    @classmethod
    def _semantic_keys_present(cls, dsl_node: Dict[str, Any], keys: Set[str]) -> bool:
        return any(k in dsl_node for k in keys)

    @classmethod
    def _align_to_canvas_position(cls, h_ue: str, v_ue: str) -> str:
        if h_ue == "Fill" or v_ue == "Fill":
            return "fill"

        h_part = {"Left": "left", "Center": "center", "Right": "right"}.get(h_ue, "left")
        v_part = {"Top": "top", "Center": "center", "Bottom": "bottom"}.get(v_ue, "top")

        if h_part == "center" and v_part == "center":
            return "center"
        if v_part == "center":
            return f"{h_part}-center"
        if h_part == "center":
            return f"{v_part}-center"
        return f"{v_part}-{h_part}"

    @classmethod
    def compile_slot_for_widget(
        cls,
        dsl_node: Dict[str, Any],
        parent_container: str,
        lint: LayoutLint,
        *,
        widget_name: Optional[str] = None,
        partial: bool = False,
        container_align: str = "stretch",
        container_justify: str = "stretch",
        is_column: bool = True,
    ) -> Dict[str, Any]:
        """Compile unified layout semantics to Slot dict for a given parent container type."""
        name = widget_name or dsl_node.get("name") or dsl_node.get("widget_name") or "?"

        if parent_container in ("VerticalBox", "HorizontalBox"):
            return cls._compile_box_slot_semantics(
                dsl_node,
                parent_container,
                lint,
                name,
                container_align=container_align,
                container_justify=container_justify,
                is_column=(parent_container == "VerticalBox") if parent_container else is_column,
                partial=partial,
            )

        if parent_container == "CanvasPanel":
            if partial and not cls._semantic_keys_present(dsl_node, SIZE_SEMANTIC_KEYS | ALIGN_SEMANTIC_KEYS | CANVAS_SEMANTIC_KEYS):
                return {}
            return cls._compile_canvas_slot(dsl_node, lint)

        if parent_container == "GridPanel":
            slot: Dict[str, Any] = {}
            if not partial or cls._semantic_keys_present(dsl_node, ALIGN_SEMANTIC_KEYS):
                h = cls._parse_ue_align(dsl_node.get("h_align"), "h")
                v = cls._parse_ue_align(dsl_node.get("v_align"), "v")
                if h:
                    slot["HorizontalAlignment"] = h
                if v:
                    slot["VerticalAlignment"] = v
            return cls._strip_incompatible_slot_keys(slot, parent_container, lint, name)

        if parent_container in ("Overlay", "ScrollBox", "WrapBox"):
            slot = {}
            h = cls._parse_ue_align(dsl_node.get("h_align"), "h")
            v = cls._parse_ue_align(dsl_node.get("v_align"), "v")
            if h:
                slot["HorizontalAlignment"] = h
            if v:
                slot["VerticalAlignment"] = v
            return cls._strip_incompatible_slot_keys(slot, parent_container, lint, name)

        return cls._compile_box_slot_semantics(
            dsl_node, "VerticalBox", lint, name, partial=partial,
        )

    @classmethod
    def _compile_box_slot_semantics(
        cls,
        dsl_node: Dict[str, Any],
        parent_container: str,
        lint: LayoutLint,
        widget_name: str,
        *,
        container_align: str = "stretch",
        container_justify: str = "stretch",
        is_column: bool = True,
        partial: bool = False,
    ) -> Dict[str, Any]:
        slot: Dict[str, Any] = {}

        has_size = cls._semantic_keys_present(dsl_node, SIZE_SEMANTIC_KEYS)
        if has_size or not partial:
            is_fill, weight = cls._resolve_size_rule(dsl_node)
            if not has_size and not partial:
                flex_grow = float(dsl_node.get("flex_grow", dsl_node.get("flex", 0)) or 0)
                is_fill = flex_grow > 0
                weight = flex_grow if is_fill else 1.0
            slot["Size"] = {
                "SizeRule": "Fill" if is_fill else "Automatic",
                "Value": weight if is_fill else 1.0,
            }

        has_align = cls._semantic_keys_present(dsl_node, ALIGN_SEMANTIC_KEYS)
        if has_align or not partial:
            h_explicit = cls._parse_ue_align(dsl_node.get("h_align"), "h")
            v_explicit = cls._parse_ue_align(dsl_node.get("v_align"), "v")

            align_items = str(dsl_node.get("align_items", dsl_node.get("align", container_align))).lower()
            justify_content = str(
                dsl_node.get("justify_content", dsl_node.get("justify", container_justify))
            ).lower()

            cross_h, cross_v = FLEX_ALIGN_MAP.get(align_items, ("Fill", "Fill"))
            main_h, main_v = FLEX_ALIGN_MAP.get(justify_content, (cross_h, cross_v))

            if is_column:
                slot["HorizontalAlignment"] = h_explicit or cls._to_ue_box_h_align(cross_h)
                slot["VerticalAlignment"] = v_explicit or cls._to_ue_box_v_align(main_v)
            else:
                slot["HorizontalAlignment"] = h_explicit or cls._to_ue_box_h_align(main_h)
                slot["VerticalAlignment"] = v_explicit or cls._to_ue_box_v_align(cross_v)

        return cls._strip_incompatible_slot_keys(slot, parent_container, lint, widget_name)

    @classmethod
    def _compile_node(
        cls,
        dsl_node: Dict[str, Any],
        parent_container: Optional[str],
        lint: LayoutLint,
    ) -> Dict[str, Any]:
        layout_type = dsl_node.get("layout_type") or dsl_node.get("display")

        if layout_type is None and (dsl_node.get("type") or dsl_node.get("widget_class")):
            return cls._compile_leaf(dsl_node, parent_container, lint)

        if layout_type in (None, "flex"):
            return cls._compile_flex(dsl_node, parent_container, lint)
        if layout_type == "grid":
            return cls._compile_grid(dsl_node, parent_container, lint)
        if layout_type in ("absolute", "canvas"):
            return cls._compile_absolute(dsl_node, parent_container, lint)

        lint.error(f"Unknown layout_type '{layout_type}'. Use flex, grid, or absolute.", rule_id="unknown-layout-type")
        return cls._compile_flex(dsl_node, parent_container, lint)

    @classmethod
    def _compile_flex(cls, dsl_node: Dict[str, Any], parent_container: Optional[str], lint: LayoutLint) -> Dict[str, Any]:
        direction = dsl_node.get("direction", "column")
        is_column = direction in ("column", "col", "vertical")
        container_short = "VerticalBox" if is_column else "HorizontalBox"
        container_class = WIDGET_CLASS_MAP[container_short]

        name = dsl_node.get("name") or dsl_node.get("widget_name") or cls._next_name("FlexBox")
        gap = float(dsl_node.get("gap", 0) or 0)
        align_items = str(dsl_node.get("align_items", dsl_node.get("align", "stretch"))).lower()
        justify_content = str(dsl_node.get("justify_content", dsl_node.get("justify", align_items))).lower()

        children_dsl = cls._normalize_children(dsl_node, container_short, lint)
        compiled_children: List[Dict[str, Any]] = []

        for idx, child_dsl in enumerate(children_dsl):
            child_json = cls._compile_child_under_flex(
                child_dsl,
                container_short,
                is_column,
                gap,
                align_items,
                justify_content,
                is_last=(idx == len(children_dsl) - 1),
                lint=lint,
            )
            compiled_children.append(child_json)

        result: Dict[str, Any] = {
            "widget_name": name,
            "widget_class": container_class,
            "children": compiled_children,
        }

        widget_props = dict(dsl_node.get("properties") or {})
        parent_slot = cls._compile_parent_slot(dsl_node, parent_container, lint)
        if parent_slot:
            widget_props["Slot"] = {**widget_props.get("Slot", {}), **parent_slot}
        if widget_props:
            result["properties"] = widget_props

        return result

    @classmethod
    def _compile_grid(cls, dsl_node: Dict[str, Any], parent_container: Optional[str], lint: LayoutLint) -> Dict[str, Any]:
        name = dsl_node.get("name") or dsl_node.get("widget_name") or cls._next_name("Grid")
        children_dsl = cls._normalize_children(dsl_node, "GridPanel", lint)
        compiled_children: List[Dict[str, Any]] = []

        for child_dsl in children_dsl:
            child_json = cls._compile_leaf(child_dsl, "GridPanel", lint)
            row = int(child_dsl.get("row", child_dsl.get("grid_row", 0)))
            col = int(child_dsl.get("col", child_dsl.get("grid_col", child_dsl.get("column", 0))))
            row_span = int(child_dsl.get("row_span", 1))
            col_span = int(child_dsl.get("col_span", child_dsl.get("column_span", 1)))

            slot = child_json.setdefault("properties", {}).setdefault("Slot", {})
            slot["Row"] = row
            slot["Column"] = col
            slot["RowSpan"] = row_span
            slot["ColumnSpan"] = col_span

            align_slot = cls.compile_slot_for_widget(
                child_dsl, "GridPanel", lint,
                widget_name=str(child_json.get("widget_name", "")),
                partial=True,
            )
            slot.update(align_slot)

            child_json["SlotData"] = slot
            compiled_children.append(child_json)

        result: Dict[str, Any] = {
            "widget_name": name,
            "widget_class": WIDGET_CLASS_MAP["GridPanel"],
            "children": compiled_children,
        }
        widget_props = dict(dsl_node.get("properties") or {})
        parent_slot = cls._compile_parent_slot(dsl_node, parent_container, lint)
        if parent_slot:
            widget_props["Slot"] = {**widget_props.get("Slot", {}), **parent_slot}
        if widget_props:
            result["properties"] = widget_props
        return result

    @classmethod
    def _compile_absolute(cls, dsl_node: Dict[str, Any], parent_container: Optional[str], lint: LayoutLint) -> Dict[str, Any]:
        name = dsl_node.get("name") or dsl_node.get("widget_name") or cls._next_name("Canvas")
        children_dsl = cls._normalize_children(dsl_node, "CanvasPanel", lint)
        compiled_children: List[Dict[str, Any]] = []

        for child_dsl in children_dsl:
            child_json = cls._compile_leaf(child_dsl, "CanvasPanel", lint)
            slot = cls._compile_canvas_slot(child_dsl, lint)
            child_json.setdefault("properties", {})["Slot"] = slot
            child_json["SlotData"] = slot
            compiled_children.append(child_json)

        result: Dict[str, Any] = {
            "widget_name": name,
            "widget_class": WIDGET_CLASS_MAP["CanvasPanel"],
            "children": compiled_children,
        }
        widget_props = dict(dsl_node.get("properties") or {})
        parent_slot = cls._compile_parent_slot(dsl_node, parent_container, lint)
        if parent_slot:
            widget_props["Slot"] = {**widget_props.get("Slot", {}), **parent_slot}
        if widget_props:
            result["properties"] = widget_props
        return result

    @classmethod
    def _compile_leaf(cls, dsl_node: Dict[str, Any], parent_container: Optional[str], lint: LayoutLint) -> Dict[str, Any]:
        short, full_class = cls._resolve_widget_class(dsl_node)
        name = dsl_node.get("name") or dsl_node.get("widget_name") or cls._next_name(short)

        widget_props = dict(dsl_node.get("properties") or {})

        if "text" in dsl_node and short in ("Text", "TextBlock"):
            widget_props.setdefault("Text", dsl_node["text"])
        if "content" in dsl_node:
            widget_props.setdefault("Text", dsl_node["content"])

        parent_slot = cls._compile_parent_slot(dsl_node, parent_container, lint)
        if parent_slot:
            widget_props["Slot"] = {**widget_props.get("Slot", {}), **parent_slot}

        result: Dict[str, Any] = {
            "widget_name": name,
            "widget_class": full_class,
        }
        if widget_props:
            result["properties"] = widget_props

        children_dsl = dsl_node.get("children")
        if children_dsl:
            normalized = cls._normalize_children(dsl_node, short, lint)
            result["children"] = [
                cls._compile_node(child, parent_container=short, lint=lint)
                for child in normalized
            ]

        if "SlotData" in dsl_node:
            slot = dsl_node["SlotData"]
            result.setdefault("properties", {})["Slot"] = {
                **result.get("properties", {}).get("Slot", {}),
                **slot,
            }
            result["SlotData"] = result["properties"]["Slot"]

        return result

    @classmethod
    def _compile_child_under_flex(
        cls,
        child_dsl: Dict[str, Any],
        container_short: str,
        is_column: bool,
        gap: float,
        align_items: str,
        justify_content: str,
        is_last: bool,
        lint: LayoutLint,
    ) -> Dict[str, Any]:
        child_layout = child_dsl.get("layout_type") or child_dsl.get("display")
        if child_layout in ("flex", "grid", "absolute", "canvas"):
            child_json = cls._compile_node(child_dsl, parent_container=container_short, lint=lint)
        else:
            child_json = cls._compile_leaf(child_dsl, parent_container=container_short, lint=lint)
            if child_dsl.get("children"):
                normalized = cls._normalize_children(child_dsl, cls._resolve_widget_class(child_dsl)[0], lint)
                child_json["children"] = [
                    cls._compile_node(c, parent_container=cls._resolve_widget_class(child_dsl)[0], lint=lint)
                    for c in normalized
                ]

        if any(k in child_dsl for k in ("anchors", "position", "offsets")) and container_short != "CanvasPanel":
            lint.warn(
                f"Widget '{child_json.get('widget_name', '?')}': anchors/position ignored under "
                f"{container_short} (use layout_type: absolute for canvas positioning).",
                rule_id="canvas-misuse",
                widget_path=str(child_json.get("widget_name", "")),
            )

        slot = cls._compile_box_slot_semantics(
            child_dsl,
            container_short,
            lint,
            str(child_json.get("widget_name", "?")),
            container_align=align_items,
            container_justify=justify_content,
            is_column=is_column,
            partial=False,
        )

        if gap > 0 and not is_last:
            if is_column:
                slot["Padding"] = {**slot.get("Padding", {}), "Bottom": gap}
            else:
                slot["Padding"] = {**slot.get("Padding", {}), "Right": gap}

        child_json.setdefault("properties", {})["Slot"] = {
            **child_json.get("properties", {}).get("Slot", {}),
            **slot,
        }
        child_json["SlotData"] = child_json["properties"]["Slot"]
        return child_json

    @classmethod
    def _compile_canvas_slot(cls, child_dsl: Dict[str, Any], lint: LayoutLint) -> Dict[str, Any]:
        slot: Dict[str, Any] = dict((child_dsl.get("properties") or {}).get("Slot", {}))
        slot.update(child_dsl.get("SlotData") or {})

        size_val = child_dsl.get("size")
        position: Optional[str] = None

        if "position" in child_dsl:
            position = str(child_dsl["position"]).lower().replace("_", "-")
        elif isinstance(size_val, str) and size_val.strip().lower() in ("fill", "100%", "stretch"):
            position = "fill"
        elif cls._semantic_keys_present(child_dsl, ALIGN_SEMANTIC_KEYS):
            h_ue = cls._parse_ue_align(child_dsl.get("h_align"), "h") or "Left"
            v_ue = cls._parse_ue_align(child_dsl.get("v_align"), "v") or "Top"
            position = cls._align_to_canvas_position(h_ue, v_ue)
        else:
            position = "top-left"

        if position and position in POSITION_PRESETS:
            anchor_min, anchor_max, alignment = POSITION_PRESETS[position]
            slot["LayoutData"] = {
                "Anchors": {"Minimum": anchor_min, "Maximum": anchor_max},
                "Offsets": slot.get("LayoutData", {}).get("Offsets", {"Left": 0, "Top": 0, "Right": 0, "Bottom": 0}),
            }
            slot["Alignment"] = alignment
        elif "anchors" in child_dsl:
            anchors = child_dsl["anchors"]
            slot["LayoutData"] = {
                "Anchors": {
                    "Minimum": anchors.get("min", anchors.get("minimum", [0, 0])),
                    "Maximum": anchors.get("max", anchors.get("maximum", anchors.get("min", [0, 0]))),
                },
            }
            if "offsets" in child_dsl:
                slot["LayoutData"]["Offsets"] = child_dsl["offsets"]
            if "alignment" in child_dsl:
                slot["Alignment"] = child_dsl["alignment"]

        if "margin" in child_dsl:
            m = child_dsl["margin"]
            if isinstance(m, (int, float)):
                slot.setdefault("LayoutData", {}).setdefault("Offsets", {})
                offsets = slot["LayoutData"]["Offsets"]
                offsets.update({"Left": m, "Top": m, "Right": m, "Bottom": m})
            elif isinstance(m, dict):
                slot.setdefault("LayoutData", {}).setdefault("Offsets", {}).update(m)

        if isinstance(size_val, (list, tuple)) or (isinstance(size_val, (int, float)) and not isinstance(size_val, bool)):
            if isinstance(size_val, (int, float)):
                w, h = size_val, size_val
            else:
                w, h = size_val if len(size_val) >= 2 else (size_val[0], size_val[0])
            layout_data = slot.setdefault("LayoutData", {})
            offsets = dict(layout_data.get("Offsets", {"Left": 0, "Top": 0, "Right": 0, "Bottom": 0}))
            offsets["Right"] = w
            offsets["Bottom"] = h
            layout_data["Offsets"] = offsets
            slot.pop("Size", None)

        return cls._strip_incompatible_slot_keys(slot, "CanvasPanel", lint, child_dsl.get("name", "?"))

    @classmethod
    def _compile_parent_slot(
        cls,
        dsl_node: Dict[str, Any],
        parent_container: Optional[str],
        lint: LayoutLint,
    ) -> Dict[str, Any]:
        """Compile slot props that belong to this node (owned by its parent slot)."""
        slot: Dict[str, Any] = dict((dsl_node.get("properties") or {}).get("Slot", {}))
        slot.update(dsl_node.get("SlotData") or {})

        if not parent_container:
            if cls._semantic_keys_present(dsl_node, SIZE_SEMANTIC_KEYS | ALIGN_SEMANTIC_KEYS):
                return cls._compile_box_slot_semantics(
                    dsl_node, "VerticalBox", lint, str(dsl_node.get("name", "?")), partial=True,
                )
            return cls._strip_incompatible_slot_keys(slot, "", lint, dsl_node.get("name", "?"))

        if parent_container in ("VerticalBox", "HorizontalBox"):
            semantics = cls._compile_box_slot_semantics(
                dsl_node,
                parent_container,
                lint,
                str(dsl_node.get("name", "?")),
                partial=not cls._semantic_keys_present(dsl_node, SIZE_SEMANTIC_KEYS | ALIGN_SEMANTIC_KEYS),
            )
            slot.update(semantics)
            return cls._strip_incompatible_slot_keys(slot, parent_container, lint, dsl_node.get("name", "?"))

        if parent_container == "CanvasPanel":
            if cls._semantic_keys_present(
                dsl_node, SIZE_SEMANTIC_KEYS | ALIGN_SEMANTIC_KEYS | CANVAS_SEMANTIC_KEYS
            ):
                canvas_slot = cls._compile_canvas_slot(dsl_node, lint)
                slot.update(canvas_slot)
            return slot

        semantics = cls.compile_slot_for_widget(
            dsl_node, parent_container, lint,
            widget_name=str(dsl_node.get("name", "?")),
            partial=True,
        )
        slot.update(semantics)
        return cls._strip_incompatible_slot_keys(slot, parent_container, lint, dsl_node.get("name", "?"))

    @classmethod
    def _strip_incompatible_slot_keys(
        cls,
        slot: Dict[str, Any],
        parent_container: str,
        lint: LayoutLint,
        widget_name: str,
    ) -> Dict[str, Any]:
        if parent_container and parent_container != "CanvasPanel":
            stripped = [k for k in CANVAS_ONLY_SLOT_KEYS if k in slot]
            if stripped:
                slot = {k: v for k, v in slot.items() if k not in CANVAS_ONLY_SLOT_KEYS}
                lint.warn(
                    f"Widget '{widget_name}': discarded canvas-only slot keys {stripped} "
                    f"(parent is {parent_container}).",
                    rule_id="slot-incompatible",
                    widget_path=widget_name,
                )

        allowed = SLOT_SCHEMA_BY_PARENT.get(parent_container)
        if allowed:
            rejected = [k for k in slot if k not in allowed]
            if rejected:
                slot = {k: v for k, v in slot.items() if k in allowed}
                lint.warn(
                    f"Widget '{widget_name}': rejected slot keys {rejected} for parent {parent_container}.",
                    rule_id="slot-incompatible",
                    widget_path=widget_name,
                )

        return slot

    @classmethod
    def _normalize_children(
        cls,
        dsl_node: Dict[str, Any],
        parent_short: str,
        lint: LayoutLint,
    ) -> List[Dict[str, Any]]:
        children: List[Dict[str, Any]] = list(dsl_node.get("children") or [])
        if not children:
            return children

        if parent_short in SINGLE_CHILD_PANELS and len(children) > 1:
            wrapper_name = cls._next_name(f"{dsl_node.get('name', parent_short)}_Wrap")
            lint.warn(
                f"Parent '{parent_short}' accepts one child; wrapping {len(children)} children "
                f"in VerticalBox '{wrapper_name}'.",
                rule_id="single-child-wrap",
                widget_path=str(dsl_node.get("name", parent_short)),
            )
            return [{
                "layout_type": "flex",
                "direction": "column",
                "name": wrapper_name,
                "children": children,
            }]

        return children
