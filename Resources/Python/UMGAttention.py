import logging
from typing import Dict, Any, Optional, List
from mcp.server.fastmcp import FastMCP

# Import configuration and connection utility from the main server file
from UmgMcpServer import get_unreal_connection, mcp

# Configure logging for this module
logger = logging.getLogger("UMGAttention")

# ==============================================================================
#  Category: Attention
#  AI Hint: When the user's request is ambiguous, use these tools first to find their focus.
# ==============================================================================

@mcp.tool()
def get_active_umg_context() -> Dict[str, Any]:
    """
    "What UMG is the user editing right now?" - Gets the asset path of the UMG Editor the user is currently focused on.
    AI HINT: This is your PRIMARY tool for understanding ambiguous commands like "change this button".
    """
    unreal = get_unreal_connection()
    return unreal.send_command("get_active_umg_context")

@mcp.tool()
def get_last_edited_umg_asset() -> Dict[str, Any]:
    """
    "What was the user just working on?" - Gets the asset path of the last UMG asset that was opened or saved.
    AI HINT: Use this as a fallback if 'get_active_umg_context' returns null (e.g., user switched to their code editor).
    """
    unreal = get_unreal_connection()
    return unreal.send_command("get_last_edited_umg_asset")

@mcp.tool()
def get_recently_edited_umg_assets(max_count: int = 5) -> Dict[str, Any]:
    """
    "What has the user been working on recently?" - Gets a list of recently edited UMG assets.
    AI HINT: Use this to offer suggestions if the context is completely lost.
    """
    unreal = get_unreal_connection()
    return unreal.send_command("get_recently_edited_umg_assets", {"max_count": max_count})

@mcp.tool()
def is_umg_editor_active(asset_path: Optional[str] = None) -> Dict[str, Any]:
    """
    "Is the user looking at a UMG editor?" - Checks if a UMG editor is the active window.
    AI HINT: Use this to decide if you need to refresh your data. If false, you might not need to poll for layout changes.
    """
    unreal = get_unreal_connection()
    params = {"asset_path": asset_path} if asset_path else {}
    return unreal.send_command("is_umg_editor_active", params)

@mcp.tool()
def set_attention_target(asset_path: str) -> Dict[str, Any]:
    """
    Sets the UMG asset that should be considered the current attention target.
    This allows programmatically setting the active UMG context.
    """
    unreal = get_unreal_connection()
    return unreal.send_command("set_attention_target", {"asset_path": asset_path})

# Note: The main server (UmgMcpServer.py) will handle mcp.run()
