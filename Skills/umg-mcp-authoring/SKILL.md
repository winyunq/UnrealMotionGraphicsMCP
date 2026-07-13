---
name: umg-mcp-authoring
description: Use when Codex needs to create, edit, inspect, validate, or demo UE5.8 UMG interfaces through Unreal Motion Graphics MCP/FabUmgMcp, especially target/default workflows, widget tree authoring, HLSL-backed UI materials, Sequencer widget animations, Blueprint event wiring, readback validation, or operation-trace demo scripts.
---

# UMG MCP Authoring

Use MCP tools as the hands and this skill as the experience. Do not mutate `.uasset` files directly and do not bypass MCP with raw socket scripts when validating the public protocol.

For the five-system login demo and concrete script workflow, read [login-demo.md](references/login-demo.md).

## Workflow

1. Set the attention target first with `set_target_umg_asset`.
   - Passing a missing `/Game/...` UMG path creates a `WidgetBlueprint`.
   - Use `set_target_widget` only when the next operation is intentionally scoped to a widget.
   - Prefer omitted/default path arguments after target is established.

2. Build widget structure with panel parents.
   - `CanvasPanel`, `Overlay`, `VerticalBox`, and `HorizontalBox` are reliable authoring parents.
   - Current `create_widget` attaches children through `UPanelWidget`; do not create children directly under `Button` or `Border`.
   - To put text on a button or content over a border, create an `Overlay` and add the visual widget plus the text as siblings.

3. Write properties as union updates.
   - `set_widget_properties` should set or overwrite supplied properties and leave unspecified properties intact.
   - Use `Slot.Position` and `Slot.Size` aliases for CanvasPanel children when possible.
   - Use nested `Slot` objects for panel-slot alignment and padding.
   - Use `delete_widget` only for explicit deletion and pass `confirm_delete=true`.
   - Prefer `apply_layout` for bulk layout authoring; `export_umg_to_json` and `apply_json_to_umg` are hidden compatibility/debug paths.

4. Use HLSL materials for UI visual richness.
   - Prefer `hlsl_set_target`, `hlsl_set`, and `hlsl_compile`.
   - New HLSL targets default to UI material behavior.
   - For detailed material rules, use the `umg-material-mcp` skill.

5. Animate widgets through Sequencer.
   - Create or select an animation with `create_animation` and `set_animation_scope`.
   - Focus the animated widget with `set_target_widget` or `set_widget_scope`.
   - Use `animation_append_widget_tracks` for widget-oriented union key writes.
   - Use `animation_append_time_slice` when authoring a single moment across one or more widgets.
   - Read back with `animation_overview` or `animation_time_properties`; do not trust write responses alone.
   - Treat `get_animation_keyframes`, `get_animation_full_data`, `set_property_keys`, `remove_property_track`, and `remove_keys` as hidden compatibility commands, not default authoring tools.

