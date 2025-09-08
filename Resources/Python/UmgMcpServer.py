"""
UmgMcp Server - Your AI UMG Design Partner (v1.0 - Complete Contract)

This MCP server is designed exclusively for interacting with the UmgMcp plugin
in Unreal Engine 5. It provides a complete and verified suite of tools for an AI
assistant to sense, understand, and manipulate UMG assets programmatically.

This script is the definitive "source of truth" for the UmgMcp API. The Gemini CLI
will be built from this contract, and the Unreal Engine plugin MUST implement a
handler for every command defined herein.
"""

import logging
import socket
import json
from contextlib import asynccontextmanager
from typing import AsyncIterator, Dict, Any, Optional, List
from mcp.server.fastmcp import FastMCP

# Import configuration from our dedicated config file
from mcp_config import UNREAL_HOST, UNREAL_PORT

# Configure logging
logging.basicConfig(
    level=logging.INFO, # Use INFO for production, DEBUG for development
    format='%(asctime)s - %(name)s - %(levelname)s - [%(filename)s:%(lineno)d] - %(message)s',
    handlers=[ logging.FileHandler('umg_mcp_server.log') ]
)
logger = logging.getLogger("UmgMcpServer")

class UnrealConnection:
    """Manages the socket connection to the UmgMcp plugin running inside Unreal Engine."""
    def __init__(self, host: str, port: int):
        self.host, self.port, self.socket = host, port, None

    def connect(self) -> bool:
        try:
            if self.socket: self.socket.close()
            logger.info(f"Connecting to UmgMcp plugin at {self.host}:{self.port}...")
            self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.socket.settimeout(10)
            self.socket.connect((self.host, self.port))
            logger.info("Successfully connected to Unreal Engine (UmgMcp).")
            return True
        except Exception as e:
            logger.error(f"Failed to connect to UmgMcp plugin: {e}")
            self.socket = None
            return False

    def send_command(self, command_type: str, params: Dict[str, Any] = None) -> Optional[Dict[str, Any]]:
        if not self.connect():
            return {"status": "error", "message": "Failed to establish connection with the Unreal Engine plugin."}
        try:
            command_obj = {"type": command_type, "params": params or {}}
            command_json = json.dumps(command_obj)
            logger.debug(f"Sending command: {command_json}")
            self.socket.sendall(command_json.encode('utf-8'))
            buffer = b""
            while True:
                chunk = self.socket.recv(8192) # Increased buffer size
                if not chunk: break
                buffer += chunk
                try:
                    response = json.loads(buffer.decode('utf-8'))
                    logger.debug(f"Received complete response ({len(buffer)} bytes)")
                    return response
                except json.JSONDecodeError:
                    continue
            return json.loads(buffer.decode('utf-8')) if buffer else {"status": "error", "message": "No response received."}
        except Exception as e:
            logger.error(f"An error occurred during command execution: {e}")
            return {"status": "error", "message": str(e)}
        finally:
            if self.socket: self.socket.close()

_unreal_connection: Optional[UnrealConnection] = None
def get_unreal_connection() -> Optional[UnrealConnection]:
    global _unreal_connection
    if _unreal_connection is None:
        _unreal_connection = UnrealConnection(UNREAL_HOST, UNREAL_PORT)
    return _unreal_connection

@asynccontextmanager
async def server_lifespan(server: FastMCP) -> AsyncIterator[Dict[str, Any]]:
    logger.info("UmgMcp server is starting up.")
    yield {}
    logger.info("UmgMcp server is shutting down.")

mcp = FastMCP(
    "UmgMcp",
    lifespan=server_lifespan
)

# --- UmgMcp API Tools (Complete Contract v4.0 - Cognitive Edition) ---

# ==============================================================================
#  Category: Meta-Knowledge & Introspection
#  AI Hint: Use these tools to "learn the rules of the world" before acting.
# ==============================================================================

