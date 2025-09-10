"""
UmgMcp Server - Your AI UMG Design Partner (v3.0 - Context-Aware)

This MCP server is designed exclusively for interacting with the UmgMcp plugin
in Unreal Engine 5. It provides a complete and verified suite of tools for an AI
assistant to sense, understand, and manipulate UMG assets programmatically.

This version introduces a context-aware workflow. The AI first sets a target
UMG asset using the 'Attention' tools, and all subsequent 'Sensing' and 'Action'
tools operate implicitly on that target.
"""

import logging
import socket
import json
from contextlib import asynccontextmanager
from typing import AsyncIterator, Dict, Any, Optional, List
from mcp.server.fastmcp import FastMCP

# Import configuration from our dedicated config file
from mcp_config import UNREAL_HOST, UNREAL_PORT

# Import the modular clients
import UMGAttention
import UMGGet
import UMGSet
import UMGFileTransformation

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - [%(filename)s:%(lineno)d] - %(message)s',
    handlers=[ logging.FileHandler('umg_mcp_server.log') ]
)
logger = logging.getLogger("UmgMcpServerV3")

class UnrealConnection:
    """Manages the socket connection to the UmgMcp plugin running inside Unreal Engine."""
    def __init__(self):
        logger.info(f"Unreal Motion Graphics UI Designer Mode Context Process Launching... Connecting to UmgMcp plugin at {UNREAL_HOST}:{UNREAL_PORT}...")
        self.socket = None
        self.connected = False
    
    def connect(self) -> bool:
        """Connect to the Unreal Engine instance."""
        try:
            # Close any existing socket
            if self.socket:
                try:
                    self.socket.close()
                except:
                    pass
                self.socket = None
            
            logger.info(f"Connecting to Unreal at {UNREAL_HOST}:{UNREAL_PORT}...")
            self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.socket.settimeout(5)  # 5 second timeout
            
            # Set socket options for better stability
            self.socket.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
            self.socket.setsockopt(socket.SOL_SOCKET, socket.SO_KEEPALIVE, 1)
            
            # Set larger buffer sizes
            self.socket.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 65536)
            self.socket.setsockopt(socket.SOL_SOCKET, socket.SO_SNDBUF, 65536)
            
            self.socket.connect((UNREAL_HOST, UNREAL_PORT))
            self.connected = True
            logger.info("Connected to Unreal Engine")
            return True
            
        except Exception as e:
            logger.error(f"Failed to connect to Unreal: {e}")
            self.connected = False
            return False
    
    def disconnect(self):
        """Disconnect from the Unreal Engine instance."""
        if self.socket:
            try:
                self.socket.close()
            except:
                pass
        self.socket = None
        self.connected = False

    def receive_full_response(self, sock, buffer_size=4096) -> bytes:
        """Receive a complete response from Unreal, handling chunked data."""
        chunks = []
        sock.settimeout(5)  # 5 second timeout
        try:
            while True:
                chunk = sock.recv(buffer_size)
                if not chunk:
                    if not chunks:
                        raise Exception("Connection closed before receiving data")
                    break
                chunks.append(chunk)
                
                # Process the data received so far
                data = b''.join(chunks)
                decoded_data = data.decode('utf-8')
                
                # Try to parse as JSON to check if complete
                try:
                    json.loads(decoded_data)
                    logger.info(f"Received complete response ({len(data)} bytes)")
                    return data
                except json.JSONDecodeError:
                    # Not complete JSON yet, continue reading
                    logger.debug(f"Received partial response, waiting for more data...")
                    continue
                except Exception as e:
                    logger.warning(f"Error processing response chunk: {str(e)}")
                    continue
        except socket.timeout:
            logger.warning("Socket timeout during receive")
            if chunks:
                # If we have some data already, try to use it
                data = b''.join(chunks)
                try:
                    json.loads(data.decode('utf-8'))
                    logger.info(f"Using partial response after timeout ({len(data)} bytes)")
                    return data
                except:
                    pass
            raise Exception("Timeout receiving Unreal response")
        except Exception as e:
            logger.error(f"Error during receive: {str(e)}")
            raise
    
    def send_command(self, command: str, params: Dict[str, Any] = None) -> Optional[Dict[str, Any]]:
        """Send a command to Unreal Engine and get the response."""
        # Always reconnect for each command, since Unreal closes the connection after each command
        # This is different from Unity which keeps connections alive
        if self.socket:
            try:
                self.socket.close()
            except:
                pass
            self.socket = None
            self.connected = False
        
        if not self.connect():
            logger.error("Failed to connect to Unreal Engine for command")
            return None
        
        try:
            # Match Unity's command format exactly
            command_obj = {
                "type": command,  # Use "type" instead of "command"
                "params": params or {}  # Use Unity's params or {} pattern
            }
            
            # Send without newline, exactly like Unity
            command_json = json.dumps(command_obj)
            logger.debug(f"Sending command: {command_json}")
            self.socket.sendall(command_json.encode('utf-8'))
            
            # Read response using improved handler
            response_data = self.receive_full_response(self.socket)
            response = json.loads(response_data.decode('utf-8'))
            
            # Log complete response for debugging
            logger.info(f"Complete response from Unreal: {response}")
            
            # Check for both error formats: {"status": "error", ...} and {"success": false, ...}
            if response.get("status") == "error":
                error_message = response.get("error") or response.get("message", "Unknown Unreal error")
                logger.error(f"Unreal error (status=error): {error_message}")
                # We want to preserve the original error structure but ensure error is accessible
                if "error" not in response:
                    response["error"] = error_message
            elif response.get("success") is False:
                # This format uses {"success": false, "error": "message"} or {"success": false, "message": "message"}
                error_message = response.get("error") or response.get("message", "Unknown Unreal error")
                logger.error(f"Unreal error (success=false): {error_message}")
                # Convert to the standard format expected by higher layers
                response = {
                    "status": "error",
                    "error": error_message
                }
            
            # Always close the connection after command is complete
            # since Unreal will close it on its side anyway
            try:
                self.socket.close()
            except:
                pass
            self.socket = None
            self.connected = False
            
            return response
            
        except Exception as e:
            logger.error(f"Error sending command: {e}")
            # Always reset connection state on any error
            self.connected = False
            try:
                self.socket.close()
            except:
                pass
            self.socket = None
            return {
                "status": "error",
                "error": str(e)
            }

