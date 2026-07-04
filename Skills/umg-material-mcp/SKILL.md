---
name: umg-material-mcp
description: Use when Codex needs to create, inspect, edit, delete, or validate UE5.8 material assets through Unreal Motion Graphics MCP/FabUmgMcp HLSL material tools, especially one-HLSL Custom-node authoring, UI-default versus non-UI material type selection, union-only writes, semantic outputs, hlsl_delete recovery, or Material ToolMode protocol work.
---

# UMG Material MCP

Use the Material HLSL MCP path for material authoring. Do not mutate `.uasset` files directly. Keep Unreal Editor open with the UMG MCP bridge listening before runtime tool calls.

For concrete command examples and regression cases, read [demo-cases.md](references/demo-cases.md).

## Primary Workflow

1. Set or create the HLSL target with `hlsl_set_target`.
   - New targets default to UI material behavior.
   - For non-UI materials, pass type fields directly on `hlsl_set_target`: `domain`, `blend_mode`, `shading_model`, `two_sided`.
   - Prefer this over `material_modify_type`; that command is compatibility-only.

2. Read with `hlsl_get`.
   - Treat the response as semantic state: HLSL text, parameter snapshot, and `output_contract`.
   - Do not request or reconstruct full material graphs unless the HLSL protocol cannot express the task.

3. Write with `hlsl_set(hlsl?, parameters?, outputs?)`.
   - `hlsl_set` is union-only: update existing entries and append missing entries.
   - It must not delete parameters or outputs.
   - Use `parameters` for Custom node inputs such as `{"name":"Intensity","kind":"Scalar"}`.
   - Use `outputs` for root material outputs such as `"Roughness"`, `"Metallic"`, `"Normal"`, or objects with `name`, `target`, and `type`.
   - Additional outputs are UE5.8 Custom node `AdditionalOutputs`; assign same-name variables in HLSL, for example `Roughness = 0.35;`.

4. Delete with `hlsl_delete(names, confirm_delete=true, kind?)`.
   - Omit `kind` in the normal path.
   - If a name matches both parameter and output, expect failure with `requires_kind: true`; retry with `kind="parameter"` or `kind="output"`.
   - Do not use `hlsl_delete_parameter` or `hlsl_delete_output` in primary flows; they are compatibility aliases.

5. Compile with `hlsl_compile`.
   - Treat compile diagnostics as the feedback loop.
   - Prefer small repair writes with `hlsl_set(hlsl=...)` rather than resetting the target.

## Protocol Rules

- The material is represented as one HLSL program on a single Custom node.
- The Custom node primary return is `float4`.
- `output_contract` describes where rgb/a and any additional outputs are wired.
- UI, PostProcess, LightFunction, and Unlit output rgb to `EmissiveColor`; lit Surface output rgb usually maps to `BaseColor`.
- Alpha maps only when semantically useful: UI/translucent to `Opacity`, masked to `OpacityMask`.
- Do not fall back to legacy `material_*` graph editing unless the requested operation cannot be modeled by HLSL target, parameters, outputs, delete, and compile.

## Validation

Use the narrowest relevant checks:

```powershell
python -m py_compile Resources\Python\UmgMcpServer.py Resources\Python\Material\UMGMaterial.py Resources\Python\APITest\Material_HLSL_Type_Demo.py Resources\Python\APITest\Material_Protocol_Static_Check.py
python Resources\Python\APITest\Material_Protocol_Static_Check.py
& 'D:\UE_5.8\Engine\Build\BatchFiles\Build.bat' FabUMGMCPEditor Win64 Development -Project='D:\UE5Project\FabUMGMCP\FabUMGMCP.uproject' -NoHotReloadFromIDE -Progress
python Resources\Python\APITest\Material_HLSL_Type_Demo.py
```

The runtime demo requires Unreal Editor to be open and the MCP socket in `Resources/Python/mcp_config.py` to be reachable.