@mcp.tool()
def get_widget_schema(widget_type: str) -> Dict[str, Any]:
    """
    "What can this component do?" - Retrieves the "schema" for a widget type, detailing its editable properties and their types.
    AI HINT: Call this BEFORE using 'set_widget_properties' to know which properties are valid, preventing errors.
    """
    unreal = get_unreal_connection()
    return unreal.send_command("get_widget_schema", {"widget_type": widget_type})

@mcp.tool()
def get_creatable_widget_types() -> Dict[str, Any]:
    """
    "What's in my toolbox?" - Returns a list of all widget class names that can be created with the 'create_widget' tool.
    AI HINT: Use this to know the full range of widgets you can spawn in the UMG editor.
    """
    unreal = get_unreal_connection()
    return unreal.send_command("get_creatable_widget_types")

# ==============================================================================
#  Category: Intent Inference Workflow
#  AI Hint: Follow this two-step process to learn from user actions.
# ==============================================================================

@mcp.tool()
def capture_ui_snapshot(asset_path: str) -> Dict[str, Any]:
    """
    "Remember this state" (Step 1) - Captures a snapshot of the UMG asset's current state and returns a unique snapshot_id.
    AI HINT: This is STEP 1 of the 'learn from user' workflow. Call this BEFORE asking the user for manual edits.
    """
    unreal = get_unreal_connection()
    return unreal.send_command("capture_ui_snapshot", {"asset_path": asset_path})

@mcp.tool()
def compare_ui_snapshots(snapshot_id_before: str, snapshot_id_after: str) -> Dict[str, Any]:
    """
    "What did the user mean by their edits?" (Step 2) - Compares two snapshots and returns inferred design rules.
    AI HINT: This is the 'brain' of the learning process. Its output contains design rules (alignment, spacing)
    that you can use to apply a much more robust and intelligent layout than just copying coordinates.
    """
    unreal = get_unreal_connection()
    return unreal.send_command("compare_ui_snapshots", {"snapshot_id_before": snapshot_id_before, "snapshot_id_after": snapshot_id_after})

import UMGAttention

# Create an instance of the UMGAttention class, passing the client connection
umg_attention_client = UMGAttention.UMGAttention(get_unreal_connection())

# ==============================================================================
#  Category: Attention
#  AI Hint: When the user's request is ambiguous, use these tools first to find their focus.
# ==============================================================================

@mcp.tool()
def get_target_umg_asset() -> Dict[str, Any]:
    """
    "What UMG is the user editing right now?" - Gets the asset path of the UMG Editor the user is currently focused on.
    AI HINT: This is your PRIMARY tool for understanding ambiguous commands like "change this button".
    """
    return umg_attention_client.get_target_umg_asset()

@mcp.tool()
def get_last_edited_umg_asset() -> Dict[str, Any]:
    """
    "What was the user just working on?" - Gets the asset path of the last UMG asset that was opened or saved.
    AI HINT: Use this as a fallback if 'get_target_umg_asset' returns null (e.g., user switched to their code editor).
    """
    return umg_attention_client.get_last_edited_umg_asset()

@mcp.tool()
def get_recently_edited_umg_assets(max_count: int = 5) -> Dict[str, Any]:
    """
    "What has the user been working on recently?" - Gets a list of recently edited UMG assets.
    AI HINT: Use this to offer suggestions if the context is completely lost.
    """
    return umg_attention_client.get_recently_edited_umg_assets(max_count)

@mcp.tool()
def set_target_umg_asset(asset_path: str) -> Dict[str, Any]:
    """
    Sets the UMG asset that should be considered the current attention target.
    This allows programmatically setting the active UMG context.
    """
    return umg_attention_client.set_target_umg_asset(asset_path)

import UMGGet
import UMGSet

# ==============================================================================
#  Category: Sensing & Insight
#  AI Hint: Once you have context, use these tools to understand the details.
# ==============================================================================

# Create an instance of the UMGGet class, passing the client connection
umg_get_client = UMGGet.UMGGet(get_unreal_connection())