# Global connection state
_unreal_connection: UnrealConnection = None

def get_unreal_connection() -> Optional[UnrealConnection]:
    """Get the connection to Unreal Engine."""
    global _unreal_connection
    try:
        if _unreal_connection is None:
            _unreal_connection = UnrealConnection()
            if not _unreal_connection.connect():
                logger.warning("Could not connect to Unreal Engine")
                _unreal_connection = None
        else:
            # Verify connection is still valid with a ping-like test
            try:
                # Simple test by sending an empty buffer to check if socket is still connected
                _unreal_connection.socket.sendall(b'\x00')
                logger.debug("Connection verified with ping test")
            except Exception as e:
                logger.warning(f"Existing connection failed: {e}")
                _unreal_connection.disconnect()
                _unreal_connection = None
                # Try to reconnect
                _unreal_connection = UnrealConnection()
                if not _unreal_connection.connect():
                    logger.warning("Could not reconnect to Unreal Engine")
                    _unreal_connection = None
                else:
                    logger.info("Successfully reconnected to Unreal Engine")
        
        return _unreal_connection
    except Exception as e:
        logger.error(f"Error getting Unreal connection: {e}")
        return None

@asynccontextmanager
async def server_lifespan(server: FastMCP) -> AsyncIterator[Dict[str, Any]]:
    """Handle server startup and shutdown."""
    global _unreal_connection
    logger.info("UnrealMCP server starting up")
    try:
        _unreal_connection = get_unreal_connection()
        if _unreal_connection:
            logger.info("Connected to Unreal Engine on startup")
        else:
            logger.warning("Could not connect to Unreal Engine on startup")
    except Exception as e:
        logger.error(f"Error connecting to Unreal Engine on startup: {e}")
        _unreal_connection = None
    
    try:
        yield {}
    finally:
        if _unreal_connection:
            _unreal_connection.disconnect()
            _unreal_connection = None
        logger.info("Unreal MCP server shut down")

