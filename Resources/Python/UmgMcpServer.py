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
import UMGHTMLParser
import UMGEditor
import UMGBlueprint
import UMGSequencer

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
            self.socket.settimeout(30)  # 30 second timeout
            
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
        sock.settimeout(30)  # 30 second timeout
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
                "command": command,  # Use "command" to match C++ ProcessMessage expectation
                "params": params or {}  # Use Unity's params or {} pattern
            }
            
            # Send with custom delimiter for robust server-side reading
            # We use '__MCP_END__' as a safe delimiter to avoid collisions with content
            command_json = json.dumps(command_obj) + "__MCP_END__"
            logger.debug(f"Sending command: {command_json.strip()}")
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

class ContextManager:
    """Manages the 'attention' context for the AI."""
    def __init__(self):
        self._target_asset_path: Optional[str] = None

    def set_target(self, asset_path: str):
        self._target_asset_path = asset_path
        logger.info(f"Context set to: {asset_path}")

    def get_target(self) -> Optional[str]:
        return self._target_asset_path

    def clear_target(self):
        self._target_asset_path = None
        logger.info("Context cleared")

# Global Context Manager Instance
context_manager = ContextManager()

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
    """
    conn = get_unreal_connection()
    umg_get_client = UMGGet.UMGGet(conn)
    return umg_get_client.get_widget_schema(widget_type)

@mcp.tool()
def get_creatable_widget_types() -> Dict[str, Any]:
    """
    "What's in my toolbox?" - Returns a list of all widget class names that can be created with the 'create_widget' tool.
    """
    # Per user instruction, return a philosophical guide instead of a fixed list.
    # This encourages the AI to experiment based on its own knowledge.
    return {
        "status": "success",
        "result": {
            "status": "info",
            "data": {
                "message": "Theoretically, any UMG widget class can be created. In practice, it depends on the AI's knowledge of valid class names and requires trial and error. Start with common types like 'Button', 'TextBlock', 'Image', 'CanvasPanel', 'VerticalBox'."
            }
        }
    }

# =============================================================================
#  Category: Attention & Context
# =============================================================================

@mcp.tool()
def get_target_umg_asset() -> Dict[str, Any]:
    """
    "What UMG is the user editing right now?" - Gets the asset path of the UMG Editor the user is currently focused on.
    """
    conn = get_unreal_connection()
    umg_attention_client = UMGAttention.UMGAttention(conn)
    return umg_attention_client.get_target_umg_asset()

@mcp.tool()
def get_last_edited_umg_asset() -> Dict[str, Any]:
    """
    "What was the user just working on?" - Gets the asset path of the last UMG asset that was opened or saved.
    """
    conn = get_unreal_connection()
    umg_attention_client = UMGAttention.UMGAttention(conn)
    return umg_attention_client.get_last_edited_umg_asset()

@mcp.tool()
def get_recently_edited_umg_assets(max_count: int = 5) -> Dict[str, Any]:
    """
    "What has the user been working on recently?" - Gets a list of recently edited UMG assets.
    """
    conn = get_unreal_connection()
    umg_attention_client = UMGAttention.UMGAttention(conn)
    return umg_attention_client.get_recently_edited_umg_assets(max_count)

@mcp.tool()
def set_target_umg_asset(asset_path: str) -> Dict[str, Any]:
    """
    Sets the UMG asset that should be considered the current attention target.
    """
    context_manager.set_target(asset_path) # Update local context
    conn = get_unreal_connection()
    umg_attention_client = UMGAttention.UMGAttention(conn)
    return umg_attention_client.set_target_umg_asset(asset_path)

# =============================================================================
#  Category: Sensing
# =============================================================================

@mcp.tool()
def get_widget_tree() -> Dict[str, Any]:
    """
    "What does this UI look like?" - Fetches the complete widget hierarchy, which is the foundation for all AI reasoning.
    """
    conn = get_unreal_connection()
    umg_get_client = UMGGet.UMGGet(conn)
    
    # Resolve Path
    final_path = context_manager.get_target()
    if not final_path:
         return {"status": "error", "error": "No target UMG asset set. Use 'set_target_umg_asset' first."}

    return umg_get_client.get_widget_tree(final_path)

@mcp.tool()
def query_widget_properties(widget_name: str, properties: List[str]) -> Dict[str, Any]:
    """
    Queries a list of specific properties from a single widget by its name (e.g., 'Slot.Size').
    """
    conn = get_unreal_connection()
    umg_get_client = UMGGet.UMGGet(conn)
    
    # Resolve Path
    final_path = context_manager.get_target()
    if not final_path:
         return {"status": "error", "error": "No target UMG asset set. Use 'set_target_umg_asset' first."}

    return umg_get_client.query_widget_properties(final_path, widget_name, properties)

@mcp.tool()
def get_layout_data(resolution_width: int = 1920, resolution_height: int = 1080) -> Dict[str, Any]:
    """
    Gets screen-space bounding boxes for all widgets in the asset at a given resolution.
    """
    conn = get_unreal_connection()
    umg_get_client = UMGGet.UMGGet(conn)
    
    # Resolve Path
    final_path = context_manager.get_target()
    if not final_path:
         return {"status": "error", "error": "No target UMG asset set. Use 'set_target_umg_asset' first."}

    return umg_get_client.get_layout_data(final_path, resolution_width, resolution_height)

@mcp.tool()
def check_widget_overlap(widget_names: Optional[List[str]] = None) -> Dict[str, Any]:
    """
    "Are any widgets overlapping?" - Efficiently checks for layout overlap between widgets in the asset.
    """
    conn = get_unreal_connection()
    umg_get_client = UMGGet.UMGGet(conn)
    
    # Resolve Path
    final_path = context_manager.get_target()
    if not final_path:
         return {"status": "error", "error": "No target UMG asset set. Use 'set_target_umg_asset' first."}

    return umg_get_client.check_widget_overlap(final_path, widget_names)

# =============================================================================
#  Category: Action
# =============================================================================

@mcp.tool()
def create_widget(parent_name: str, widget_type: str, new_widget_name: str) -> Dict[str, Any]:
    """
    Creates a new widget and attaches it to a parent.
    AI HINT: If 'asset_path' is omitted, it uses the globally targeted asset. Use 'get_creatable_widget_types' for 'widget_type'.
    """
    conn = get_unreal_connection()
    
    # Resolve Path
    final_path = context_manager.get_target()
    if not final_path:
        resp = conn.send_command("get_target_umg_asset")
        if resp.get("status") == "success":
            data = resp.get("result", {}).get("data", {})
            if data and data.get("asset_path"):
                final_path = data.get("asset_path")
                context_manager.set_target(final_path)
            else:
                return {"status": "error", "error": "No target UMG asset set. Use 'set_target_umg_asset' first."}
        else:
            return {"status": "error", "error": "Failed to get target UMG asset from Unreal."}

    umg_set_client = UMGSet.UMGSet(conn)
    result = umg_set_client.create_widget(final_path, parent_name, widget_type, new_widget_name)
    
    # Invalidate Cache on Modification
    if result.get("status") == "success":
        context_manager.invalidate_cache()
        
    return result

@mcp.tool()
def set_widget_properties(widget_name: str, properties: Dict[str, Any]) -> Dict[str, Any]:
    """
    Sets one or more properties on a specific widget by its name in the currently targeted UMG asset. This is your primary modification tool.
    AI HINT: Use get_widget_schema to see available properties.
    """
    conn = get_unreal_connection()
    
    # Resolve Path
    final_path = context_manager.get_target()
    if not final_path:
        return {"status": "error", "error": "No target UMG asset set. Use 'set_target_umg_asset' first."}

    umg_set_client = UMGSet.UMGSet(conn)
    return umg_set_client.set_widget_properties(final_path, widget_name, properties)

@mcp.tool()
def delete_widget(widget_name: str) -> Dict[str, Any]:
    """
    Deletes a widget by its name from the UMG asset.
    AI HINT: If 'asset_path' is omitted, it uses the globally targeted asset.
    """
    conn = get_unreal_connection()
    
    # Resolve Path
    final_path = context_manager.get_target()
    if not final_path:
        return {"status": "error", "error": "No target UMG asset set. Use 'set_target_umg_asset' first."}

    umg_set_client = UMGSet.UMGSet(conn)
    return umg_set_client.delete_widget(final_path, widget_name)

@mcp.tool()
def reparent_widget(widget_name: str, new_parent_name: str) -> Dict[str, Any]:
    """
    Moves a widget to be a child of a different parent.
    AI HINT: If 'asset_path' is omitted, it uses the globally targeted asset.
    """
    conn = get_unreal_connection()
    
    # Resolve Path
    final_path = context_manager.get_target()
    if not final_path:
        return {"status": "error", "error": "No target UMG asset set. Use 'set_target_umg_asset' first."}

    umg_set_client = UMGSet.UMGSet(conn)
    return umg_set_client.reparent_widget(final_path, widget_name, new_parent_name)

@mcp.tool()
def save_asset() -> Dict[str, Any]:
    """
    "Save my work" - Saves the UMG asset to disk.
    AI HINT: If 'asset_path' is omitted, it uses the globally targeted asset.
    """
    conn = get_unreal_connection()
    
    # Resolve Path
    final_path = context_manager.get_target()
    if not final_path:
        return {"status": "error", "error": "No target UMG asset set. Use 'set_target_umg_asset' first."}

    umg_set_client = UMGSet.UMGSet(conn)
    return umg_set_client.save_asset(final_path)

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
def apply_layout(layout_content: str) -> Dict[str, Any]:
    """
    "Design the UI" - Applies a layout definition to a UMG asset.
    
    **Smart Format Detection**:
    - If content starts with `<`: Treated as **HTML/XML**.
    - If content starts with `{`: Treated as **JSON**.
    
    Args:
        layout_content: The HTML or JSON string defining the widget tree.
    """
    # 1. Determine Format
    content = layout_content.strip()
    is_html = content.startswith("<")
    
    # 2. Parse if HTML
    json_data = None
    if is_html:
        try:
            parser = UMGHTMLParser.UMGHTMLParser()
            json_data = parser.parse(content)
        except Exception as e:
            return {"status": "error", "error": f"HTML Parsing Failed: {str(e)}"}
    else:
        try:
            json_data = json.loads(content)
        except json.JSONDecodeError as e:
             return {"status": "error", "error": f"JSON Parsing Failed: {str(e)}"}

    # 3. Apply
    conn = get_unreal_connection()
    
    # Resolve Path
    final_path = context_manager.get_target()
    if not final_path:
        # Try to get currently open asset from Unreal
        resp = conn.send_command("get_target_umg_asset")
        if resp.get("status") == "success":
            data = resp.get("result", {}).get("data", {})
            if data:
                final_path = data.get("asset_path")
                if final_path:
                    context_manager.set_target(final_path)
    
    if not final_path:
        return {"status": "error", "error": "No target UMG asset set. Use 'set_target_umg_asset' first."}
    
    logger.info(f"Resolved target asset path: {final_path}")
    
    umg_trans_client = UMGFileTransformation.UMGFileTransformation(conn)
    return umg_trans_client.apply_json_to_umg(final_path, json_data)

# Deprecated: Kept for backward compatibility
@mcp.tool()
def apply_json_to_umg(asset_path: str, json_data: Dict[str, Any]) -> Dict[str, Any]:
    """[Deprecated] Use apply_layout instead."""
    return apply_layout(json.dumps(json_data), asset_path)

# @mcp.tool()
def apply_html_to_umg(asset_path: str, html_content: str) -> Dict[str, Any]:
    """[Deprecated] Use apply_layout instead."""
    return apply_layout(html_content, asset_path)

# @mcp.tool()
def preview_html_conversion(html_content: str) -> Dict[str, Any]:
    """
    "Debug HTML" - Returns the UMG JSON that WOULD be generated from the given HTML.
    Use this to verify how your HTML tags map to UMG properties before applying.
    """
    try:
        parser = UMGHTMLParser.UMGHTMLParser()
        json_data = parser.parse(html_content)
        return {"status": "success", "data": json_data}
    except Exception as e:
        return {"status": "error", "error": f"HTML Parsing Failed: {str(e)}"}

# =============================================================================
#  Category: Editor & Level (New)
# =============================================================================

# @mcp.tool()
def refresh_asset_registry(paths: Optional[List[str]] = None) -> Dict[str, Any]:
    """
    "I can't find the file!" - Forces Unreal to re-scan the disk for new assets.
    AI HINT: Use this if you created a file (like a Blueprint) but cannot find it immediately.
    """
    conn = get_unreal_connection()
    editor_client = UMGEditor.UMGEditor(conn)
    return editor_client.refresh_asset_registry(paths)

# @mcp.tool()
def get_actors_in_level() -> Dict[str, Any]:
    """
    "What's in the scene?" - Gets a list of all actors in the current level.
    """
    conn = get_unreal_connection()
    editor_client = UMGEditor.UMGEditor(conn)
    return editor_client.get_actors_in_level()

# @mcp.tool()
def spawn_actor(actor_type: str, name: str, location: List[float] = None, rotation: List[float] = None) -> Dict[str, Any]:
    """
    Spawns a basic actor (StaticMeshActor, PointLight, etc.) in the level.
    """
    conn = get_unreal_connection()
    editor_client = UMGEditor.UMGEditor(conn)
    return editor_client.spawn_actor(actor_type, name, location, rotation)

# =============================================================================
#  Category: Blueprint (New)
# =============================================================================

# @mcp.tool()
def create_blueprint(name: str, parent_class: str = "AActor") -> Dict[str, Any]:
    """
    Creates a new Blueprint asset.
    """
    conn = get_unreal_connection()
    bp_client = UMGBlueprint.UMGBlueprint(conn)
    return bp_client.create_blueprint(name, parent_class)

# @mcp.tool()
def compile_blueprint(blueprint_name: str) -> Dict[str, Any]:
    """
    Compiles a Blueprint.
    """
    conn = get_unreal_connection()
    bp_client = UMGBlueprint.UMGBlueprint(conn)
    return bp_client.compile_blueprint(blueprint_name)

# =============================================================================

# =============================================================================
#  Category: Animation & Sequencer (New)
# =============================================================================

@mcp.tool()
def get_all_animations() -> Dict[str, Any]:
    """
    "What animations are there?" - Lists all animations in the current UMG asset.
    """
    conn = get_unreal_connection()
    
    # Resolve Path
    final_path = context_manager.get_target()
    if not final_path:
        return {"status": "error", "error": "No target UMG asset set. Use 'set_target_umg_asset' first."}

    sequencer_client = UMGSequencer.UMGSequencer(conn)
    return sequencer_client.get_all_animations(final_path)

@mcp.tool()
def create_animation(animation_name: str) -> Dict[str, Any]:
    """
    Creates a new animation in the current UMG asset.
    """
    conn = get_unreal_connection()
    
    # Resolve Path
    final_path = context_manager.get_target()
    if not final_path:
        return {"status": "error", "error": "No target UMG asset set. Use 'set_target_umg_asset' first."}

    sequencer_client = UMGSequencer.UMGSequencer(conn)
    return sequencer_client.create_animation(final_path, animation_name)

@mcp.tool()
def delete_animation(animation_name: str) -> Dict[str, Any]:
    """
    Deletes an animation from the current UMG asset.
    """
    conn = get_unreal_connection()
    
    # Resolve Path
    final_path = context_manager.get_target()
    if not final_path:
        return {"status": "error", "error": "No target UMG asset set. Use 'set_target_umg_asset' first."}

    sequencer_client = UMGSequencer.UMGSequencer(conn)
    return sequencer_client.delete_animation(final_path, animation_name)

@mcp.tool()
def focus_animation(animation_name: str) -> Dict[str, Any]:
    """
    "I'm working on this animation." - Sets the context to a specific animation.
    Subsequent commands like 'add_key' will use this animation by default.
    """
    conn = get_unreal_connection()
    sequencer_client = UMGSequencer.UMGSequencer(conn)
    return sequencer_client.focus_animation(animation_name)

@mcp.tool()
def focus_widget(widget_name: str) -> Dict[str, Any]:
    """
    "I'm animating this widget." - Sets the context to a specific widget track.
    Subsequent commands like 'add_key' will use this widget by default.
    """
    conn = get_unreal_connection()
    sequencer_client = UMGSequencer.UMGSequencer(conn)
    return sequencer_client.focus_widget(widget_name)

@mcp.tool()
def add_track(property_name: str, animation_name: Optional[str] = None, widget_name: Optional[str] = None) -> Dict[str, Any]:
    """
    Adds a track for a specific widget property to an animation.
    AI HINT: If animation_name or widget_name are omitted, uses the focused context.
    """
    conn = get_unreal_connection()
    
    # Resolve Path
    final_path = context_manager.get_target()
    if not final_path:
        return {"status": "error", "error": "No target UMG asset set. Use 'set_target_umg_asset' first."}

    sequencer_client = UMGSequencer.UMGSequencer(conn)
    return sequencer_client.add_track(final_path, animation_name, widget_name, property_name)

@mcp.tool()
def add_key(property_name: str, time: float, value: float, animation_name: Optional[str] = None, widget_name: Optional[str] = None) -> Dict[str, Any]:
    """
    Adds a keyframe to a track at a specific time.
    AI HINT: If animation_name or widget_name are omitted, uses the focused context.
    """
    conn = get_unreal_connection()
    
    # Resolve Path
    final_path = context_manager.get_target()
    if not final_path:
        return {"status": "error", "error": "No target UMG asset set. Use 'set_target_umg_asset' first."}

    sequencer_client = UMGSequencer.UMGSequencer(conn)
    return sequencer_client.add_key(final_path, animation_name, widget_name, property_name, time, value)

# =============================================================================

@mcp.prompt()
def info():
    """Run method:"tools/list" to get more details."""
    return """