6. Wire Blueprint through BlueCode.
   - Use `bluecode_set_function("WidgetName.EventName")` for component event scope.
   - Default `bluecode_read_function(detail="semantic")` is the attention-compressed comprehension view and intentionally omits node/pin sidecars. Before editing an existing graph, re-read with `detail="roundtrip"`; use `detail="debug"` or `include_connections=true` only when raw nodes, pins, or edges are actually needed.
   - Preserve `base_revision`, `source_map`, `action_hints`, `expression_hints`, and `action_hints_by_line` from the round-trip read and pass them back to `bluecode_apply`. A stale `base_revision` must fail instead of rematching changed graph state.
   - When editing a readback line, keep the corresponding hint. Hints with `node_id` cause `bluecode_apply` to update the existing node's supplied input pins/defaults instead of appending a duplicate node, including variable-set and Branch nodes; `Pin=A+B` and `Pin=value("Select", ...)` can create data expression nodes for that existing pin. Unmentioned pins and links are preserved.
   - When reconnecting or deleting edges, read `connections` via `detail="debug"` or `include_connections=true`; use each entry's `connect` as a `bluecode_connect` reference and `delete_target` as a `bluecode_delete(..., confirm_delete=true)` target.
   - Write with default `bluecode_apply(mode="union")` and `bluecode_apply_variables`; use `dry_run=true` when the insertion plan should be inspected first. Union mode aligns semantic statements, inserts unmatched statements before the next matched right anchor, otherwise appends at the block end, and never deletes omitted graph content. Use `FunctionName(Pin=value, ...)` for normal calls, `node("Action Menu Name", Pin=value, ...)` for generic exec/action nodes, and `value("Action Menu Name", Pin=value, ...)` for pure/data nodes.
   - Pin names in `Pin=value` and `bluecode_connect` endpoints may use internal pin names, editor display names, or FriendlyName. When omitting an endpoint pin, rely on automatic resolution only if the node has one matching data input/output.
   - Pin values may be strings, numbers, booleans, or structured objects such as `{path:"/Game/..."}`, `{class:"/Script/..."}`, `{object:"..."}`, `{text:"..."}`, or `{literal:"(X=0,Y=0)"}`. Check `input_warnings` after writes.
   - Pin values may also be data expressions: `Pin=value("Select", Index=ActiveIndex)`, `Pin=PureFunction(A, B)`, or `Pin=A + B`. These create pure/data nodes and connect their compatible output.
   - For custom pure/data expression nodes, pass action hints for the inner expression as well, using the inner Action Menu name or full expression key, such as `expression_hints={"My Custom Pure": {...}}` for `Pin=value("My Custom Pure", Input=A)`. Readback returns these dependency hints in `expression_hints`, mirrors them into `action_hints`, and `bluecode_apply` accepts either object directly.
   - If a custom, plugin, or macro node stores behavior in UObject node properties rather than pins, pass `node_properties` to `bluecode_apply` using the same statement/name keys as `action_hints`, or put `node_properties`/`properties` inside the matching hint object. Treat `operations[].result.node_properties.failures` as a failed semantic write.
   - If a data expression cannot connect to the target pin, the temporary expression nodes are rolled back and the reason appears in `input_warnings`.
   - Variable-input nodes that implement UE's native add-pin interface can grow before BlueCode writes values, such as `value("Make Array", 0=First, 1=Second, 2=Third)` or multi-input math nodes. Array-style pins accept either `[0]` or `0`.
   - `Format Text` is preconfigured from its `Format` pin before the rest of the inputs are applied, so `value("Format Text", Format="Hello {Name}", Name=PlayerName)` can generate the `Name` argument pin.
   - Treat `input_warnings` as write feedback that must be read and handled; warnings mean part of the requested semantic write did not map cleanly.
   - Add `alias=Name` inside `bluecode_apply` statements, or pass `node_aliases`, when newly created nodes must be referenced later. Pass the returned `aliases` map into `bluecode_connect` and use `Name:Pin` endpoints.
   - Treat each successful `bluecode_apply.operations[].result` as immediate context for the next operation: it returns `nodeId`/`node_id`, node title/class, `is_exec`, `pin_counts`, and `inputs`/`outputs` pin evidence using the same endpoint format as readback hints.
   - Use `bluecode_connect` only when a complex custom, plugin, or macro node needs explicit pin links that cannot be inferred from `bluecode_apply` inputs. Treat it as union-only connection append, not as a reconnect/delete tool.
   - For custom, plugin, macro, or ambiguous nodes, call `bluecode_search_nodes` first and pass its returned `handle` as an `action_handle`, or place the returned search result object directly in `bluecode_apply.action_hints` so `signature/category/node_class/node_class_path` remain available for fallback matching. Use `is_exec` to choose `node(...)` versus `value(...)`; use `include_pins=true` only when `pin_counts` is not enough.
   - Delete nodes, variables, or existing pin connections only with `bluecode_delete(..., confirm_delete=true)`. For connection deletes, use `{"kind":"connection","source":"NodeA:Then","target":"NodeB:Execute"}` or `"NodeA:Then -> NodeB:Execute"`; if a node pair has multiple links, provide the exact pin names instead of guessing.
   - Treat `add_step`, `prepare_value`, `connect_data_to_pin`, `get_function_nodes`, `delete_node`, and `delete_variable` as backend compatibility tools, not default authoring tools.

7. Validate with read operations.
   - Use `get_widget_tree` after structure writes.
   - Use `query_widget_properties` for precise property checks.
   - Use `export_umg_to_json` only when that advanced tool is enabled and a full file snapshot is needed.
   - Use `save_asset` only after the target has passed focused readback checks.

## Blueprint Direction

Treat BlueCode as the default Blueprint MCP direction. The target is semantic/code-like Blueprint read/write, not raw node dumps. Node-level reads should be compressed into intent such as events, variables, calls, branches, bindings, and data dependencies.

Useful reference directions:

- Code-like Blueprint representations, similar to Blueprint-to-C++ or pseudocode translation.
- JSON extraction for complete fallback inspection.
- Compact Markdown/ASCII views only for debug or review, not as the primary protocol.

## Validation Commands

```powershell
python -m py_compile Resources\Python\UmgMcpServer.py Resources\Python\APITest\Login_Demo_Five_Systems.py Resources\Python\APITest\Umg_Widget_Protocol_Static_Check.py Resources\Python\APITest\Blueprint_Protocol_Static_Check.py Resources\Python\APITest\Animation_Protocol_Static_Check.py Resources\Python\APITest\Sequencer_RenderTransform_Check.py
python Resources\Python\APITest\Umg_Widget_Protocol_Static_Check.py
python Resources\Python\APITest\Blueprint_Protocol_Static_Check.py
python Resources\Python\APITest\Animation_Protocol_Static_Check.py
python Resources\Python\APITest\Material_Protocol_Static_Check.py
python Resources\Python\APITest\Sequencer_RenderTransform_Check.py
python Resources\Python\APITest\Login_Demo_Five_Systems.py
```

The runtime demo requires Unreal Editor to be open with the UMG MCP bridge listening on the configured host and port.