@mcp.tool()
def get_widget_tree(asset_path: str) -> Dict[str, Any]:
    """Retrieves the full widget hierarchy for a UMG asset as a nested JSON object."""
    return umg_get_client.get_widget_tree(asset_path)

@mcp.tool()
def query_widget_properties(widget_id: str, properties: List[str]) -> Dict[str, Any]:
    """Queries a list of specific properties from a single widget (e.g., 'Slot.Size')."""
    return umg_get_client.query_widget_properties(widget_id, properties)

@mcp.tool()
def get_layout_data(asset_path: str, resolution_width: int = 1920, resolution_height: int = 1080) -> Dict[str, Any]:
    """Gets screen-space bounding boxes for all widgets at a given resolution."""
    return umg_get_client.get_layout_data(asset_path, resolution_width, resolution_height)

@mcp.tool()
def check_widget_overlap(asset_path: str, widget_ids: Optional[List[str]] = None) -> Dict[str, Any]:
    """
    "Are any widgets overlapping?" - Efficiently checks for layout overlap between widgets.
    AI HINT: PREFER this server-side check over fetching all layout data and calculating it yourself. It's much faster.
    """
    return umg_get_client.check_widget_overlap(asset_path, widget_ids)

@mcp.tool()
def get_asset_file_system_path(asset_path: str) -> Dict[str, Any]:
    """
    Converts an Unreal Engine asset path to an absolute file system path.
    """
    return umg_get_client.get_asset_file_system_path(asset_path)

# ==============================================================================
#  Category: Action
#  AI Hint: These are the tools you use to change the world (the UMG asset).
# ==============================================================================

# Create an instance of the UMGSet class, passing the client connection
umg_set_client = UMGSet.UMGSet(get_unreal_connection())

@mcp.tool()
def create_widget(asset_path: str, parent_id: str, widget_type: str, widget_name: str) -> Dict[str, Any]:
    """Creates a new widget and attaches it to a parent."""
    return umg_set_client.create_widget(asset_path, parent_id, widget_type, widget_name)

@mcp.tool()
def set_widget_properties(widget_id: str, properties: Dict[str, Any]) -> Dict[str, Any]:
    """Sets one or more properties on a specific widget. This is your primary modification tool."""
    return umg_set_client.set_widget_properties(widget_id, properties)

@mcp.tool()
def delete_widget(widget_id: str) -> Dict[str, Any]:
    """Deletes a widget from the UMG asset."""
    return umg_set_client.delete_widget(widget_id)

@mcp.tool()
def reparent_widget(widget_id: str, new_parent_id: str) -> Dict[str, Any]:
    """
    Moves a widget to be a child of a different parent.
    AI HINT: Use this to restructure the UI, for example, to group items in a VerticalBox for automatic alignment.
    """
    return umg_set_client.reparent_widget(widget_id, new_parent_id)

# ==============================================================================
#  Category: File Workflow
#  AI Hint: Use these tools to interact with the file system and version control.
# ==============================================================================

@mcp.tool()
def export_umg_to_json(asset_path: str) -> Dict[str, Any]:
    """'Decompiles' a UMG .uasset file into a JSON string, returned in the response."""
    unreal = get_unreal_connection()
    return unreal.send_command("export_umg_to_json", {"asset_path": asset_path})

@mcp.tool()
def apply_json_to_umg(asset_path: str, json_data: Dict[str, Any]) -> Dict[str, Any]:
    """'Compiles' a JSON string into a UMG .uasset file, creating or overwriting it."""
    unreal = get_unreal_connection()
    # The MCP framework deserializes the incoming JSON argument into a Python dict.
    # We need to re-serialize it back into a JSON string before sending it to the Unreal C++ backend.
    json_string_for_cpp = json.dumps(json_data)
    return unreal.send_command("apply_json_to_umg", {"asset_path": asset_path, "json_data": json_string_for_cpp})

# ==============================================================================

if __name__ == "__main__":
    logger.info("Starting UmgMcp Server (Contract v4.0) with stdio transport...")
    mcp.run(transport='stdio')