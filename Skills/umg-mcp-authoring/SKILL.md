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

6. Wire Blueprint only at the current semantic level.
   - Use `set_edit_function("WidgetName.EventName")` for component event scope.
   - Use `add_step` for simple executable calls such as `PrintString`.
   - Use `get_function_nodes` only as a minimal sanity check; it is still node-shaped and not the desired long-term semantic Blueprint read protocol.
   - Do not use `delete_node` or `delete_variable` from the compatibility backend; default Blueprint MCP hides them until deletion is hardened with `confirm_delete=true`.
   - For bluecode design work, read `Document/BlueprintBluecodeProtocol.md` and treat it as a draft target, not an implemented tool set.

7. Validate with read operations.
   - Use `get_widget_tree` after structure writes.
   - Use `query_widget_properties` for precise property checks.
   - Use `export_umg_to_json` only when that advanced tool is enabled and a full file snapshot is needed.
   - Use `save_asset` only after the target has passed focused readback checks.

## Blueprint Direction

Treat current Blueprint MCP as transitional. The long-term target is semantic/code-like Blueprint read/write, not raw node dumps. Node-level reads should be compressed into intent such as events, variables, calls, branches, bindings, and data dependencies.

Useful reference directions:

- Code-like Blueprint representations, similar to Blueprint-to-C++ or pseudocode translation.
- JSON extraction for complete fallback inspection.
- Compact Markdown/ASCII views only for debug or review, not as the primary protocol.

## Validation Commands

```powershell
python -m py_compile Resources\Python\UmgMcpServer.py Resources\Python\APITest\Login_Demo_Five_Systems.py Resources\Python\APITest\Blueprint_Protocol_Static_Check.py Resources\Python\APITest\Animation_Protocol_Static_Check.py Resources\Python\APITest\Sequencer_RenderTransform_Check.py
python Resources\Python\APITest\Blueprint_Protocol_Static_Check.py
python Resources\Python\APITest\Animation_Protocol_Static_Check.py
python Resources\Python\APITest\Material_Protocol_Static_Check.py
python Resources\Python\APITest\Sequencer_RenderTransform_Check.py
python Resources\Python\APITest\Login_Demo_Five_Systems.py
```

The runtime demo requires Unreal Editor to be open with the UMG MCP bridge listening on the configured host and port.
