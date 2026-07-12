# FabServer 删除清单

> 依据：`FabServer_SerialFlow.md` 与 `.github/copilot-instructions.md`
>
> 原则：只保留符合“send 在 AIProvider、chat/MCP 在 chat/agent、task_begin/task_end 归 task 系统”的代码。其余直接删除，不做兼容性保留。

## 必删项

1. Provider 内部的工具执行闭环
- 任何在 AIProvider 中直接解析 function call 并立即执行工具的代码。
- 任何 provider 内部递归调用自身来继续处理 tool call 的代码。
- 任何把 `task_begin` / `task_end` 当作 provider 侧普通工具执行的代码。

2. Provider 内部的 task 强hold
- 任何 provider 直接调用 `TaskProtocolToolExecutor` 的代码。
- 任何 provider 自己决定 task begin / task end / agent 切换的代码。

3. 混入 provider 的聊天状态控制
- 任何 provider 维护 `thinking / wait MCP / MCP running / completed` 这类 UI 状态的代码。
- 任何 provider 维护普通消息审批队列的代码。

4. 非文档化的中间层强hold
- 任何不在 `FabServer_SerialFlow.md` 节点表中的中间 executor、dispatcher、batch executor。
- 任何把普通 MCP 与 task_begin/task_end 合并到同一审批队列的代码。

5. 残余耦合引用
- 已删除/已废弃符号的残余调用、包含和绑定。
- 与当前架构无关的“兼容性”、旧流程、重复参数、重复状态字段。

## 删除顺序

1. 先删 provider 侧直执行工具链。
2. 再删 task 强hold 执行入口。
3. 然后复查 chat window / session surface / subsystem 的残余引用。
4. 最后编译验证。