mcp = FastMCP(
    "UmgMcp",
    lifespan=server_lifespan
)

# =============================================================================
#  Category: Introspection (Knowledge Base)
# =============================================================================

@mcp.tool()
def get_widget_schema(widget_type: str) -> Dict[str, Any]:
    """
    "What can this component do?" - Retrieves the "schema" for a widget type, detailing its editable properties and their types.
    AI HINT: Call this BEFORE using 'set_widget_properties' to know which properties are valid, preventing errors.
    """
    conn = get_unreal_connection()
    umg_get_client = UMGGet.UMGGet(conn)
    return umg_get_client.get_widget_schema(widget_type)

@mcp.tool()
def get_creatable_widget_types() -> Dict[str, Any]:
    """
    "What's in my toolbox?" - Returns a list of all widget class names that can be created with the 'create_widget' tool.
    AI HINT: Use this to know the full range of widgets you can spawn in the UMG editor.
    """
    conn = get_unreal_connection()
    umg_get_client = UMGGet.UMGGet(conn)
    return umg_get_client.get_creatable_widget_types()

# =============================================================================
#  Category: Attention & Context
# =============================================================================

@mcp.tool()
def get_target_umg_asset() -> Dict[str, Any]:
    """
    "What UMG is the user editing right now?" - Gets the asset path of the UMG Editor the user is currently focused on.
    AI HINT: This is your PRIMARY tool for understanding ambiguous commands like "change this button".
    """
    conn = get_unreal_connection()
    umg_attention_client = UMGAttention.UMGAttention(conn)
    return umg_attention_client.get_target_umg_asset()

@mcp.tool()
def get_last_edited_umg_asset() -> Dict[str, Any]:
    """
    "What was the user just working on?" - Gets the asset path of the last UMG asset that was opened or saved.
    AI HINT: Use this as a fallback if 'get_target_umg_asset' returns null. Our tools do this automatically.
    """
    conn = get_unreal_connection()
    umg_attention_client = UMGAttention.UMGAttention(conn)
    return umg_attention_client.get_last_edited_umg_asset()

@mcp.tool()
def get_recently_edited_umg_assets(max_count: int = 5) -> Dict[str, Any]:
    """
    "What has the user been working on recently?" - Gets a list of recently edited UMG assets.
    AI HINT: Use this to offer suggestions if the context is completely lost.
    """
    conn = get_unreal_connection()
    umg_attention_client = UMGAttention.UMGAttention(conn)
    return umg_attention_client.get_recently_edited_umg_assets(max_count)

@mcp.tool()
def set_target_umg_asset(asset_path: str) -> Dict[str, Any]:
    """
    Sets the UMG asset that should be considered the current attention target.
    This allows programmatically setting the active UMG context.
    """
    conn = get_unreal_connection()
    umg_attention_client = UMGAttention.UMGAttention(conn)
    return umg_attention_client.set_target_umg_asset(asset_path)

# =============================================================================
#  Category: Sensing
# =============================================================================

@mcp.tool()
def get_widget_tree(asset_path: Optional[str] = None) -> Dict[str, Any]:
    """
    Retrieves the full widget hierarchy for a UMG asset.
    AI HINT: If 'asset_path' is omitted, the command will use the globally targeted asset set by 'set_target_umg_asset'.
    This is your EYES. Call this first to get a map of the UI.
    """
    conn = get_unreal_connection()
    umg_get_client = UMGGet.UMGGet(conn)
    return umg_get_client.get_widget_tree(asset_path)

