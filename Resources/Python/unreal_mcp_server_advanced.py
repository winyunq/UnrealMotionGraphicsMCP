"""
UmgMcp Server - Your AI UMG Design Partner

This MCP server is designed exclusively for interacting with the UmgMcp plugin
in Unreal Engine 5. It provides a suite of tools for an AI assistant to sense,
understand, and manipulate UMG assets programmatically.

This server acts as the client-side bridge, translating MCP tool calls into
JSON commands sent over a socket to the Unreal Engine plugin.
"""

import logging
import socket
import json
from contextlib import asynccontextmanager
from typing import AsyncIterator, Dict, Any, Optional, List
from mcp.server.fastmcp import FastMCP

# Import configuration from our dedicated config file
from mcp_config import UNREAL_HOST, UNREAL_PORT

# Configure logging for our UMG-specific server
logging.basicConfig(
    level=logging.DEBUG,
    format='%(asctime)s - %(name)s - %(levelname)s - [%(filename)s:%(lineno)d] - %(message)s',
    handlers=[
        logging.FileHandler('umg_mcp_server.log'), # Log to a specific file
    ]
)
logger = logging.getLogger("UmgMcpServer")

class UnrealConnection:
    """
    Manages the socket connection to the UmgMcp plugin running inside Unreal Engine.
    This class is responsible for sending JSON commands and receiving responses.
    """
    
    def __init__(self, host: str, port: int):
        """Initializes the connection with host and port from the config."""
        self.host = host
        self.port = port
        self.socket = None

    def connect(self) -> bool:
        """Establishes a connection to the Unreal Engine plugin."""
        try:
            if self.socket:
                self.socket.close()
            
            logger.info(f"Connecting to UmgMcp plugin at {self.host}:{self.port}...")
            self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.socket.settimeout(10) # 10-second timeout for connection
            self.socket.connect((self.host, self.port))
            logger.info("Successfully connected to Unreal Engine (UmgMcp).")
            return True
        except Exception as e:
            logger.error(f"Failed to connect to UmgMcp plugin: {e}")
            self.socket = None
            return False

    def send_command(self, command_type: str, params: Dict[str, Any] = None) -> Optional[Dict[str, Any]]:
        """Sends a single JSON command and waits for a complete JSON response."""
        if not self.connect():
            return {"status": "error", "message": "Failed to establish connection with the Unreal Engine plugin."}
        
        try:
            command_obj = {"type": command_type, "params": params or {}}
            command_json = json.dumps(command_obj)
            logger.debug(f"Sending command: {command_json}")
            
            self.socket.sendall(command_json.encode('utf-8'))
            
            # Receive loop to handle potentially large JSON responses
            buffer = b""
            while True:
                chunk = self.socket.recv(4096)
                if not chunk:
                    break
                buffer += chunk
                try:
                    # Try to decode the buffer on each chunk to see if we have a full JSON object
                    response = json.loads(buffer.decode('utf-8'))
                    logger.debug(f"Received complete response ({len(buffer)} bytes): {response}")
                    return response
                except json.JSONDecodeError:
                    # Not a complete JSON object yet, continue receiving
                    continue

            if buffer:
                # This case handles if the server closes the connection right after sending
                return json.loads(buffer.decode('utf-8'))
            else:
                return {"status": "error", "message": "No response received from the server."}

        except Exception as e:
            logger.error(f"An error occurred during command execution: {e}")
            return {"status": "error", "message": str(e)}
        finally:
            if self.socket:
                self.socket.close()
                self.socket = None

# Global connection instance
_unreal_connection: Optional[UnrealConnection] = None

def get_unreal_connection() -> Optional[UnrealConnection]:
    """Provides a global accessor to the UnrealConnection instance."""
    global _unreal_connection
    if _unreal_connection is None:
        _unreal_connection = UnrealConnection(UNREAL_HOST, UNREAL_PORT)
    return _unreal_connection

@asynccontextmanager
async def server_lifespan(server: FastMCP) -> AsyncIterator[Dict[str, Any]]:
    """A lifecycle manager for the MCP server. Not strictly needed for this client-style server."""
    logger.info("UmgMcp server is starting up.")
    yield {}
    logger.info("UmgMcp server is shutting down.")

