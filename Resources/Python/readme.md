# UmgMcp Python Servers

This directory contains the Python implementation of the Model Context Protocol (MCP) servers for the UmgMcp Unreal Engine plugin.

## Server Entry Points

We have split the functionality into two specialized servers:

1.  **`UmgMcpServer.py`**: The core server for Widget manipulation, Layouts, and general Context.
2.  **`UmgSequencerServer.py`**: The specialized server for Animation and Sequencer operations.

## Directory Structure

The Python code is organized to mirror the C++ plugin structure:

*   **`Animation/`**: Contains `UMGSequencer.py` for Sequencer and Animation tools.
*   **`Blueprint/`**: Contains `UMGBlueprint.py` for Blueprint creation and compilation tools.
*   **`Widget/`**: Contains `UMGSet.py`, `UMGGet.py` for core Widget manipulation (Create, Modify, Query).
*   **`FileManage/`**: Contains `UMGAttention.py` (Context Management) and `UMGFileTransformation.py` (Export/Import).
*   **`Editor/`**: Contains `UMGEditor.py` for Editor-level tools (Asset Registry, Level Actors).
*   **`Bridge/`**: Contains utilities like `UMGHTMLParser.py` and `read_unreal_log.py`.
*   **`APITest/`**: Contains test scripts and demos (e.g., `Gemini_Demo_ShowOff.py`).

## Tool Categories

### Server 1: UmgMcp (Widget & Core)

**Sensing (Widget)**
*   `get_widget_tree`
*   `query_widget_properties`
*   `get_layout_data`
*   `check_widget_overlap`

**Action (Widget)**
*   `create_widget`
*   `set_widget_properties`
*   `delete_widget`
*   `reparent_widget`
*   `save_asset`
*   `apply_layout`
*   `apply_json_to_umg` (Deprecated)

**Context (FileManage)**
*   `get_target_umg_asset`
*   `set_target_umg_asset`
*   `get_last_edited_umg_asset`
*   `get_recently_edited_umg_assets`

**Introspection (Widget)**
*   `get_widget_schema`
*   `get_creatable_widget_types`

*(Note: Editor & Blueprint tools are currently disabled in the server code)*

### Server 2: UmgSequencer (Animation)

**Attention (Context)**
*   `set_animation_scope`
*   `set_widget_scope`

**Read (Sensing)**
*   `get_all_animations`
*   `get_animation_keyframes`
*   `get_animated_widgets`
*   `get_animation_full_data`
*   `get_widget_animation_data`

**Write (Action)**
*   `create_animation`
*   `delete_animation`
*   `set_property_keys`
*   `remove_property_track`
*   `remove_keys`