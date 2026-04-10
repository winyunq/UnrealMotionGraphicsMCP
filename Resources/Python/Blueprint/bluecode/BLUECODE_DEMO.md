# Bluecode Demo（Blueprint ↔ JSON）

## 核心约定

- `main`：字符串数组，顺序即执行连线顺序。
- `branches`：对象，值为字符串数组；`key = anchorNodeId.pinName`。
- `end`：连空（不再连接后续节点）。
- `return`：连接回主路径下一节点。
- `connect_list`：默认不使用；仅在 `bluecode_read_function(include_connect_list=true)` 时返回底层细节。

---

## Demo 1：双分支 If

### Blueprint（语义）

- Main: `Entry -> Branch(IsEven) -> PrintString(AfterIf)`
- True: `PrintString(hi) -> return`
- False: `PrintString(fail) -> end`

### Bluecode JSON

```json
{
  "main": [
    "main",
    "Branch(IsEven(i))",
    "PrintString(\"AfterIf\")",
    "end"
  ],
  "branches": {
    "A_BRANCH_NODE_ID.Then": [
      "PrintString(\"hi\")",
      "return"
    ],
    "A_BRANCH_NODE_ID.Else": [
      "PrintString(\"fail\")",
      "end"
    ]
  }
}
```

---

## Demo 2：循环 + 多分支

### Blueprint（语义）

- Main: `Entry -> ForLoop(0..Count) -> PrintString(Done) -> return`
- LoopBody: `Branch(IsBoss(index))`
- Branch.Then: `PrintString(Boss) -> return`
- Branch.Else: `PrintString(Minion) -> return`
- ForLoop.Completed: `PrintString(Completed) -> end`

### Bluecode JSON

```json
{
  "main": [
    "main",
    "ForLoop(0, Count)",
    "PrintString(\"Done\")",
    "return"
  ],
  "branches": {
    "B_FORLOOP_NODE_ID.LoopBody": [
      "Branch(IsBoss(index))",
      "return"
    ],
    "B_FORLOOP_NODE_ID.Completed": [
      "PrintString(\"Completed\")",
      "end"
    ],
    "C_BRANCH_NODE_ID.Then": [
      "PrintString(\"Boss\")",
      "return"
    ],
    "C_BRANCH_NODE_ID.Else": [
      "PrintString(\"Minion\")",
      "return"
    ]
  },
  "floating_nodes": [
    "MakeLiteralString(\"debug\")"
  ]
}
```

---

## 读写行为（当前实现）

- `bluecode_read_function`：输出 `main + branches`，可选 `connect_list`。
- `bluecode_write_function`：**append-only**（不会删除节点），优先复用同名节点，其余增量追加。
- `bluecode_compile`：编译并返回紧凑结果。
