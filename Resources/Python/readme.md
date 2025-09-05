# UMG MCP Python Modules

This directory contains the Python modules that form the client-side library and server definitions for the UMG MCP.

## Modules

- **`UmgMcpServer.py`**: The main FastMCP server that exposes all UMG manipulation tools to the AI.
- **`UMGAttention.py`**: A client module for tools related to understanding the user's focus and the editor's context.
- **`UMGGet.py`**: A client module for tools related to "Sensing" or reading data from UMG assets.
- **`UMGSet.py`**: A client module for tools related to "Action" or writing/modifying UMG assets.

## Attention API Clarification

The tools provided by `UMGAttention.py` are crucial for creating a fluid, context-aware interaction.

- **`get_active_umg_context()`**: This is the primary tool for the AI to determine what the user is currently working on. It returns the asset path of the UMG Blueprint that is currently open and focused in the Unreal Editor. This should be the default target for any ambiguous commands (e.g., "change this button").

- **`set_attention_target(asset_path)`**: This allows the AI to programmatically set its own focus. If the AI needs to work on a specific UMG asset that the user is not currently editing, it can use this tool to make that asset the new "default target" for subsequent commands.

- **Other Tools**: `get_last_edited_umg_asset()` and `get_recently_edited_umg_assets()` serve as fallbacks to regain context if the user is no longer focused on any specific UMG editor.