# Initialize our UMG-focused MCP server
mcp = FastMCP(
    "UmgMcp",
    description="A dedicated MCP server for AI-driven UMG design in Unreal Engine 5.",
    lifespan=server_lifespan
)

# --- UMG API Tools ---

# Category: Attention
@mcp.tool()
def get_active_umg_context() -> Dict[str, Any]:
    """
    Gets the asset path of the UMG Editor that the user is currently focused on.
    This is the primary way to get the user's current work context.
    Returns null if the user is not in a UMG editor.
    """
    unreal = get_unreal_connection()
    return unreal.send_command("get_active_umg_context")

@mcp.tool()
def get_last_edited_umg_asset() -> Dict[str, Any]:
    """
    Gets the asset path of the last UMG asset that was opened or saved.
    Useful for maintaining context when the user switches away from the UMG editor.
    """
    unreal = get_unreal_connection()
    return unreal.send_command("get_last_edited_umg_asset")

# Category: Sensing & Insight
@mcp.tool()
def get_widget_tree(asset_path: str) -> Dict[str, Any]:
    """
    Retrieves the entire widget hierarchy (the 'tree') for a given UMG asset.
    The result is a nested JSON object representing the parent-child relationships.
    
    Args:
        asset_path: The full asset path, e.g., "/Game/UI/WBP_MainMenu.WBP_MainMenu".
    """
    unreal = get_unreal_connection()
    return unreal.send_command("get_widget_tree", {"asset_path": asset_path})

@mcp.tool()
def check_widget_overlap(asset_path: str, widget_ids: Optional[List[str]] = None) -> Dict[str, Any]:
    """
    Checks for layout overlap between widgets in a UMG asset.
    
    Args:
        asset_path: The asset to check.
        widget_ids: Optional. If provided, only checks for overlap between these specific widgets.
                    If omitted, checks all widgets against each other.
    """
    unreal = get_unreal_connection()
    params = {"asset_path": asset_path}
    if widget_ids:
        params["widget_ids"] = widget_ids
    return unreal.send_command("check_widget_overlap", params)

# Category: Action
@mcp.tool()
def create_widget(asset_path: str, parent_id: str, widget_type: str, widget_name: str) -> Dict[str, Any]:
    """
    Creates a new widget of a specific type and attaches it to a parent widget.
    
    Args:
        asset_path: The UMG asset to modify.
        parent_id: The name of the parent widget to attach to (e.g., "CanvasPanel_0").
        widget_type: The class of the widget to create (e.g., "Button", "TextBlock").
        widget_name: The name to assign to the new widget.
    """
    unreal = get_unreal_connection()
    params = {
        "asset_path": asset_path,
        "parent_id": parent_id,
        "widget_type": widget_type,
        "widget_name": widget_name
    }
    return unreal.send_command("create_widget", params)

@mcp.tool()
def set_widget_properties(widget_id: str, properties: Dict[str, Any]) -> Dict[str, Any]:
    """
    Sets one or more properties on a specific widget. This is the main tool for modification.
    
    Args:
        widget_id: The unique identifier for the widget (e.g., "WBP_MainMenu.PlayButton").
        properties: A dictionary of properties to set, e.g., {"Slot.Size": [200, 50], "bIsEnabled": false}.
    """
    unreal = get_unreal_connection()
    return unreal.send_command("set_widget_properties", {"widget_id": widget_id, "properties": properties})

# Category: File Workflow
@mcp.tool()
def export_umg_to_json(asset_path: str) -> Dict[str, Any]:
    """
    'Decompiles' a binary .uasset UMG file into a JSON string representation.
    The JSON string is returned directly in the response.
    """
    unreal = get_unreal_connection()
    return unreal.send_command("export_umg_to_json", {"asset_path": asset_path})

@mcp.tool()
def apply_json_to_umg(asset_path: str, json_data: str) -> Dict[str, Any]:
    """
    'Compiles' a JSON string representation into a binary .uasset UMG file.
    This will create or overwrite the specified UMG asset.
    
    Args:
        asset_path: The target asset path to create or overwrite.
        json_data: The full JSON content as a string.
    """
    unreal = get_unreal_connection()
    return unreal.send_command("apply_json_to_umg", {"asset_path": asset_path, "json_data": json_data})

# Main entry point to run the server
if __name__ == "__main__":
    logger.info("Starting UmgMcp server with stdio transport...")
    mcp.run(transport='stdio')