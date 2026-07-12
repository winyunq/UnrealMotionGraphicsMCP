# Blueprint Bluecode Protocol Draft

本文记录新的 Blueprint MCP 方向。范围限定为 UE 5.8；当前实现已有 BlueCode MVP，旧节点级 MCP 仍作为后端兼容层保留。

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
| `bluecode_read_function(detail?, include_connections?)` | 以代码式摘要读取当前函数。默认返回执行流、必要数据依赖和 round-trip hints；`detail="debug"` 或 `include_connections=true` 时返回节点 ID、pin 和连接 manifest。 |
| `bluecode_read_variables()` | 读取变量、类型、默认值和引用摘要。 |
| `bluecode_read_events()` | 读取当前 UMG Target 下可编辑事件和已绑定事件。 |
| `bluecode_search_nodes(query?, category?, node_class?, node_class_path?, include_pins?)` | 搜索当前上下文可用的 Blueprint Action Menu 节点，返回稳定 `handle`，用于插件、自定义、宏或同名歧义节点。写入时可作为 `action_handle` 使用。 |

默认 `bluecode_read_function` 应输出类似：

```text
main()
  PrintString("Welcome")
  Score = A + B
  node("Set Timer by Event", Time=1.0)
  value("Select", Index=ActiveIndex)
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
  "action_hints": {
    "PrintString(\"Welcome\")": {
      "handle": "bpact:...",
      "node_class": "K2Node_CallFunction",
      "category": "Utilities|String"
    }
  },
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
| `bluecode_apply(code, anchor?, mode?, action_handles?, action_hints?, expression_hints?, action_hints_by_line?, node_aliases?, node_properties?)` | 并集式应用代码片段。默认插入到当前函数 `end` 前；可携带 `action_handle` 精确生成任意 Action Menu 节点；可直接复用读回的 `expression_hints`；可用 `alias=Name` 或 `node_aliases` 给本次新节点命名；可用 `node_properties` 写入非 pin 的 UObject 节点属性。 |
| `bluecode_apply_variables(variables)` | 并集式添加/更新变量，不删除未提及变量。 |
| `bluecode_connect(connects, aliases?)` | 并集式追加少量显式数据依赖或 pin 连接，作为复杂自定义节点 escape hatch；不删除已有连接；可传入 `bluecode_apply` 返回的 `aliases` 后使用 `NodeAlias:Pin`。 |
| `bluecode_compile()` | 编译并保存当前 Blueprint，返回紧凑诊断。 |

写入原则：

- 写操作只能追加或覆盖匹配到的节点、变量、连接。
- 如果输入片段省略了既有节点，不视为删除。
- 如果片段与既有节点相似，应按匹配策略复用或在右侧插入，并返回 `merge_report`。
- 无法连接的数据节点应先尝试转换为参数表达式，再尝试 cast，仍失败时放入 `floating_nodes`，并在返回中解释。
- 每个成功的 `bluecode_apply.operations[].result` 应返回 `nodeId`/`node_id`、`title`、`node_class`、`node_class_path`、`is_exec`、`pin_counts`、`inputs`、`outputs` 和 `input_warnings`，其中 `inputs`/`outputs` 使用与 `bluecode_read_function.action_hints` 相同的 pin evidence 格式；调用方可以直接用这些 endpoint 或返回的 `aliases` 继续 `bluecode_connect`，不必为了找到新节点 pin 立刻再读整图。

任意/自定义节点映射：

1. `bluecode_search_nodes` 从 UE Blueprint Action Menu 读取当前图上下文内可创建的节点。
2. 返回项包含稳定 `handle`/`action_handle`、显示名、类别、节点类、`signature`、`is_exec` 和精简 `pin_counts`；只有需要 pin 名时才打开 `include_pins` 获取模板 pin 签名。
3. `bluecode_apply` 的 `action_handles` 可用语句全文、归一化语句或调用名作为 key，value 是搜索结果 `handle`。
4. `bluecode_apply` 的 `action_hints` 可直接传搜索结果对象，或传 `action_handle`/`handle`、`category`、`node_class`、`node_class_path`、`signature`，用于更强约束；handle 失效时完整类路径与 signature 可作为精确 fallback。只有过期 handle 而没有额外证据时必须失败，不能退回到显示名猜测。
5. 标识符函数可直接写 `FunctionName(Pin=value, ...)`；显示名含空格/符号、插件节点、宏、自定义节点写 `node("Action Menu Name", Pin=value, ...)`。
6. 纯/数据节点写 `value("Action Menu Name", Pin=value, ...)`，由后端创建为非执行节点并按 pin 推断连接。
7. 如果后续需要显式连接新建节点，语句内可写 `alias=Name`，或通过 `node_aliases`/`aliases` map 按语句名指定别名；成功返回 `aliases={Name: NodeId}`，并在对应 operation result 中返回新节点的 pin evidence。
8. 同名节点无法唯一匹配时必须失败并返回候选，不能猜测。

复杂 pin 连线 escape hatch：

- 默认仍应优先用 `bluecode_apply` 的 `Pin=value` 语义输入，让后端按 pin 类型和目标上下文推断。
- `Pin=value` 的 pin 名可使用内部 `PinName`、编辑器显示名或 `FriendlyName`；后端会归一化空白与大小写。
- pin 值可传字符串、数字、布尔；复杂默认值可用对象形式 `{ "path": "/Game/..." }`、`{ "class": "/Script/..." }`、`{ "object": "..." }`、`{ "text": "..." }` 或 `{ "literal": "(X=0,Y=0)" }`。后端会通过 K2 schema 写入 Object/Class/Text/Enum/Struct 等默认值，失败时返回 `input_warnings`。
- 如果插件、自定义或宏节点的行为由节点 UObject 属性而不是 pin 控制，可在 `bluecode_apply` 传 `node_properties`。它的 key 与 `action_hints` 一样可用语句全文、归一化语句或节点名，value 是属性写入对象；也可把 `node_properties` 或 `properties` 直接放进对应 hint。属性写入通过 UE 反射执行，支持 bool/number/string/name/text/enum/object/class 与 `literal` import text；失败会返回 `node_properties.failures`。
- pin 值也可以是数据表达式，例如 `Target=value("Select", Index=ActiveIndex)`、`Target=PureFunction(A, B)` 或 `Target=A + B`；后端会创建纯/数据节点并连接兼容输出。String/Text/Name pin 只有显式 `value(...)` 才按节点表达式处理，普通 `Foo(Bar)` 会保留为文本默认值。
- 数据表达式里的内层节点同样可以使用 `action_hints` / `expression_hints` / `action_handles` 精确映射；例如 `Target=value("My Custom Pure", Input=A)` 可通过 key `My Custom Pure` 或完整表达式 key 提供 handle/signature，不能退回到显示名猜测。读回时，依赖纯/数据节点会出现在 `expression_hints` 中，同时也会按表达式文本和节点名写入 `action_hints`，下一次 `bluecode_apply` 可以把 `expression_hints` 原样传回。
- 数据表达式写入如果无法找到唯一兼容输出，会回滚本次表达式临时创建的节点，并在 `input_warnings` 返回原因；不会留下半成品表达式节点。
- 支持 UE 原生 `IK2Node_AddPinInterface` 的变参节点会在写入前按位置参数数量或动态命名输入自动增加 pin，例如 `Make Array` 的 `[0]`、`[1]`、多输入数学节点、部分可扩展节点。`[0]` 与 `0` 视为同一语义 pin。
- `Format Text` 节点会先写入 `Format` 文本并触发 UE 的参数 pin 生成，因此 `value("Format Text", Format="Hello {Name}", Name=PlayerName)` 可以用语义参数写入。
- 无法由 UE 原生规则新增 pin 的节点不能强行造 pin；后端应返回 `input_warnings`，提示需要调整节点类型、连接类型或使用显式 escape hatch。
- 当插件、自定义或宏节点的复杂 pin 无法推断时，使用 `bluecode_connect` 显式追加连接。
- 需要连接本轮刚创建的节点时，先在 `bluecode_apply` 中写 `alias=Name`，再把返回的 `aliases` 传给 `bluecode_connect`，连接端点可写 `Name:Pin`。
- `bluecode_connect` 的端点 pin 同样支持内部名、显示名或 `FriendlyName`；省略 source/output 或 target/input pin 时，只在唯一候选 pin 存在时自动解析，否则返回候选要求补充 pin 名。
- `bluecode_connect` 是写操作但仍是并集语义：已有连接视为成功；如果 UE 连接响应要求 break/replace 旧连接，则操作失败并要求先显式删除或迁移。

示例：

```json
{
  "connects": [
    {"source": "SelectTheme:Return Value", "target": "ApplyTheme:Theme"},
    "IsValid:Return Value -> Branch:Condition"
  ],
  "aliases": {
    "SelectTheme": "D7437C7449A4A52791F5A4A67B4DE0AB",
    "ApplyTheme": "4C0BA44F4C404C1F86B6DDA6A238E82F"
  }
}
```

示例：

```json
{
  "code": "node(\"My Custom Node\", Target=LoginButton, alias=ApplyTheme)",
  "action_handles": {
    "My Custom Node": "bpact:..."
  }
}
```

默认读取应优先从事件/函数入口沿 exec flow 输出语句，未连接的节点再按编辑器位置作为 floating 语句追加。被数据 pin 引用的纯节点、变量 getter、简单数学节点应内联成表达式；只有未被引用的纯节点才作为 `value(...)` 顶层语句出现。这样 read 回来的是可继续编辑的高密度语义，而不是噪声节点 dump。

`bluecode_read_function` 同时返回 `action_hints` 和 `expression_hints`，以语句或表达式文本为 key，value 包含可重建节点所需的 `handle`、`category`、`node_class`、`node_class_path`、`signature`、`node_id` 等信息。读取到自定义、插件、宏、同名节点或嵌套纯/数据节点后，应把这些对象原样传回 `bluecode_apply(action_hints=..., expression_hints=...)`，保证语义代码之外仍有一一映射证据。若存在重复语句，`action_hints` 的文本 key 会天然冲突，因此读回还会返回 `action_hints_by_line`；修改读回代码时应尽量一并传回 `bluecode_apply(action_hints_by_line=...)`，使相同语句也能按行/顺序保留精确映射。

当 `bluecode_apply` 收到 readback hint 中的 `node_id` / `target_node_id` 时，它应优先更新该既有节点的输入 pin/default，而不是追加重复节点。这适用于自定义/插件节点，也适用于基础的变量赋值节点和 Branch 节点。这是写操作“并集覆盖重复项”的具体语义：同一节点的已给定 pin 会被覆盖、追加兼容连接，或根据 `Pin=A+B` / `Pin=value("Select", ...)` 生成新的数据表达式节点并连接回来；未提及 pin、未提及连接和未提及节点不会被删除。若更新某个已连接 pin 会导致替换旧连接，必须失败并要求先用 `bluecode_delete(kind="connection")` 删除旧边。

当需要迁移、重连或删除边时，使用 `bluecode_read_function(detail="debug")` 或 `bluecode_read_function(include_connections=true)`。返回的 `connections` 是结构化边清单，每条边包含 `source`、`target`、`kind`、`connect` 和 `delete_target`。`connect` 可直接作为 `bluecode_connect` 的字符串形式参考；`delete_target` 可放入 `bluecode_delete(targets=[...], confirm_delete=true)`。这使任意节点的语义语句、Action Menu 证据、pin 证据和边证据分层存在，而不是把低层噪声混进默认 `code`。

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
- target 必须是稳定 ID、稳定 label、变量名或连接描述；连接可写 `{"kind":"connection","source":"NodeA:Then","target":"NodeB:Execute"}` 或 `"NodeA:Then -> NodeB:Execute"`。
- 删除连接时只断开已经存在的一条连接；如果两个节点之间有多条连接而 pin 信息不足，必须失败并返回候选，要求补 `source_pin` / `target_pin`。
- 匹配多个对象时失败，并返回候选。
- 删除返回 `deleted_count`、`operations` 和 `failures`；后续可继续扩展 `affected_edges` 与 `new_flow_summary`。

## 当前默认工具

当前默认 Blueprint MCP 暴露：

- `bluecode_set_function`
- `bluecode_read_function`
- `bluecode_read_variables`
- `bluecode_read_events`
- `bluecode_search_nodes`
- `bluecode_apply`
- `bluecode_apply_variables`
- `bluecode_connect`
- `bluecode_delete`
- `bluecode_compile`
- `search_function_library`

旧节点级工具只作为后端兼容层保留，不作为默认协议：

- `set_edit_function`
- `add_step`
- `prepare_value`
- `connect_data_to_pin`
- `add_variable`
- `get_variables`
- `get_function_nodes`
- `set_cursor_node`
- `compile_blueprint`

隐藏兼容删除命令：

- `delete_node`
- `delete_variable`

这些删除命令后端仍存在，但默认 MCP prompt 和 Blueprint ToolMode 不暴露。它们需要在后续实现 `confirm_delete=true` 后再重新评估。

## 下一步实现建议

1. 扩展 `bluecode_read_function` 的信息密度，确保默认读回是语义摘要，`detail="debug"` 才暴露节点/pin 细节。
2. 扩展 `bluecode_apply` 的语法覆盖面，并保持 action handle 路径可创建用户自定义节点、插件节点、宏节点。
3. 强化 `bluecode_delete` 的候选消歧和影响摘要，替代旧节点级删除。
4. 保留旧节点工具作为 backend compatibility，不再放入默认 prompts 或 Blueprint ToolMode。
