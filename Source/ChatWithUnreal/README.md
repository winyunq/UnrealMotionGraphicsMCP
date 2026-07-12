# ChatWithUnreal

`ChatWithUnreal` is a high-performance, visually stunning **Slate Chat UI plugin** deeply customized for **Unreal Engine**, specifically designed for **AI Agent collaboration** scenarios.

Unlike traditional text-only dialog widgets, this project is built from the ground up to support rich AI Agent interactions, featuring integrated task execution nodes, toolchain state visualization, and interactive manual approval workflows.

Looking for Chinese documentation? See [README_zh.md](README_zh.md).

---

## Core Features

`ChatWithUnreal` provides a comprehensive suite of modular Slate UI components tailored for state-of-the-art LLM and AI workflows:

### 1. Welcome Screen & Session History
- **`SChatWelcome` / `SChatWelcomeHeader`**: Elegant welcome dashboard providing system status and customizable headers.
- **`SChatHistoryItem`**: Multi-session context history list allowing users to seamlessly review, switch, and manage past AI conversations.

### 2. Top Info Bar
- **Multi-Role Avatars**: Integrated user (`SUserAvatar`) and agent assistant (`SAgentAvatar`) avatar representations.
- **Dynamic Name Editing (`SEditableName`)**: Allows users to quickly and lightweightly update profile names in-place.
- **Authentication Gateway (`SConfigByAuthentication`)**: Dynamically shows configuration and setting gateways based on active token authentication.

### 3. Multifunctional Bottom Input Bar
- **Multimodal Input (`SChatInput` / `SChatSendButton`)**: Multi-line rich text input, instant keyboard hotkeys, and dynamic send-state animations.
- **Attachment Upload Queue (`SAttachmentList`)**: Support for attaching files, UAssets, and other referencing data directly in the chat with interactive file queue lists.
- **Token Quota Visualizer (`SQuotaBar`)**: Real-time token consumption and API quota monitoring to keep costs transparent.
- **Multi-dimensional Mode Selectors**:
  - **`SInteractionModeSelector`**: Toggles between single-turn, continuous chat, or background daemon execution.
  - **`SToolModeSelector`**: Tool triggering policy (Auto, Manual confirmation, or Disable tools entirely).
  - **`SAbilitiesSelector`**: Toggles specific Agent tool capabilities (e.g. web search, file modification, code execution).

### 4. Advanced Message Hub & Agent Response Group
- **`SMessageInteractionHub`**: A high-performance, scrollable conversation viewport designed to render rich bubble widgets smoothly.
- **Adaptive Message Templates**:
  - **User Message Gas Bubble (`SUserMessageWidget`)**: Clean user-side message presentation.
  - **System Notifications (`SSystemNotificationWidget`)**: Streamed execution logs, error messages, and background status warnings.
  - **Task Segment Indicators (`STaskBeginNode` / `STaskEndNode`)**: Visually marks long-chain task start and summary endpoints.
- **Exclusive Agent Response Widget Pack (`SAgentResponseGroup`)**:
  - **Agent Status Bar (`SAgentStatusBar`)**: Real-time agent state indicators (Thinking, Writing, or Executing).
  - **Tool Execution Blocks (`SToolExecutionBlock` / `SMcpToolItem`)**: Displays tool-calling processes (such as C++ methods or external MCP services) with inline execution logs and itemized progress bars.
  - **Smart Approval Queue (`SMcpApprovalQueue`)**: **Industry-leading Human-in-the-Loop alignment mechanism**! Automatically displays a verification card for sensitive tool requests, allowing the developer to `Approve` or `Reject` with one click.

---

## Technical Architecture

Designed entirely in native **Slate (C++)** rather than heavy UMG/Webview implementations, `ChatWithUnreal` boasts exceptional performance:
1. **Uncompromised Performance**: High-density rich text rendering and deep nested widget hierarchies maintain a buttery-smooth **60+ FPS** scrolling experience.
2. **Loosely Coupled Design**: Completely decoupled from specific network/LLM communication layers, enabling developers to bridge the UI with any custom AI Subsystem or socket connection.
3. **Winyunq Standards**: Follows PascalCase naming guidelines with zero underscores, fully exporting C++ symbols for extreme extensibility.

---

## License

This project is open-sourced under the **[MIT License](LICENSE)**. You are free to modify, distribute, and integrate it in both commercial and non-commercial Unreal Engine projects.
