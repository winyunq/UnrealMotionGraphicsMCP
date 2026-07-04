# Five-System Login Demo

Run the demo through stdio MCP:

```powershell
python Resources\Python\APITest\Login_Demo_Five_Systems.py
```

The script creates timestamped assets under `/Game/UMGMCP/Demos`, writes a JSONL operation trace under `Resources/Python/APITest/Artifacts`, and prints a summary with the UMG asset path, material path, animation name, tool count, Sequencer track count, and Blueprint node count.

## Systems Demonstrated

1. Target/default context
   - `set_target_umg_asset` creates/selects the UMG asset.
   - The root widget creation omits `parent_name` to verify default/root behavior.
   - Later calls rely on target context instead of repeating the asset path.

2. Widget authoring
   - Creates a login screen using `CanvasPanel`, `Overlay`, `VerticalBox`, `HorizontalBox`, `Image`, `Border`, `TextBlock`, `EditableTextBox`, and `Button`.
   - Uses overlay siblings instead of adding children under `Button` or `Border`, because current creation only attaches through panel parents.
   - Reads back `get_widget_tree` and `query_widget_properties` and checks for key widgets.

3. HLSL Material
   - Creates a default UI material with `hlsl_set_target`.
   - Writes one Custom-node HLSL program with `hlsl_set`.
   - Compiles with `hlsl_compile`.
   - Assigns the material as an `Image.Brush.ResourceObject`.

4. Sequencer
   - Creates `LoginIntro`.
   - Animates `LoginCard` opacity and scale.
   - Animates `ScannerAccent` angle.
   - Reads `animation_overview` and asserts expected tracks.

5. Blueprint
   - Compiles the target UMG asset.
   - Calls `set_edit_function("LoginButton.OnClicked")`.
   - Adds a `PrintString` step.
   - Reads `get_function_nodes` as a minimal sanity check.

## Expected Gaps

- Current Blueprint readback is node-shaped. It is enough for sanity checks but not enough for a stable semantic protocol.
- Current widget creation cannot parent through `UContentWidget` classes such as `Button` and `Border`; use `Overlay` siblings for now.
- Full UMG JSON export is an advanced read path and may be hidden by tool configuration; prefer semantic reads in normal demos.
- The demo uses procedural HLSL visuals. If product-like image composition is required, add or validate an MCP image/texture import and brush assignment workflow.
- Video recording is not required for protocol validation. Keep the JSONL operation trace as the reproducible source of truth; record video only as presentation material.

## Failure Triage

- If `tools/list` is missing required tools, fix the Python FastMCP registration layer first.
- If `hlsl_compile` fails, repair with a small `hlsl_set(hlsl=...)` write; do not reset the material graph.
- If `LoginButton.OnClicked` does not produce a cursor node, compile the UMG asset first and verify the widget exists in `get_widget_tree`.
- If Sequencer reads do not show `RenderTransform.Scale.X/Y`, inspect the 2D transform track path before changing demo data.
