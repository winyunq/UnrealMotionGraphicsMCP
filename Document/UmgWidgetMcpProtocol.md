# UMG Widget MCP Protocol Draft

本文记录当前 UMG Widget MCP 的 Target、读、写、删除语义。范围限定为 UE 5.8。

## 设计原则

1. Target 是默认操作对象。`set_target_umg_asset` 决定当前 UMG 资产；`set_target_widget` 决定当前控件或容器。Widget Target 继承 UMG Target。
2. 读操作返回语义。`get_widget_tree` 返回紧凑树；设置 Widget Target 后只读该子树。`query_widget_properties` 返回指定属性；`get_layout_data` 返回布局边界。
3. 写操作只做并集覆盖和追加。`create_widget` 只创建缺失控件；`set_widget_properties` 只覆盖传入属性，不删除未提及属性；`apply_layout` 应按 append/upsert 语义处理布局。
4. 删除必须走单独命令。`delete_widget` 是显式删除工具，必须传入 `confirm_delete=true`。
5. 默认 MCP 不暴露完整 JSON dump/apply 兼容路径。`export_umg_to_json` 和 `apply_json_to_umg` 保留为兼容/调试命令，默认 prompt 隐藏。

## 当前默认工具

| Tool | 作用 |
| --- | --- |
| `set_target_umg_asset(asset_path)` | 设置或创建当前 UMG Target。 |
| `set_target_widget(widget_name)` | 设置当前 Widget Target。 |
| `get_widget_tree()` | 读取当前 Widget Target 子树；无 Widget Target 时读取根树。 |
| `query_widget_properties(widget_name, properties)` | 读取指定控件属性。 |
| `get_layout_data(width?, height?)` | 读取布局边界。 |
| `create_widget(widget_type, new_widget_name, parent_name?)` | 创建控件；未给 parent 时使用 Widget Target 或根。 |
| `set_widget_properties(widget_name?, properties)` | 并集式设置属性；命令层支持缺省 widget_name 使用 Widget Target。 |
| `reorder_widget_tree(root?, tree)` | 并集式调整同父级顺序；给出局部 tree/order，未提及 sibling 保持相对顺序，不创建、不删除。 |
| `reparent_widget(widget_name?, new_parent_widget)` | 转换/重构控件层级；会保留子控件，无法保留时失败。 |
| `apply_layout(layout_content, widget_name?)` | 批量布局应用，作为默认高层布局入口。 |
| `delete_widget(widget_name, confirm_delete=true)` | 显式删除控件。 |
| `save_asset()` | 保存当前资产。 |

## 写与删除

`set_widget_properties` 示例：

```json
{
  "widget_name": "LoginCard",
  "properties": {
    "RenderOpacity": 0.95,
    "Slot": {
      "Padding": {"Left": 24, "Top": 16, "Right": 24, "Bottom": 16}
    }
  }
}
```

这只覆盖 `RenderOpacity` 和指定 Slot 字段，不会删除其他属性。

`reorder_widget_tree` 示例：

```json
{
  "root": "RootPanel",
  "tree": ["DialogLayer"]
}
```

如果 `RootPanel` 当前 children 为 `[Background, DialogLayer, HUD]`，执行后会变为 `[DialogLayer, Background, HUD]`。没有提及的 sibling 保持原相对顺序。

删除必须显式：

```json
{
  "widget_name": "DebugButton",
  "confirm_delete": true
}
```

未传 `confirm_delete=true` 时必须失败，并返回 Issue 15 风格错误。

## 隐藏兼容命令

以下命令不进入默认 MCP prompt 或 UMG ToolMode：

- `check_widget_overlap`
- `export_umg_to_json`
- `apply_json_to_umg`
- `apply_html_to_umg`

`check_widget_overlap` 是诊断工具；`export_umg_to_json` / `apply_json_to_umg` 是完整 JSON 兼容路径，容易让 AI 过度依赖全量快照或误判为替换式写入。默认流程应优先使用 Target 化的读写工具和 `apply_layout`。
