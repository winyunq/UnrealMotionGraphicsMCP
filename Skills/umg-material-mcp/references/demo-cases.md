# Material HLSL MCP Demo Cases

Use these cases as examples when authoring or validating material MCP changes.

## Case 1: Default UI Material

Create or lock a UI material target without type fields:

```python
await hlsl_set_target(
    path="/Game/UMGMCP/Demos/M_Hlsl_UI",
    create_if_not_found=True
)
await hlsl_set(
    hlsl="return float4(UV.x, UV.y, 1.0, 0.85);",
    parameters=[]
)
await hlsl_compile()
```

Expected contract:

```json
{
  "return": "float4",
  "domain": "ui",
  "blend_mode": "translucent",
  "shading_model": "unlit",
  "rgb_to": "EmissiveColor",
  "a_to": "Opacity"
}
```

## Case 2: Non-UI Surface Material

Set material type on `hlsl_set_target`; do not call a separate type command:

```python
await hlsl_set_target(
    path="/Game/UMGMCP/Demos/M_Hlsl_Surface",
    create_if_not_found=True,
    domain="surface",
    shading_model="unlit",
    blend_mode="opaque"
)
await hlsl_set(
    hlsl="\n".join([
        "float glow = saturate(Intensity);",
        "Roughness = 0.28;",
        "Metallic = 0.0;",
        "Normal = normalize(float3(0.0, 0.0, 1.0));",
        "return float4((0.05 + UV.x * 0.7) * glow, 0.2, 1.0 - UV.y * 0.6, 1.0);"
    ]),
    parameters=[{"name": "Intensity", "kind": "Scalar"}],
    outputs=["Roughness", "Metallic", "Normal"]
)
await hlsl_compile()
```

Expected contract includes:

```json
{
  "domain": "surface",
  "blend_mode": "opaque",
  "shading_model": "unlit",
  "rgb_to": "EmissiveColor",
  "outputs": [
    {"name": "Roughness", "target": "Roughness", "connected": true},
    {"name": "Metallic", "target": "Metallic", "connected": true},
    {"name": "Normal", "target": "Normal", "connected": true}
  ]
}
```

## Case 3: Unified Delete Ambiguity

If a parameter and output share the same semantic name, call `hlsl_delete` without `kind` first:

```python
await hlsl_set(
    parameters=[{"name": "Roughness", "kind": "Scalar"}],
    outputs=["Roughness", "Metallic", "Normal"]
)
result = await hlsl_delete(names=["Roughness"], confirm_delete=True)
```

Expected failure:

```json
{
  "status": "error",
  "requires_kind": true,
  "ambiguous": [
    {
      "name": "Roughness",
      "candidates": [
        {"kind": "parameter", "name": "Roughness"},
        {"kind": "output", "name": "Roughness"}
      ]
    }
  ]
}
```

Then retry precisely:

```python
await hlsl_delete(names=["Roughness"], kind="parameter", confirm_delete=True)
await hlsl_delete(names=["Roughness"], kind="output", confirm_delete=True)
```

After deleting one output, remaining outputs must keep their own targets and `connected: true`; this guards against UE Custom-node AdditionalOutput index drift.

## Repo Tests

The runnable demo is:

```powershell
python Resources\Python\APITest\Material_HLSL_Type_Demo.py
```

The protocol shape/static regression is:

```powershell
python Resources\Python\APITest\Material_Protocol_Static_Check.py
```
