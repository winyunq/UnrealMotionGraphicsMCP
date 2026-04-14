[中文版点击这里](Readme_zh.md)

# UE5-UMG-MCP 🤖📄

**A Version-Controlled AI-Assisted UMG Workflow**

![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)![Status: Experimental](https://img.shields.io/badge/status-experimental-red.svg)![Built with AI](https://img.shields.io/badge/Built%20with-AI%20Assistance-blueviolet.svg)[![AgentSeal MCP](https://agentseal.org/api/v1/mcp/https-githubcom-winyunq-unrealmotiongraphicsmcp/badge)](https://agentseal.org/mcp/https-githubcom-winyunq-unrealmotiongraphicsmcp)

[Demo Designed A RTS UI](https://youtu.be/O86VCzxyF5o)

[Demo Recreating the UE5 editor](https://youtu.be/h_J70I0m4Ls)

[Demo Recreating the UE5 editor in UMG editor](https://youtu.be/pq12x2MH1L4)

[Chat with Gemini 3 to editor the UMG file](https://youtu.be/93_Fiil9nd8)

---

### 🛍️ Looking for an easier Setup? Try the Fab Version!

If you find manual plugin configuration and MCP environment setup too cumbersome, check out our commercial version on **Fab**:
[**UMG MCP on Fab Marketplace**](https://www.fab.com/zh-cn/listings/f70dbcb0-11e4-46bf-b3f7-9f30ba2c9b71)

**Differences between Fab and Open Source versions:**
- **Out-of-the-Box**: The Fab version installs directly via the Unreal Engine launcher, eliminating the need for manual cloning and Python environment setup.
- **Context Compression**: The Fab version includes advanced context management, automatically filtering redundant MCP history to maximize available tokens for the AI.
- **Commercial Focus**: While the Open Source version is our protocol testbed, the Fab version is optimized for production business logic. Commercial users receive prioritized support for their specific integration challenges.
- **Maintained Parity**: We remain committed to open source; all core logic improvements will continue to be synced to this GitHub repository.

---

### 🚀 Quick Start

This guide covers the two-step process to install the `UmgMcp` plugin and connect it to your Gemini CLI.

*   **Prerequisite:** Unreal Engine 5.6 or newer.

#### 1. Install the Plugin

1.  **Navigate to your project's Plugins folder:** `YourProject/Plugins/` (create it if it doesn't exist).
2.  **Clone the repository** directly into this directory:

    ```bash
    git clone https://github.com/winyunq/UnrealMotionGraphicsMCP.git UmgMcp
    ```

3.  **Restart the Unreal Editor.** This allows the engine to detect and compile the new plugin.

#### 2. Connect the Gemini CLI

Tell Gemini how to find and launch the MCP server.

1.  **Edit your `settings.json`** file (usually located at `C:\Users\YourUsername\.gemini\`).
2.  **Add the tool definition** to the `mcpServers` object.

    ```json
    "mcpServers": {
      "UmgMcp": {
        "command": "uv",
        "args": [
          "run",
          "--directory",
          "D:\\Path\\To\\YourUnrealProject\\Plugins\\UmgMcp\\Resources\\Python",
          "UmgMcpServer.py"
        ]
      }
    }
    ```

    **IMPORTANT:** You **must** replace the path with the correct **absolute path** to the `Resources/Python` folder from the cloned repository on your machine.

That's it! When you start the Gemini CLI, it will automatically launch the MCP server in the background.

#### Testing the Connection

After restarting your Gemini CLI and opening your Unreal project, you can test the connection by calling any tool function:

```python
  cd Resources/Python/APITest
  python UE5_Editor_Imitation.py
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

### 🧪 Experimental: Gemini CLI Skill Support

We are experimenting with the **Gemini CLI Skill** system as an alternative to the standard MCP approach. 
The Skill architecture allows the Python tools to be loaded directly by the CLI runtime, potentially **optimizing context usage** by dynamically enabling/disabling tools via `prompts.json` and avoiding the overhead of managing a separate MCP server process.

> **Note**: The MCP server (configured above) is still the stable and recommended way to use this plugin. Use Skill mode if you want to test the latest integration capabilities.

#### Configuration (Skill Mode)

To enable Skill mode, add the following to your `settings.json` (replacing `<YOUR_PROJECT_PATH>`):

```json
  "skills": {
    "unreal_umg": {
      "path": "<YOUR_PROJECT_PATH>/Plugins/UmgMcp/Resources/Python/UmgMcpSkills.py",
      "type": "local",
      "description": "Direct control of Unreal Engine UMG via Python Skills. Auto-loads tools from prompts.json."
    }
  },
```

---

## English

This project provides a powerful, command-line driven workflow for managing Unreal Engine's UMG UI assets. By treating **human-readable `.json` files as the sole Source of Truth**, it fundamentally solves the challenge of versioning binary `.uasset` files in Git.

Inspired by tools like `blender-mcp`, this system allows developers, UI designers, and AI assistants to interact with UMG assets programmatically, enabling true Git collaboration, automated UI generation, and iteration.

---

## Prompt Manager

A visual web tool for configuring system instructions, tool descriptions, and user prompt templates.

### Features

1.  **System Instruction Editor**: Modify the global instructions for the AI context.
2.  **Tool Management**:
    *   **Enable/Disable**: Toggle specific MCP tools on or off. Disabled tools are not registered with the MCP server, effectively **compressing the context window** to prevent AI distraction.
    *   **Edit Descriptions**: Customize tool descriptions (prompts) to better suit your workflow.
3.  **User Templates (Prompts)**: Add reusable prompt templates for quick access by the MCP client.

### How to Run

Execute the following command in your Python environment:
```bash
python Resources/Python/PromptManager/server.py
```
The browser will automatically open `http://localhost:8085`。

### Usage Tips

Prompts are crucial for AI tool effectiveness. Use the Prompt Manager to tailor the AI's behavior:

*   **One-Click Deployment Mode**: If you want the AI to focus solely on generating UI from design, disable all tools except `apply_layout` and `export_umg_to_json`.
*   **Tutor Mode**: If you want the AI to guide you without making changes, keep only read-only tools (e.g., `get_widget_tree`, `get_widget_schema`).
*   **Context Optimization**: For models with smaller context windows, disable tools you aren't currently using to improve speed and accuracy.

Contributions of effective prompt configurations are welcome!

---

### AI Authorship & Disclaimer

This project has been developed with significant assistance from **Gemini, an AI**. As such:
*   **Experimental Nature**: This is an experimental project. Its reliability is not guaranteed.
*   **Commercial Use**: Commercial use is not recommended without thorough independent validation and understanding of its limitations.
*   **Disclaimer**: Use at your own risk. The developers and AI are not responsible for any consequences arising from its use.

---

### Current Technical Architecture Overview

The system now primarily relies on the `UE5_UMG_MCP` plugin for communication between external clients (like this CLI) and the Unreal Engine Editor.

**Architecture Diagram:**

```mermaid
flowchart LR
    subgraph "Local Execution Environment"
        CLI["Gemini CLI"] --"StdIO (JSON-RPC)"--> PY["Python (UmgMcpServer.py)"]
    end

    subgraph "Unreal Engine 5"
        PY --"TCP Socket (JSON)"--> TCP["UmgMcpBridge (C++)"]
        TCP --> API["Unreal API / UMG"]
    end
```

## API Status

| Category                    | API Name                         | Status |
| --------------------------- | -------------------------------- | :----: |
| **Context & Attention**     | `get_target_umg_asset`           |   ✅    |
|                             | `set_target_umg_asset`           |   ✅    |
|                             | `get_last_edited_umg_asset`      |   ✅    |
|                             | `get_recently_edited_umg_assets` |   ✅    |
| **Sensing & Querying**      | `get_widget_tree`                |   ✅    |
|                             | `query_widget_properties`        |   ✅    |
|                             | `get_creatable_widget_types`     |   ✅    |
|                             | `get_widget_schema`              |   ✅    |
|                             | `get_layout_data`                |   ✅    |
|                             | `check_widget_overlap`           |   ✅    |
| **Actions & Modifications** | `create_widget`                  |   ✅    |
|                             | `delete_widget`                  |   ✅    |
|                             | `set_widget_properties`          |   ✅    |
|                             | `reparent_widget`                |   ✅    |
|                             | `save_asset`                     |   ✅    |
| **File Transformation**     | `export_umg_to_json`             |   ✅    |
|                             | `apply_json_to_umg`              |   ✅    |
|                             | `apply_layout`                   |   ✅    |
 
 ## UMG Blueprint API Status (New)
 
 | Category                    | API Name                  | Status | Description                                                                                      |
 | --------------------------- | ------------------------- | :----: | ------------------------------------------------------------------------------------------------ |
 | **Context & Attention**     | `set_edit_function`       |   ✅    | Set the current edit context (Function/Event). Supports auto-creating Custom Events.             |
 |                             | `set_cursor_node`         |   ✅    | Explicitly set the "Cursor" node (Program Counter).                                              |
 | **Sensing & Querying**      | `get_function_nodes`      |   ✅    | Get nodes in **Current Context Scope** (Filtered to connected graph to avoid global noise).      |
 |                             | `get_variables`           |   ✅    | Get list of member variables.                                                                    |
 |                             | `search_function_library` |   ✅    | Search callable libraries (C++/BP). Supports Fuzzy Search.                                       |
 | **Actions & Modifications** | `add_step(name)`          |   ✅    | **Core**: Add executable node by Name (e.g. "PrintString"). Auto-Wiring & Auto-Layout supported. |
 |                             | `prepare_value(name)`     |   ✅    | Add Data Node by Name (e.g. "MakeLiteralString", "GetVariable").                                 |
 |                             | `connect_data_to_pin`     |   ✅    | Connect pins precisely (Supports `NodeID:PinName` format).                                       |
 |                             | `add_variable`            |   ✅    | Add new member variable.                                                                         |
 |                             | `delete_variable`         |   ✅    | Delete member variable.                                                                          |
 |                             | `delete_node`             |   ✅    | Delete specific node.                                                                            |
 |                             | `compile_blueprint`       |   ✅    | Compile and apply changes.                                                                       |

## UMG Sequencer API Status

| Command                          | Status | Description                                                                                                      |
| :------------------------------- | :----: | :--------------------------------------------------------------------------------------------------------------- |
| `animation_target`               |   ✅    | Set/focus the current animation (alias of `set_animation_scope`, auto-creates when missing).                     |
| `widget_target`                  |   ✅    | Set/focus the current widget (alias of `set_widget_scope`).                                                      |
| `animation_overview`             |   ✅    | Returns keyframe counts, track counts, key times, and changed properties.                                        |
| `animation_widget_properties`    |   ✅    | Timeline view: per-widget property changes (ignores unanimated properties).                                      |
| `animation_time_properties`      |   ✅    | Time-slice view: property values at specific times (multi-time supported).                                       |
| `animation_append_widget_tracks` |   ✅    | Append/overwrite keys per widget+property (union only, no implicit deletion).                                    |
| `animation_append_time_slice`    |   ✅    | Append a diff-style time slice for multiple widgets at a given time.                                             |
| `animation_delete_widget_keys`   |   ✅    | Scoped delete for widget+property at specific times (`confirm_delete=true` required per Issue 15 safety policy). |
| `create_animation`               |   ✅    | Create or focus an animation with auto naming.                                                                   |
| `set_property_keys`              |   ✅    | Low-level track write helper (supports float/color/vector2D).                                                    |

Notes:
- `animation_target`/`widget_target` reuse the current UMG target asset; names are auto-corrected (no “animal” typo) and auto-create when missing.
- Write paths are union/overwrite only—no implicit deletion. Use `animation_delete_widget_keys` with `confirm_delete=true` for scoped removals.
- Responses now include counts/timeline context so every sequencer MCP returns actionable data.

## UMG Material API Status (New: The 5 Core Pillars Strategy)

| Category                | API Name                       |  Status   | Description                                                                                     |
| ----------------------- | ------------------------------ | :-------: | ----------------------------------------------------------------------------------------------- |
| **P0: Context**         | `material_set_target`          |     ✅     | **Anchor**: Specify target asset (path or parent). Required for stateful editing.               |
| **P1: Def & Query**     | `material_define_variable`     |     ✅     | Define external interface parameters (Def, not wire). Supports Scalar, Vector, Texture.         |
| **P2: Symbol Place**    | `material_add_node`            |     ✅     | **Drag Symbol**: Place a symbol from lib into graph and assign a unique instance handle.        |
|                         | `material_get_graph`           |     ✅     | Query existence and state of node instances in the graph.                                       |
| **P3: Connectivity**    | `material_connect_nodes`       |     ✅     | **Natural Connection**: Establish node-to-node functional flow (A -> B).                        |
|                         | `material_connect_pins`        |     ✅     | **Surgical Wiring**: Manually connect specific pins for complex topologies.                     |
| **P4: Lib Search**      | `material_search_library`      | 🚧 Planned | Search for available Material Expressions (symbols) and Functions.                              |
| **P5: Tactical Detail** | `material_set_hlsl_node_io`    |     ✅     | **Tactical Code**: Inject HLSL into Custom nodes and sync IO pins via JSON mapping.             |
|                         | `material_set_node_properties` |     ✅     | **Property Editing**: Set internal properties for regular nodes (e.g. Constant Value, Texture). |
| **Lifecycle**           | `material_compile_asset`       |     ✅     | Submit changes and analyze HLSL compilation errors.                                             |
| **Maintenance**         | `material_delete`              |     ✅     | Delete node instances or clean up logic by unique handle.                                       |
|                         | `material_get_pins`            |     ✅     | Introspect pins for a specific node handle.                                                     |

## UMG HLSL MCP API Status (New: Text-Edit Loop for UMG)

| Command             | Status | Description                                                                                                                           |
| ------------------- | :----: | ------------------------------------------------------------------------------------------------------------------------------------- |
| `hlsl_set_target`   |   ✅    | Lock/create HLSL target material. Validates UI-domain + single Custom node topology; can request confirmation before overwrite.     |
| `hlsl_get`          |   ✅    | Read current HLSL code and structured input parameters from the single Custom node.                                                  |
| `hlsl_set`          |   ✅    | Incremental update of HLSL and/or parameters. Deletion is explicit (`delete: true`) to avoid accidental destructive edits.           |
| `hlsl_compile`      |   ✅    | Compile current HLSL target and return concise diagnostics for AI post-processing.                                                   |

### HLSL Protocol Contract (UMG-Optimized)

- Material is treated as a single HLSL program.
- Backend assumes HLSL returns `float4`.
- Output auto-wiring is fixed: `.rgb -> FinalColor`, `.a -> Opacity`.
- Input parameters are returned as structured descriptors (`name`, `kind`, `source_handle`) for learning/replay by AI agents.
- `hlsl_set` is safety-biased: writing is easy, deletion must be explicit.

## UMG Style & Theming API Status (New)

| Category    | API Name             |  Status   | Description                                                                   |
| ----------- | -------------------- | :-------: | ----------------------------------------------------------------------------- |
| **Styling** | `set_widget_style`   | 🚧 Planned | Set detailed style (e.g. FButtonStyle) for a specific widget.                 |
| **Theming** | `apply_global_theme` | 🚧 Planned | Batch apply styles and fonts across multiple widgets based on a theme config. |
| **Assets**  | `style_create_asset` | 🚧 Planned | Create a standalone Slate Widget Style asset.                                 |

## Project Status Update

I'm sorry, due to Google's cancellation of my student discount, my free time to advance this project is limited. But don't worry, as this has forced me to use AIs from other platforms, which might actually accelerate the support for other platforms in this plugin. Specifically, our upcoming development plan is to add various API key access in the Fab version because I believe you are all tech-savvy, but those who truly need UMG are artists who may not have a technical background. In any case, if you could purchase a paid version of UMG MCP on Fab, that would be wonderful—maybe I could then directly subscribe to Gemini AI for development?
