# Material MCP Protocol Draft

本文记录当前开源 Material MCP 的精简协议原则。范围限定为 UE 5.8；不引入商业版服务依赖。

## 设计原则

1. 默认创建 UI 材质。`material_set_target` 和 `hlsl_set_target` 创建新材质时仍使用 `MD_UI` + `Translucent`，保证 UMG 场景只给名字也能看到结果。
2. 非 UI 材质必须显式声明。使用 `hlsl_set_target` 的 `domain`、`blend_mode`、`shading_model`、`two_sided` 参数修改当前目标材质类型。
3. HLSL 是主路径。Material ToolMode 默认只暴露 HLSL 闭环、显式类型修改和显式删除参数，避免 AI 走低阶图编辑。
4. 读操作返回语义，不返回完整图。`hlsl_get` 返回 HLSL、参数快照和 `output_contract`，说明 `float4` 的 rgb/a 与额外 outputs 当前连接到什么材质输出。
5. 写操作只做并集覆盖和追加。`hlsl_set` 可以更新代码、更新已有参数类型、追加新参数、追加/更新 outputs，但不能删除。
6. 删除必须走单独命令。统一使用 `hlsl_delete(names, confirm_delete=true, kind?)`。默认按名称语义匹配 parameter/output；如果歧义，返回失败并要求补 `kind`。

## 当前精简工具

| Tool | 作用 |
| --- | --- |
| `hlsl_set_target(path, confirm_overwrite=false, create_if_not_found=true, domain?, blend_mode?, shading_model?, two_sided?)` | 设置或创建 HLSL 材质目标。新材质默认 UI；可携带类型字段切到非 UI。已有材质必须满足单 Custom 节点拓扑，否则需要确认覆写。 |
| `hlsl_get()` | 读取当前 HLSL、参数列表和输出契约。 |
| `hlsl_set(hlsl?, parameters?, outputs?)` | 并集式写入 HLSL、参数与材质输出。不会删除。 |
| `hlsl_delete(names, confirm_delete=true, kind?)` | 显式删除 HLSL 参数或输出。名称歧义时补 `kind="parameter"` 或 `kind="output"`。 |
| `hlsl_compile()` | 编译当前材质并返回精简诊断。 |

## `hlsl_set_target` 类型参数

`domain` 支持：

- `ui`
- `surface`
- `post_process`
- `deferred_decal`
- `light_function`
- `volume`

`blend_mode` 支持：

- `opaque`
- `masked`
- `translucent`
- `additive`
- `modulate`
- `alpha_composite`
- `alpha_holdout`

`shading_model` 支持 UE 5.8 常用模型：

- `unlit`
- `default_lit`
- `subsurface`
- `preintegrated_skin`
- `clear_coat`
- `subsurface_profile`
- `two_sided_foliage`
- `hair`
- `cloth`
- `eye`
- `single_layer_water`
- `thin_translucent`

## 输出契约

HLSL Custom 节点主返回值固定为 `float4`。后端根据材质类型生成语义契约：

```json
{
  "return": "float4",
  "domain": "ui",
  "blend_mode": "translucent",
  "shading_model": "unlit",
  "rgb_to": "EmissiveColor",
  "a_to": "Opacity",
  "outputs": [
    {"name": "Roughness", "target": "Roughness", "type": "float1", "connected": true}
  ]
}
```

规则：

- UI、Post Process、Light Function、Unlit 材质：`rgb -> EmissiveColor`
- 默认 Lit Surface：`rgb -> BaseColor`
- Masked：`a -> OpacityMask`
- 非 Opaque 且非 Masked：`a -> Opacity`
- Opaque：不声明 alpha 输出

`outputs` 是 Custom 节点的 AdditionalOutputs。HLSL 代码里使用同名 `inout` 变量赋值：

```hlsl
Roughness = 0.35;
Metallic = 0.0;
Normal = normalize(float3(0, 0, 1));
return float4(BaseColorValue, 1);
```

`outputs` 最简写法是字符串数组：

```json
["Roughness", "Metallic", "Normal"]
```

需要自定义 HLSL 变量名或类型时使用对象：

```json
[
  {"name": "ClearCoatAmount", "target": "CustomData0", "type": "float1"},
  {"name": "BentNormal", "target": "Normal", "type": "float3"}
]
```

常用 target 默认类型：

- `BaseColor`, `EmissiveColor`, `Normal`, `Tangent`, `WorldPositionOffset`, `SubsurfaceColor`: `float3`
- `CustomizedUVs0` 到 `CustomizedUVs7`: `float2`
- `Metallic`, `Specular`, `Roughness`, `Anisotropy`, `Opacity`, `OpacityMask`, `AmbientOcclusion`, `Refraction`, `PixelDepthOffset`, `CustomData0`, `CustomData1`: `float1`

## 非 UI 示例

创建一个可见的非 UMG Surface HLSL 材质：

```python
await hlsl_set_target(
    "/Game/Materials/M_HlslSurface",
    domain="surface",
    shading_model="unlit",
    blend_mode="opaque"
)
await hlsl_set(
    hlsl="Roughness = 0.25; return float4(0.1, 0.8, 1.0, 1.0);",
    outputs=["Roughness"]
)
await hlsl_compile()
```

创建 Masked Surface：

```python
await hlsl_set_target(
    "/Game/Materials/M_HlslMasked",
    domain="surface",
    shading_model="unlit",
    blend_mode="masked"
)
await hlsl_set(hlsl="return float4(1, 1, 1, UV.x > 0.5 ? 1 : 0);")
await hlsl_compile()
```

## 兼容层

传统 `material_*` 低阶图编辑工具暂时保留作为兼容层，但 Material ToolMode 不建议使用。后续协议清理时应逐个判断是否合并、下线或转为内部实现细节。
