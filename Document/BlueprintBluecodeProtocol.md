# Blueprint Bluecode Protocol Draft

本文记录新的 Blueprint MCP 方向。范围限定为 UE 5.8；当前实现仍是过渡期节点级 MCP，bluecode 尚未落地。

## 为什么需要 bluecode

旧 Blueprint MCP 的主要问题：

1. 需要频繁逐节点调用 MCP，交互成本高。
2. `get_function_nodes` 返回节点 ID、类名等调试信息，但缺少执行语义、参数语义和数据依赖压缩。
3. `add_step`/`prepare_value`/`connect_data_to_pin` 把 AI 暴露在节点和 pin 细节里，信息密度低。
4. 旧 `delete_node`、`delete_variable` 是直接删除命令，不符合 Issue 15 的删除困难原则；默认 MCP 应隐藏它们，直到删除协议改为显式 `confirm_delete=true`。

bluecode 的目标不是一次替换所有后端，而是像 HLSL 替代 Material 图编辑一样，先定义高层协议，再逐步迁移。

## Target 层级

Target 继承关系：

1. `set_target_umg_asset`：当前 UMG/Widget Blueprint 资产。
2. `set_target_widget`：当前控件，可用于优先匹配 `Widget.Event` 或绑定函数。
3. `bluecode_set_function`：当前函数、事件或图。

`bluecode_set_function` 的目标格式建议：

```text
FunctionName
WidgetName.EventName
/Game/UI/W_Login:EventGraph
/Game/UI/W_Login:LoginButton.OnClicked
```

缺省规则：

- 未给资产路径时使用 Active UMG Target。
- 存在 Widget Target 时，优先匹配该控件的事件或绑定函数。
- 匹配多个候选时必须失败，并返回候选列表。
- 设置函数后，逻辑入口称为 `main`，空结束称为 `end`，返回称为 `return`，分支块使用稳定标签。

## 读语义

建议工具：

| Tool | 作用 |
| --- | --- |
| `bluecode_read_function(detail?)` | 以代码式摘要读取当前函数。默认返回执行流和必要数据依赖；`detail="debug"` 时才返回节点 ID 和 pin。 |
| `bluecode_read_variables()` | 读取变量、类型、默认值和引用摘要。 |
| `bluecode_read_events()` | 读取当前 UMG Target 下可编辑事件和已绑定事件。 |

默认 `bluecode_read_function` 应输出类似：

```text
main()
  PrintString("Welcome")
  if IsValid(LoginButton):
    SetVisibility(LoginPanel, Visible)
  end
```

返回 JSON 同时带结构化信息：

```json
{
  "function": "LoginButton.OnClicked",
  "entry": "main",
  "exit": "end",
  "code": "main()\n  PrintString(\"Welcome\")\n  end",
  "exec_paths": 1,
  "data_dependencies": [
    {"name": "LoginPanel", "kind": "widget_reference"}
  ],
  "debug_nodes_available": true
}
```

## 写语义

建议工具：

| Tool | 作用 |
| --- | --- |
| `bluecode_apply(code, anchor?, mode?)` | 并集式应用代码片段。默认插入到当前函数 `end` 前。 |
| `bluecode_apply_variables(variables)` | 并集式添加/更新变量，不删除未提及变量。 |
| `bluecode_connect(connects)` | 处理少量显式数据依赖或 pin 连接，作为 escape hatch。 |
| `bluecode_compile()` | 编译当前 Blueprint，返回紧凑诊断。 |

写入原则：

- 写操作只能追加或覆盖匹配到的节点、变量、连接。
- 如果输入片段省略了既有节点，不视为删除。
- 如果片段与既有节点相似，应按匹配策略复用或在右侧插入，并返回 `merge_report`。
- 无法连接的数据节点应先尝试转换为参数表达式，再尝试 cast，仍失败时放入 `floating_nodes`，并在返回中解释。

匹配优先级建议：

1. 明确 node id 或 stable label。
2. 函数名、成员类、目标对象和参数完全一致。
3. 函数名和关键参数一致。
4. 节点类型一致且邻接节点相似。
5. 无法确认时追加新节点，不覆盖旧节点。

示例：

已有：

```text
main()
  PrintString("one")
  PrintString("three")
  end
```

输入：

```text
PrintString("two")
```

结果：

```text
main()
  PrintString("one")
  PrintString("three")
  PrintString("two")
  end
```

如果输入：

```text
main()
  PrintString("two")
  PrintString("three")
  end
```

结果：

```text
main()
  PrintString("one")
  PrintString("two")
  PrintString("three")
  end
```

`PrintString("three")` 被匹配为既有节点，`PrintString("two")` 插入到它左侧。

## 删除语义

建议工具：

| Tool | 作用 |
| --- | --- |
| `bluecode_delete(targets, confirm_delete=true)` | 删除节点、变量或连接。必须显式确认。 |

删除必须满足：

- 未传 `confirm_delete=true` 时失败。
- target 必须是稳定 ID、稳定 label、变量名或连接描述。
- 匹配多个对象时失败，并返回候选。
- 删除返回 `deleted_count`、`affected_edges` 和 `new_flow_summary`。

## 当前过渡工具

当前默认 Blueprint MCP 只保留：

- `set_edit_function`
- `add_step`
- `prepare_value`
- `connect_data_to_pin`
- `add_variable`
- `get_variables`
- `get_function_nodes`
- `set_cursor_node`
- `search_function_library`
- `compile_blueprint`

隐藏兼容命令：

- `delete_node`
- `delete_variable`

这些删除命令后端仍存在，但默认 MCP prompt 和 Blueprint ToolMode 不暴露。它们需要在后续实现 `confirm_delete=true` 后再重新评估。

## 下一步实现建议

1. 先实现 `bluecode_read_function`，把当前节点图压缩成代码式摘要。
2. 再实现 `bluecode_apply`，内部可复用现有 `add_step`、`prepare_value`、`connect_data_to_pin`。
3. 最后实现 `bluecode_delete`，替代旧节点级删除。
4. 保留旧节点工具作为 backend compatibility，逐步从默认 prompts 中移除。
