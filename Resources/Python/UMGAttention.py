
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

# Note: The main server (UmgMcpServer.py) will handle mcp.run()