@mcp.tool()
def query_widget_properties(widget_name: str, properties: List[str], asset_path: Optional[str] = None) -> Dict[str, Any]:
    """
    Queries a list of specific properties from a single widget by its name (e.g., 'Slot.Size').
    AI HINT: If 'asset_path' is omitted, the command will use the globally targeted asset.
    """
    conn = get_unreal_connection()
    umg_get_client = UMGGet.UMGGet(conn)
    return umg_get_client.query_widget_properties(asset_path, widget_name, properties)

@mcp.tool()
def get_layout_data(resolution_width: int = 1920, resolution_height: int = 1080, asset_path: Optional[str] = None) -> Dict[str, Any]:
    """
    Gets screen-space bounding boxes for all widgets in the asset at a given resolution.
    AI HINT: If 'asset_path' is omitted, the command will use the globally targeted asset.
    """
    conn = get_unreal_connection()
    umg_get_client = UMGGet.UMGGet(conn)
    return umg_get_client.get_layout_data(asset_path, resolution_width, resolution_height)

@mcp.tool()
def check_widget_overlap(widget_names: Optional[List[str]] = None, asset_path: Optional[str] = None) -> Dict[str, Any]:
    """
    "Are any widgets overlapping?" - Efficiently checks for layout overlap between widgets in the asset.
    AI HINT: PREFER this server-side check. If 'asset_path' is omitted, it uses the globally targeted asset.
    """
    conn = get_unreal_connection()
    umg_get_client = UMGGet.UMGGet(conn)
    return umg_get_client.check_widget_overlap(asset_path, widget_names)

# =============================================================================
#  Category: Action
# =============================================================================

@mcp.tool()
def create_widget(parent_name: str, widget_type: str, new_widget_name: str, asset_path: Optional[str] = None) -> Dict[str, Any]:
    """
    Creates a new widget and attaches it to a parent.
    AI HINT: If 'asset_path' is omitted, it uses the globally targeted asset. Use 'get_creatable_widget_types' for 'widget_type'.
    """
    conn = get_unreal_connection()
    umg_set_client = UMGSet.UMGSet(conn)
    return umg_set_client.create_widget(asset_path, parent_name, widget_type, new_widget_name)

@mcp.tool()
def set_widget_properties(widget_name: str, properties: Dict[str, Any], asset_path: Optional[str] = None) -> Dict[str, Any]:
    """
    Sets one or more properties on a specific widget by its name. This is your primary modification tool.
    AI HINT: If 'asset_path' is omitted, it uses the globally targeted asset. Use get_widget_schema to see available properties.
    """
    conn = get_unreal_connection()
    umg_set_client = UMGSet.UMGSet(conn)
    return umg_set_client.set_widget_properties(asset_path, widget_name, properties)

@mcp.tool()
def delete_widget(widget_name: str, asset_path: Optional[str] = None) -> Dict[str, Any]:
    """
    Deletes a widget by its name from the UMG asset.
    AI HINT: If 'asset_path' is omitted, it uses the globally targeted asset.
    """
    conn = get_unreal_connection()
    umg_set_client = UMGSet.UMGSet(conn)
    return umg_set_client.delete_widget(asset_path, widget_name)

@mcp.tool()
def reparent_widget(widget_name: str, new_parent_name: str, asset_path: Optional[str] = None) -> Dict[str, Any]:
    """
    Moves a widget to be a child of a different parent.
    AI HINT: If 'asset_path' is omitted, it uses the globally targeted asset.
    """
    conn = get_unreal_connection()
    umg_set_client = UMGSet.UMGSet(conn)
    return umg_set_client.reparent_widget(asset_path, widget_name, new_parent_name)

# =============================================================================
#  Category: File Transformation (Explicit Path)
# =============================================================================

@mcp.tool()
def export_umg_to_json(asset_path: str) -> Dict[str, Any]:
    """'Decompiles' a UMG .uasset file into a JSON string. Requires an explicit asset path."""
    conn = get_unreal_connection()
    umg_file_client = UMGFileTransformation.UMGFileTransformation(conn)
    return umg_file_client.export_umg_to_json(asset_path)

