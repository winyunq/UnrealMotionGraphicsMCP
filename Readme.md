[中文版请点击此处](Readme_zh.md)

# UE5-UMG-MCP 🤖📄

**A Version-Controlled AI-Assisted UMG Workflow**

![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)![Status: Experimental](https://img.shields.io/badge/status-experimental-red.svg)![Built with AI](https://img.shields.io/badge/Built%20with-AI%20Assistance-blueviolet.svg)

[![Project Background](Resources/Docs/project_background.png)](https://youtu.be/O86VCzxyF5o)

---

### 🚀 Quick Start



This guide covers the two-step process to install the `UmgMcp` plugin and connect it to your Gemini CLI.



*   **Prerequisite:** Unreal Engine 5.6 or newer.



2.  **Clone the repository** directly into this directory:

    ```bash

    git clone https://github.com/winyunq/UnrealMotionGraphicsMCP.git UmgMcp

    ```



3.  **Restart the Unreal Editor.** This allows the engine to detect and compile the new plugin.



#### 2. Connect the Gemini CLI



Tell Gemini how to find and launch the MCP server.



1.  **Edit your `settings.json`** file (usually located at `C:\Users\YourUsername\.gemini\`).

2.  **Add the tool definition** to the `tools` object.



    ```json

    "UmgMcp": {

      "command": "uv",

      "args": [

        "run",

        "--directory",

        "D:\\Path\\To\\YourUnrealProject\\Plugins\\UmgMcp\\Resources\\Python",

        "UmgMcpServer.py"

      ]

    }

    ```

    **IMPORTANT:** You **must** replace the path with the correct **absolute path** to the `Resources/Python` folder from the cloned repository on your machine.



That's it! When you start the Gemini CLI, it will automatically launch the MCP server in the background.



#### Testing the Connection



After restarting your Gemini CLI and opening your Unreal project, you can test the connection by calling any tool function:

```python

print(default_api.get_target_umg_asset())

```



#### Python Environment (Optional)



The plugin's Python environment is managed by `uv`. In most cases, it should work automatically. If you encounter issues related to Python dependencies (e.g., `uv` command not found or module import errors), you can manually set up the environment:



1.  Navigate to the directory: `cd YourUnrealProject/Plugins/UmgMcp/Resources/Python`

2.  Run the setup:

    ```bash

    uv venv

    .\.venv\Scripts\activate

    uv pip install -e .

    ```



---

## English

This project provides a powerful, command-line driven workflow for managing Unreal Engine's UMG UI assets. By treating **human-readable `.json` files as the sole Source of Truth**, it fundamentally solves the challenge of versioning binary `.uasset` files in Git.

Inspired by tools like `blender-mcp`, this system allows developers, UI designers, and AI assistants to interact with UMG assets programmatically, enabling true Git collaboration, automated UI generation, and iteration.

### Core Philosophy: Focus & Traceability

*   **Focus**: We only care about UMG. This project aims to be the optimal solution for managing UI assets in UE, without venturing into other domains.
*   **Traceability**: All UI changes are made by modifying `.json` files. This means every adjustment to alignment, color, or layout can be clearly reviewed, merged, and reverted in Git history.

### Core Workflow

The workflow revolves around two core operations, orchestrated by a central management script (`mcp.py`):\n\n1.\t**Export (UMG -> JSON):** A UE Python script reads existing `.uasset` files in the editor and "decompiles" their widget hierarchy and properties into a structured `.json` file. This `.json` file is what you commit to Git.\n2.\t**Apply (JSON -> UMG):** A UE Python script reads the `.json` file and programmatically creates or overwrites a `.uasset` file within the editor. This is the step where your version-controlled text file is "compiled" back into an engine-usable asset.

### Current Status & Limitations

*   **Functionality**: The current version supports `export_umg_to_json` (exporting UMG to JSON) and `apply_json_to_umg` (applying JSON to UMG).
*   **Known Issues**: Please be aware that applying JSON to UMG assets might still lead to unexpected behavior or crashes due to underlying plugin limitations or specific property handling. This is an experimental tool.



### AI Authorship & Disclaimer

This project has been developed with significant assistance from **Gemini, an AI**. As such:\n*   **Experimental Nature**: This is an experimental project. Its reliability is not guaranteed.
*   **Commercial Use**: Commercial use is not recommended without thorough independent validation and understanding of its limitations.
*   **Disclaimer**: Use at your own risk. The developers and AI are not responsible for any consequences arising from its use.

---

### Current Technical Architecture Overview

The system now primarily relies on the `UE5_UMG_MCP` plugin for communication between external clients (like this CLI) and the Unreal Engine Editor.

**Architecture Diagram:**

```mermaid
flowchart TD
    subgraph Client [External Client Gemini CLI]
        A[User Input/Tool Call] --> B{JSON Command}
    end

    subgraph Editor [Unreal Engine Editor]
            subgraph Plugin [UE5_UMG_MCP Plugin]
            C[TCP Server UUmgMcpBridge]
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


## API Status

| Category | API Name | Status |
|---|---|:---:|
| **Context & Attention** | `get_target_umg_asset` | ✅ |
| | `set_target_umg_asset` | ✅ |
| | `get_last_edited_umg_asset` | ✅ |
| | `get_recently_edited_umg_assets` | ✅ |
| **Sensing & Querying** | `get_widget_tree` | ✅ |
| | `query_widget_properties` | ✅ |
| | `get_creatable_widget_types` | ✅ |
| | `get_widget_schema` | ✅ |
| | `get_layout_data` | ✅ |
| | `check_widget_overlap` | ✅ |
| **Actions & Modifications** | `create_widget` | ✅ |
| | `delete_widget` | ✅ |
| | `set_widget_properties` | ✅ |
| | `reparent_widget` | ✅ |
| | `save_asset` | ✅ |
| **File Transformation** | `export_umg_to_json` | ✅ |
| | `apply_json_to_umg` | ✅ |
| | `apply_layout` | ✅ |

##  `apply_json_to_umg` is able to use.This is mean that you need guide AI do that: learing from `export_umg_to_json` ,using to `apply_json_to_umg`