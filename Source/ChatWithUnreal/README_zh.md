# ChatWithUnreal

`ChatWithUnreal` 是一个专为 **虚幻引擎 (Unreal Engine)** 深度定制的、兼顾高性能与极佳视觉美感的 **AI 对话交互界面 (Slate Chat UI) 插件**。

与传统的纯文本对话框不同，本项目在底层架构与交互逻辑上完全面向 **AI Agent 协同** 场景设计，深度集成了任务执行节点、工具链状态机以及交互审批流。

---

## 核心特性与功能介绍

`ChatWithUnreal` 提供了整套开箱即用的模块化 UI 组件，涵盖了 AI 大模型对话交互中所需的全套核心板块：

### 1. 欢迎页与历史记录管理 (Welcome & Session History)
- **`SChatWelcome` / `SChatWelcomeHeader`**：设计优雅的欢迎看板，直观地展示系统的就绪状态。
- **`SChatHistoryItem`**：支持多会话上下文历史列表管理，允许用户便捷地回顾与切换过去的 AI 交互流程。

### 2. 状态顶栏 (Top Info Bar)
- **多角色头像**：集成用户头像 (`SUserAvatar`) 与 Agent 助手头像 (`SAgentAvatar`)，直观显示对话主体。
- **动态编辑名称 (`SEditableName`)**：允许用户在 UI 中直接轻量化修改名称标识。
- **安全认证状态 (`SConfigByAuthentication`)**：根据当前的账户认证令牌状态，自适应展示对应的偏好配置面板入口。

### 3. 多功能交互底栏 (Bottom Input Bar)
- **多模态输入 (`SChatInput` / `SChatSendButton`)**：支持富文本多行输入、快捷键响应、发送状态指示等。
- **附件装载队列 (`SAttachmentList`)**：支持在对话中附加文件、数据资产、引用上下文等内容，并提供可视化的待上传列表。
- **额度余量可视化 (`SQuotaBar`)**：实时追踪显示 API 额度与 Token 消耗状态，保持预算透明透明。
- **多维度模式选择器**：
  - **`SInteractionModeSelector`**：交互模式选择，支持单次应答、连续对话或后台静默任务。
  - **`SToolModeSelector`**：工具触发控制，控制大模型自动调用、手动确认或禁用工具。
  - **`SAbilitiesSelector`**：Agent 能力集开关（如联网检索、静态分析、文件操作等）。

### 4. 高级消息枢纽与智能体响应群组 (Message Hub & Agent Interaction Response)
- **`SMessageInteractionHub`**：高性能的滚动对话面板，平滑承载丰富多样的气泡卡片。
- **多维度消息卡片**：
  - **用户消息气泡 (`SUserMessageWidget`)**：美观的用户侧文本气泡。
  - **系统通知栏 (`SSystemNotificationWidget`)**：渲染流式错误、提示或底层状态日志。
  - **任务隔离节点 (`STaskBeginNode` / `STaskEndNode`)**：可视化标注大任务的“开始执行”与“结束汇总”，方便追踪长链路任务。
- **Agent 专属智能应答卡片群组 (`SAgentResponseGroup`)**：
  - **状态控制栏 (`SAgentStatusBar`)**：直观展示 Agent 当前是否在思考（Thinking）、生成（Writing）或执行（Executing）。
  - **工具执行块 (`SToolExecutionBlock` / `SMcpToolItem`)**：当 Agent 决定调用 C++ 外部工具或外部 MCP 服务时，卡片内会自动嵌套可视化的工具执行日志及单项执行进度。
  - **智能审批队列 (`SMcpApprovalQueue`)**：**业界领先的“人机对齐”安全审批机制**！当 Agent 尝试执行敏感操作（如文件修改、命令运行）时，会弹出的可视化交互确认框，由开发者一键决定 `Approve (允许)` 或 `Reject (拒绝)`，兼顾自动化与安全性。

---

## 架构说明

本项目完全采用虚幻引擎底层的 **Slate (C++)** 框架编写，摒弃了高开销的 UMG/Webview 方案，具备如下架构优势：
1. **极致性能**：纯 Slate 渲染，即使渲染成百上千条复杂的富文本和嵌套控制块，依然保持 60+ FPS 的丝滑滚动。
2. **完全解耦**：彻底与底层具体的大模型通信协议剥离，仅通过声明式数据接口（Data Bindings / Subsystems）进行交互，可极速接入任何 AI Subsystem 或自定义网络通道。
3. **Winyunq 标准**：遵循 PascalCase 命名法则，完全无下划线，全量 C++ 模块级导出，易于二次扩展开发。

---

## 许可证 (License)

本项目基于 **[MIT License](LICENSE)** 协议开源。您可以自由地在商业或非商业的虚幻引擎项目中使用、修改和分发本项目。