@mcp.tool()
def apply_json_to_umg(asset_path: str, json_data: Dict[str, Any]) -> Dict[str, Any]:
    """'Compiles' a JSON string into a UMG .uasset file. Requires an explicit asset path."""
    conn = get_unreal_connection()
    umg_file_client = UMGFileTransformation.UMGFileTransformation(conn)
    return umg_file_client.apply_json_to_umg(asset_path, json_data)

# =============================================================================

@mcp.prompt()
def info():
    """Run method:"tools/list" to get more details."""
    return """
# UMG MCP API - v3.0

This document lists all available tools for interacting with the UMG MCP server.

## 1. 感知 (Sensing) - AI的“眼睛”

*   `get_widget_tree(asset_path: Optional[str] = None)`
    *   **作用**: 获取UI的完整层级结构。**这是最核心的感知API**。
    *   **工作流**: 如果提供了 `asset_path`，则直接操作该文件。如果省略，则自动操作由 `set_target_umg_asset` 设定的全局目标。

*   `query_widget_properties(widget_name: str, properties: List[str], asset_path: Optional[str] = None)`
    *   **作用**: 精确查询某个控件的一个或多个具体属性的值。
    *   **工作流**: 同上，优先使用 `asset_path`，否则回退到全局目标。

*   `get_layout_data(resolution_width: int = 1920, resolution_height: int = 1080, asset_path: Optional[str] = None)`
    *   **作用**: 获取所有控件在给定分辨率下的屏幕坐标和尺寸。
    *   **工作流**: 同上。

*   `check_widget_overlap(widget_names: Optional[List[str]] = None, asset_path: Optional[str] = None)`
    *   **作用**: 一个高效的后端API，直接判断界面上是否存在控件重叠。
    *   **工作流**: 同上。

## 2. 行动 (Action) - AI的“双手”

*   `create_widget(parent_name: str, widget_type: str, new_widget_name: str, asset_path: Optional[str] = None)`
    *   **作用**: 在指定的父控件下创建一个新的控件。
    *   **工作流**: 同上。

*   `delete_widget(widget_name: str, asset_path: Optional[str] = None)`
    *   **作用**: 删除一个指定的控件。
    *   **工作流**: 同上。

*   `set_widget_properties(widget_name: str, properties: Dict[str, Any], asset_path: Optional[str] = None)`
    *   **作用**: 修改一个控件的一个或多个属性。**这是最核心的修改API**。
    *   **工作流**: 同上。

*   `reparent_widget(widget_name: str, new_parent_name: str, asset_path: Optional[str] = None)`
    *   **作用**: 将一个控件从一个父容器移动到另一个。
    *   **工作流**: 同上。

## 3. 自省与上下文 (Introspection & Context)

*   `get_creatable_widget_types()`
    *   **作用**: 告诉AI它“工具箱”里有哪些类型的控件可以被创建。

*   `get_widget_schema(widget_type)`
    *   **作用**: 告诉AI某个特定类型的控件有哪些可以被编辑的属性。

*   `set_target_umg_asset(asset_path: str)`
    *   **作用**: 设置一个全局工作目标，所有后续省略 `asset_path` 参数的命令都将操作此目标。

*   `get_target_umg_asset()`
    *   **作用**: 查询当前设定的全局工作目标是什么。
"""

if __name__ == "__main__":
    port = UNREAL_PORT
    try:
        # Step 1: Start the main server and begin listening for connections from Gemini CLI.
        logger.info(f"Starting UmgMcp Server (v3.0) on port {port}. Waiting for connections...")
        mcp.run(transport='stdio')

    except Exception as e:
        logger.error(f"A critical error occurred while starting the UmgMcp Server: {e}")
        # If we failed, print an error code that the parent process might see.
        print("MCP_SERVER_START_FAILED")