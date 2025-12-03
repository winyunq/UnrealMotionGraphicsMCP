# UMG MCP Plugin Source Architecture

此文档用于跟踪代码架构演进与开发进度。核心理念是建立一套符合"Winyunq"概念的、具有明确"语法"的上下文感知API。

## 核心设计理念 (Design Philosophy)

**1. "语法"与"上下文" (Grammar & Context)**
为了降低AI的认知负担并缩短上下文，我们采用"主语置顶"的设计模式。
- **默认主语**: 系统始终维护一个"当前关注对象"（Context）。
- **省略参数**: 当上下文确立后，后续操作默认作用于当前对象，无需重复传递参数。

**2. 工作记忆 (Working Memory)**
与其每次传递参数，不如维护一个"操作对象缓存"。
- **线性聚焦 (Drill-down)**: 操作通常是线性的：`Asset` -> `Animation` -> `Track` -> `Key`。
- **缓存即主语**: 当我们"编辑"了一个对象（如蓝图函数、动画序列），它就自动进入"工作记忆"。后续的指令（如"绑定回调"）应默认使用这个记忆中的对象。
- **序列化上下文**: 上下文不应只是单一指针，而是一个序列（History/Stack）。Index 0 是当前（Current/Temporary），Index 1 是默认（Default/Previous）。

## 模块清单 (Module Checklist)

### 1. 桥接与通信 (Bridge)
负责与外部MCP客户端建立TCP连接，分发指令。
- [x] `UmgMcpBridge` (Server Lifecycle, Socket Management)
- [x] `MCPServerRunnable` (Threaded Command Processing)
- [x] `UmgMcpCommonUtils` (Shared Utilities, JSON Parsing)

### 2. 上下文与注意力 (Context & Attention)
**核心模块**：负责维护"当前正在编辑谁"。
- [x] `UmgAttentionSubsystem` (Asset Context - 维护当前打开的UMG资产)
- [x] `UmgContextSubsystem` (Extended Context - 扩展上下文)
    - [x] `CurrentWidget` (当前选中的组件)
    - [x] `CurrentAnimation` (当前编辑的动画)
    - [x] `CurrentTrack` (当前操作的轨道)

### 3. 感知 (Sensing)
负责获取UMG的状态、层级和属性。
- [x] `UmgGetSubsystem` (Widget Tree, Properties)
- [x] `UmgMcpWidgetCommands` (Command Wrapper)
- [x] **Context Awareness**: 更新API以支持隐式目标。

### 4. 操作 (Manipulation)
负责修改UMG结构和属性。
- [x] `UmgSetSubsystem` (Create, Delete, Reparent, SetProperty)
- [x] **Context Awareness**: 更新API以支持隐式目标 (如 `SetProperty` 无需传 `WidgetName` 如果已选中)。

### 5. 动画与序列 (Animation/Sequencer)
负责UMG动画的创建与关键帧编辑。
- [x] `UmgMcpSequencerCommands` (Basic Structure)
- [x] **Refactor**: 重构为基于上下文的"K帧语法"。
    - [x] `SelectAnimation` (进入动画编辑模式)
    - [x] `SelectWidgetTrack` (选中特定组件的轨道)
    - [x] `AddKey` (在当前轨道打帧)

### 6. 文件与资源 (File & Assets)
负责资产的导入导出与转换。
- [x] `UmgFileTransformation` (JSON <-> UAsset)
- [x] `UmgMcpFileTransformationCommands`

### 7. 编辑器与蓝图 (Editor & Blueprint)
辅助性的编辑器操作。
- [x] `UmgMcpEditorCommands` (Spawn Actor, Level Query)
- [x] `UmgMcpBlueprintCommands` (Create BP, Add Component)

## 开发计划 (Roadmap)

1.  **完善上下文系统**: 扩展 `UmgAttentionSubsystem` 或新建 `UmgContextSubsystem`，使其能记住 Widget 和 Animation。
2.  **重构动画API**: 废弃冗余参数的API，实现 `Focus` -> `Action` 的流式操作。
3.  **Python端适配**: 更新 `UmgMcpServer.py` 以利用新的上下文能力，提供更自然的工具函数。