# UMG MCP API - v3.0 (English)

This document lists all available tools for interacting with the UMG MCP server.

## 1. Sensing - The "Eyes" of the AI

*   `get_widget_tree()`
    *   **Purpose**: Get the complete hierarchy of the UI. **This is the core sensing API**.
    *   **Workflow**: Operates on the global target set by `set_target_umg_asset`.

*   `query_widget_properties(widget_name: str, properties: List[str])`
    *   **Purpose**: Query specific property values of a widget.
    *   **Workflow**: Same as above.

*   `get_layout_data(resolution_width: int = 1920, resolution_height: int = 1080)`
    *   **Purpose**: Get screen coordinates and size for all widgets at a given resolution.
    *   **Workflow**: Same as above.

*   `check_widget_overlap(widget_names: Optional[List[str]] = None)`
    *   **Purpose**: Efficient backend check for widget overlaps.
    *   **Workflow**: Same as above.

## 2. Action - The "Hands" of the AI

*   `create_widget(parent_name: str, widget_type: str, new_widget_name: str)`
    *   **Purpose**: Create a new widget under a specified parent.
    *   **Workflow**: Same as above.

*   `delete_widget(widget_name: str)`
    *   **Purpose**: Delete a specified widget.
    *   **Workflow**: Same as above.

