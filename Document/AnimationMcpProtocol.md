# Animation MCP Protocol Draft

本文记录当前开源 Sequencer/UMG Animation MCP 的精简协议原则。范围限定为 UE 5.8；不引入商业版服务依赖。

## 设计原则

1. Target 是默认操作对象。`set_target_umg_asset` 决定 UMG 资产；`set_animation_scope` 决定当前动画；`set_widget_scope` 决定当前动画内的控件。下层 Target 继承上层 Target。
2. 读操作返回语义。读动画时默认返回概览；读控件时返回该控件被动画驱动的属性时间线；读时间时返回该时刻的属性值。只有调试兼容层才返回完整 keyframe dump。
3. 写操作只做并集覆盖和追加。`animation_append_widget_tracks` 和 `animation_append_time_slice` 只能覆盖已有关键帧或追加新关键帧，不允许隐式删除。
4. 删除必须走单独命令。删除指定控件、属性、时间的 key 使用 `animation_delete_widget_keys(confirm_delete=true)`；删除整段动画使用 `delete_animation(confirm_delete=true)`。
5. 所有默认 MCP 返回都必须包含可复述的计数或上下文，例如动画名、控件名、轨道数、关键帧数、时间点或删除数量。

## 当前精简工具

| Tool | 作用 |
| --- | --- |
| `set_animation_scope(animation_name)` | 设置当前动画 Target；若动画不存在则自动创建。 |
| `set_widget_scope(widget_name)` | 设置当前动画内的控件 Target。 |
| `get_all_animations()` | 返回当前 UMG Target 的动画简表。 |
| `animation_overview(animation_name?, widget_name?, property_name?)` | 返回紧凑概览：被动画驱动的控件、属性、关键时间点、轨道数和关键帧数。 |
| `animation_widget_properties(animation_name?, widget_name?, property_name?)` | 按控件视角读取属性时间线，只返回被动画驱动的属性。 |
| `animation_time_properties(times, animation_name?, widget_name?, property_name?)` | 按一个或多个时间点读取属性值。 |
| `create_animation(animation_name)` | 创建或聚焦动画。 |
| `delete_animation(animation_name, confirm_delete=true)` | 显式删除整段动画。 |
| `animation_append_widget_tracks(widget_name, tracks, animation_name?)` | 控件视角追加/覆盖多个属性轨道的关键帧。 |
| `animation_append_time_slice(time, widgets, animation_name?)` | 时间视角追加/覆盖一个时刻下多个控件的属性值，推荐只提供 diff。 |
| `animation_delete_widget_keys(property_name, times, widget_name?, animation_name?, confirm_delete=true)` | 显式删除指定控件、属性、时间点的关键帧。 |

## 默认缺省

最短可用流程：

```python
set_target_umg_asset("/Game/UI/W_Login")
create_animation("Intro")
set_animation_scope("Intro")
set_widget_scope("LoginCard")
animation_append_widget_tracks(
    widget_name="LoginCard",
    tracks=[
        {"property": "RenderOpacity", "keys": [{"time": 0.0, "value": 0.0}, {"time": 0.4, "value": 1.0}]},
        {"property": "RenderTransform.Scale", "keys": [{"time": 0.0, "value": {"x": 0.95, "y": 0.95}}, {"time": 0.4, "value": {"x": 1.0, "y": 1.0}}]},
    ],
)
animation_overview()
```

`animation_name` 和 `widget_name` 可以显式传入，也可以从当前 Target 推导。需要精确操作时传参；需要流式编辑时使用 Target。

## 读语义层级

- `get_all_animations`：图书馆目录，回答有哪些动画。
- `animation_overview`：读一本书的目录，回答有哪些控件、属性和关键时间。
- `animation_widget_properties`：读一个控件章节，回答该控件哪些属性在何时变化。
- `animation_time_properties`：读一个时间页，回答该时刻哪些属性值成立。

`get_animation_keyframes`、`get_animated_widgets`、`get_animation_full_data`、`get_widget_animation_data` 保留在后端兼容层，用于调试或旧脚本迁移，不进入默认 MCP 提示面。

## 写与删除

`animation_append_widget_tracks`：

```json
{
  "widget_name": "LoginCard",
  "tracks": [
    {
      "property": "RenderTransform.Translation",
      "keys": [
        {"time": 0.0, "value": {"x": 0, "y": 24}},
        {"time": 0.5, "value": {"x": 0, "y": 0}}
      ]
    }
  ]
}
```

`animation_append_time_slice`：

```json
{
  "time": 0.75,
  "widgets": [
    {
      "widget_name": "LoginCard",
      "properties": {
        "RenderOpacity": 1.0,
        "RenderTransform.Angle": 2.0
      }
    }
  ]
}
```

删除必须显式：

```json
{
  "property_name": "RenderTransform.Scale.X",
  "times": [0.5],
  "widget_name": "LoginCard",
  "confirm_delete": true
}
```

不带 `confirm_delete=true` 的删除请求必须失败，并返回可解释的安全提示。

## 隐藏兼容命令

以下命令仍在后端路由中保留，但默认 MCP prompt 不暴露：

- `get_animation_keyframes`
- `get_animated_widgets`
- `get_animation_full_data`
- `get_widget_animation_data`
- `set_property_keys`
- `set_animation_data`
- `remove_property_track`
- `remove_keys`

这些命令只能作为内部兼容或调试工具；新 Skill 和 Demo 应使用精简工具。

## ToolMode 支撑工具

Animation ToolMode 除了上述动画工具，还必须允许 `set_target_umg_asset`、`get_target_umg_asset`、`set_target_widget`、`get_target_widget` 和 `save_asset`。这些不是动画语义本身，但它们是 Target/缺省链路和保存流程的一部分。
