# UE5-UMG-MCP 🤖📄

**一个以版本控制为核心的AI协同UMG工作流**

![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)![Status: In Development](https://img.shields.io/badge/status-in%20development-orange.svg)![Built with AI](https://img.shields.io/badge/Built%20with-AI%20Assistance-blueviolet.svg)

本项目提供了一个强大的、由命令行驱动的工作流，用于管理虚幻引擎的UMG界面资产。它通过将**人类可读的 `.json` 文件作为唯一信源 (Source of Truth)**，从根本上解决了在Git中对二进制 `.uasset` 文件进行版本控制的难题。

受到 `blender-mcp` 等工具的启发，本系统允许开发者、UI设计师以及AI助手以程序化的方式与UMG资产互动，从而实现真正的Git协同、自动化UI生成与迭代。

### 核心理念：专注与可追溯性

*   **专注**: 我们只关心UMG。本项目致力于成为UE中管理UI资产的最优解，而不涉足其他领域。
*   **可追溯性**: 所有的UI变更都通过修改`.json`文件完成。这意味着每一次对齐、颜色或布局的调整，都可以清晰地在Git历史中被审查(Review)、合并(Merge)和回滚(Revert)。

### 核心工作流

工作流围绕两个核心操作展开，由一个中央管理脚本 (`mcp.py`) 负责调度：

1.  **导出 (UMG -> JSON):** 一个UE Python脚本读取编辑器中已有的 `.uasset` 文件，并将其控件层级和属性“反编译”为一个结构化的 `.json` 文件。这个`.json`文件是你需要提交到Git的。
2.  **应用 (JSON -> UMG):** 一个UE Python脚本读取`.json`文件，并在编辑器内部以程序化的方式创建或覆盖一个`.uasset`文件。这是将你的版本控制文本文件“编译”回引擎可用资产的步骤。

---

### Current Technical Architecture Overview

The system now primarily relies on the `UE5_UMG_MCP` plugin for communication between external clients (like this CLI) and the Unreal Engine Editor.

*   **Client-Server Model:** The `UE5_UMG_MCP` plugin embeds a TCP server within the Unreal Engine Editor. External clients connect to this server to send commands and receive responses.
*   **Configuration:** The server's host and port are now configurable via `UmgMcpConfig.h`, allowing for flexible deployment and testing scenarios (e.g., local or remote connections).
*   **Command Handling:** Commands received by the server are routed to specialized handlers (`FUmgMcpEditorCommands`, `FUmgMcpBlueprintCommands`, `FUmgMcpCommonUtils`) responsible for executing specific operations within the Unreal Editor (e.g., actor manipulation, Blueprint creation, material management).
*   **UMG Focus:** The plugin is specifically named `UE5_UMG_MCP` to indicate its primary focus on Unreal Motion Graphics (UMG) related operations, although it currently includes broader editor functionalities.

**Architecture Diagram:**

```mermaid
flowchart TD
    subgraph External Client (Gemini CLI)
        A[User Input/Tool Call] --> B{JSON Command}
    end

    subgraph Unreal Engine Editor
        subgraph UE5_UMG_MCP Plugin
            C[TCP Server (UUmgMcpBridge)]
            D{Command Dispatcher}
            E[C++ Command Handlers]
            F[Python Scripting Layer]
            G[Unreal Engine Core API]

            C -- Receives JSON --> D
            D -- Dispatches --> E
            D -- Dispatches --> F
            E -- Interacts with --> G
            F -- Calls C++ Functions --> G
            F -- Executes Python Logic --> G
        end

        H[Unreal Editor Environment]
        I[Unreal Python API]

        D -- Queries for Tools --> H
        H -- Exposes C++ BlueprintCallable --> I
        I -- Exposes Python Functions --> H

        C -- Listens on Configured Host/Port --> H
    end

    B -- Sends via TCP/IP --> C
    E -- Returns JSON --> C
    F -- Returns JSON --> C
    C -- Sends JSON Response --> B
```