*   `set_widget_properties(widget_name: str, properties: Dict[str, Any])`
    *   **Purpose**: Modify one or more properties of a widget. **This is the core modification API**.
    *   **Workflow**: Same as above.

*   `reparent_widget(widget_name: str, new_parent_name: str)`
    *   **Purpose**: Move a widget from one parent to another.
    *   **Workflow**: Same as above.

## 3. Introspection & Context

*   `get_creatable_widget_types()`
    *   **Purpose**: Tells the AI what types of widgets are in its "toolbox".

*   `get_widget_schema(widget_type)`
    *   **Purpose**: Tells the AI what editable properties exist for a widget type.

*   `set_target_umg_asset(asset_path: str)`
    *   **Purpose**: Sets a global target. All subsequent commands without `asset_path` will use this target.

*   `get_target_umg_asset()`
    *   **Purpose**: Query the current global target.

## 4. File Transformation

*   `export_umg_to_json(asset_path: str)`
    *   **Purpose**: "Decompile" a UMG `.uasset` to JSON. Requires explicit path.

*   `apply_layout(layout_content: str, asset_path: Optional[str] = None)`
    *   **Purpose**: Apply a JSON or HTML layout definition to a UMG asset.

## 5. Animation & Sequencer (New)

*   `get_all_animations()`
    *   **Purpose**: List all animations.

*   `create_animation(animation_name: str)`
    *   **Purpose**: Create a new animation.

*   `delete_animation(animation_name: str)`
    *   **Purpose**: Delete an animation.

*   `add_track(animation_name: str, widget_name: str, property_name: str)`
    *   **Purpose**: Add a property track to an animation.

*   `add_key(animation_name: str, widget_name: str, property_name: str, time: float, value: float)`
    *   **Purpose**: Add a keyframe to a track.

## 6. Editor & Blueprint (New)

(Hidden to avoid confusion, strictly UMG focused)

## Pro-Tip for AI Assistants

You can work as a powerful assistant by observing user changes. Use `export_umg_to_json` to 'see' what the user has done. Then, you can help them by using `apply_layout` (or `set_widget_properties`) to apply normalized data, such as cleaning up variable names, refining text, or fine-tuning transform values.
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