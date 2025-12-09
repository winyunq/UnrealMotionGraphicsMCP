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
import sys
import os

from contextlib import asynccontextmanager
from typing import AsyncIterator, Dict, Any, Optional, List
from mcp.server.fastmcp import FastMCP

# Import configuration from our dedicated config file
from mcp_config import UNREAL_HOST, UNREAL_PORT

# Import the modular clients
from FileManage import UMGAttention
from Widget import UMGGet
from Widget import UMGSet
from FileManage import UMGFileTransformation
from Bridge import UMGHTMLParser
from Editor import UMGEditor
from Blueprint import UMGBlueprint


# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - [%(filename)s:%(lineno)d] - %(message)s',
    handlers=[ logging.FileHandler('umg_mcp_server.log') ]
)
logger = logging.getLogger("UmgMcpServer")

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
        """Receive a complete response from Unreal, handling chunked data and delimiters."""
        chunks = []
        sock.settimeout(0.3)  # 0.3 second timeout as requested
        try:
            while True:
                chunk = sock.recv(buffer_size)
                if not chunk:
                    if not chunks:
                        raise Exception("Connection closed before receiving data")
                    break
                chunks.append(chunk)
                
                # Check for null byte delimiter in the raw chunk
                if b'\0' in chunk:
                    # We found the end.
                    # Combine all chunks
                    data = b''.join(chunks)
                    
                    # Split by null byte
                    parts = data.split(b'\0')
                    valid_json_bytes = parts[0]
                    
                    logger.info(f"Received complete response with null delimiter ({len(valid_json_bytes)} bytes)")
                    return valid_json_bytes
                
        except socket.timeout:
            logger.warning("Socket timeout during receive")
            if chunks:
                # If we have some data already, try to use it
                data = b''.join(chunks)
                if b'\0' in data:
                     parts = data.split(b'\0')
                     return parts[0]
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
            
            # Send with null delimiter
            command_json = json.dumps(command_obj)
            logger.debug(f"Sending command: {command_json.strip()}")
            
            # Send JSON + Null Byte
            self.socket.sendall(command_json.encode('utf-8') + b'\0')
            
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
#  Dynamic Tool Configuration (Loaded from prompts.json)
# =============================================================================

PROMPTS_CONFIG = []

def load_tool_and_prompt_config():
    """Load tool definitions and prompts from prompts.json."""
    global TOOLS_CONFIG, PROMPTS_CONFIG
    try:
        current_dir = os.path.dirname(os.path.abspath(__file__))
        json_path = os.path.abspath(os.path.join(current_dir, "..", "prompts.json"))
        
        if os.path.exists(json_path):
            with open(json_path, 'r', encoding='utf-8') as f:
                data = json.load(f)
                
                # Load Tools
                for tool in data.get("tools", []):
                    TOOLS_CONFIG[tool["name"]] = tool
                    
                # Load Prompts
                PROMPTS_CONFIG = data.get("prompts", [])
                
            logger.info(f"Loaded {len(TOOLS_CONFIG)} tools and {len(PROMPTS_CONFIG)} prompts from prompts.json")
        else:
            logger.warning(f"prompts.json not found at {json_path}")
            
    except Exception as e:
        logger.error(f"Error loading prompts.json: {e}")

load_tool_and_prompt_config()

# Register Prompts dynamically
for p_data in PROMPTS_CONFIG:
    p_name = p_data.get("name", "Unknown")
    p_content = p_data.get("content", "")
    p_desc = p_data.get("description", "")
    
    # Create closure to capture content
    def make_prompt_func(content):
        def prompt_func():
            return content
        return prompt_func
        
    safe_name = p_name.lower().replace(" ", "_").replace("-", "_")
    mcp.prompt(name=safe_name, description=p_desc)(make_prompt_func(p_content))
    


def register_tool(name: str, default_desc: str):
    """
    Decorator to register a tool with MCP only if it is enabled in prompts.json.
    This effectively compresses the context by hiding disabled tools from the AI.
    """
    def decorator(func):
        config = TOOLS_CONFIG.get(name, {})
        is_enabled = config.get("enabled", True)
        if is_enabled:
            description = config.get("description", default_desc)
            # Register with FastMCP
            return mcp.tool(name=name, description=description)(func)
        else:
            # Do not register, just return the function
            return func
    return decorator

# =============================================================================
#  Category: Introspection (Knowledge Base)
# =============================================================================

@register_tool("get_widget_schema", "Retrieves the schema for a widget type.")
def get_widget_schema(widget_type: str) -> Dict[str, Any]:
    """
    (Description loaded from prompts.json)
    """
    conn = get_unreal_connection()
    umg_get_client = UMGGet.UMGGet(conn)
    return umg_get_client.get_widget_schema(widget_type)

@register_tool("get_creatable_widget_types", "Returns a list of creatable widget types.")
def get_creatable_widget_types() -> Dict[str, Any]:
    """
    (Description loaded from prompts.json)
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

@register_tool("get_target_umg_asset", "Gets the asset path of the UMG Editor user is focused on.")
def get_target_umg_asset() -> Dict[str, Any]:
    """
    (Description loaded from prompts.json)
    """
    conn = get_unreal_connection()
    umg_attention_client = UMGAttention.UMGAttention(conn)
    return umg_attention_client.get_target_umg_asset()

@register_tool("get_last_edited_umg_asset", "Gets the last edited UMG asset path.")
def get_last_edited_umg_asset() -> Dict[str, Any]:
    """
    (Description loaded from prompts.json)
    """
    conn = get_unreal_connection()
    umg_attention_client = UMGAttention.UMGAttention(conn)
    return umg_attention_client.get_last_edited_umg_asset()

@register_tool("get_recently_edited_umg_assets", "Gets a list of recently edited assets.")
def get_recently_edited_umg_assets(max_count: int = 5) -> Dict[str, Any]:
    """
    (Description loaded from prompts.json)
    """
    conn = get_unreal_connection()
    umg_attention_client = UMGAttention.UMGAttention(conn)
    return umg_attention_client.get_recently_edited_umg_assets(max_count)

@register_tool("set_target_umg_asset", "Sets the target UMG asset.")
def set_target_umg_asset(asset_path: str) -> Dict[str, Any]:
    """
    (Description loaded from prompts.json)
    """
    context_manager.set_target(asset_path) # Update local context
    conn = get_unreal_connection()
    umg_attention_client = UMGAttention.UMGAttention(conn)
    return umg_attention_client.set_target_umg_asset(asset_path)

# =============================================================================
#  Category: Sensing
# =============================================================================

@register_tool("get_widget_tree", "Fetches the complete widget hierarchy.")
def get_widget_tree() -> Dict[str, Any]:
    """
    (Description loaded from prompts.json)
    """
    conn = get_unreal_connection()
    umg_get_client = UMGGet.UMGGet(conn)
    
    # Note: We rely on the implicit context on the server/plugin side now.
    # But for robustness, we can still check if we have a local context to warn early,
    # or just let the plugin handle it.
    # Given the user's strong preference for implicit defaults, we just call the method.
    
    return umg_get_client.get_widget_tree()

@register_tool("query_widget_properties", "Queries specific properties of a widget.")
def query_widget_properties(widget_name: str, properties: List[str]) -> Dict[str, Any]:
    """
    (Description loaded from prompts.json)
    """
    conn = get_unreal_connection()
    umg_get_client = UMGGet.UMGGet(conn)
    return umg_get_client.query_widget_properties(widget_name, properties)

@register_tool("get_layout_data", "Gets bounding boxes for widgets.")
def get_layout_data(resolution_width: int = 1920, resolution_height: int = 1080) -> Dict[str, Any]:
    """
    (Description loaded from prompts.json)
    """
    conn = get_unreal_connection()
    umg_get_client = UMGGet.UMGGet(conn)
    return umg_get_client.get_layout_data(resolution_width, resolution_height)

@register_tool("check_widget_overlap", "Checks for widget overlap.")
def check_widget_overlap(widget_names: Optional[List[str]] = None) -> Dict[str, Any]:
    """
    (Description loaded from prompts.json)
    """
    conn = get_unreal_connection()
    umg_get_client = UMGGet.UMGGet(conn)
    return umg_get_client.check_widget_overlap(widget_names)

# =============================================================================
#  Category: Action
# =============================================================================

@register_tool("create_widget", "Creates a new widget.")
def create_widget(parent_name: str, widget_type: str, new_widget_name: str) -> Dict[str, Any]:
    """
    (Description loaded from prompts.json)
    """
    conn = get_unreal_connection()
    umg_set_client = UMGSet.UMGSet(conn)
    result = umg_set_client.create_widget(widget_type, new_widget_name, parent_name)
    
    # Invalidate Cache on Modification
    if result.get("status") == "success":
        # context_manager.invalidate_cache() # If we had one
        pass
        
    return result

@register_tool("set_widget_properties", "Sets properties on a widget.")
def set_widget_properties(widget_name: str, properties: Dict[str, Any]) -> Dict[str, Any]:
    """
    (Description loaded from prompts.json)
    """
    conn = get_unreal_connection()
    umg_set_client = UMGSet.UMGSet(conn)
    return umg_set_client.set_widget_properties(widget_name, properties)

@register_tool("delete_widget", "Deletes a widget.")
def delete_widget(widget_name: str) -> Dict[str, Any]:
    """
    (Description loaded from prompts.json)
    """
    conn = get_unreal_connection()
    umg_set_client = UMGSet.UMGSet(conn)
    return umg_set_client.delete_widget(widget_name)

@register_tool("reparent_widget", "Moves a widget to a new parent.")
def reparent_widget(widget_name: str, new_parent_name: str) -> Dict[str, Any]:
    """
    (Description loaded from prompts.json)
    """
    conn = get_unreal_connection()
    umg_set_client = UMGSet.UMGSet(conn)
    return umg_set_client.reparent_widget(widget_name, new_parent_name)

@register_tool("save_asset", "Saves the UMG asset.")
def save_asset() -> Dict[str, Any]:
    """
    (Description loaded from prompts.json)
    """
    conn = get_unreal_connection()
    umg_set_client = UMGSet.UMGSet(conn)
    return umg_set_client.save_asset()

# =============================================================================
#  Category: File Transformation (Explicit Path)
# =============================================================================

@register_tool("export_umg_to_json", "Decompiles UMG to JSON.")
def export_umg_to_json(asset_path: str) -> Dict[str, Any]:
    """
    (Description loaded from prompts.json)
    """
    conn = get_unreal_connection()
    umg_file_client = UMGFileTransformation.UMGFileTransformation(conn)
    return umg_file_client.export_umg_to_json(asset_path)

@register_tool("apply_layout", "Applies a layout logic to a UMG asset.")
def apply_layout(layout_content: str) -> Dict[str, Any]:
    """
    (Description loaded from prompts.json)
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
    return apply_layout(json.dumps(json_data))

# @mcp.tool()
def apply_html_to_umg(asset_path: str, html_content: str) -> Dict[str, Any]:
    """[Deprecated] Use apply_layout instead."""
    return apply_layout(html_content)

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

@register_tool("refresh_asset_registry", "Forces asset registry rescan.")
def refresh_asset_registry(paths: Optional[List[str]] = None) -> Dict[str, Any]:
    """
    (Description loaded from prompts.json)
    """
    conn = get_unreal_connection()
    editor_client = UMGEditor.UMGEditor(conn)
    return editor_client.refresh_asset_registry(paths)

@register_tool("get_actors_in_level", "Lists actors in level.")
def get_actors_in_level() -> Dict[str, Any]:
    """
    (Description loaded from prompts.json)
    """
    conn = get_unreal_connection()
    editor_client = UMGEditor.UMGEditor(conn)
    return editor_client.get_actors_in_level()

@register_tool("spawn_actor", "Spawns an actor.")
def spawn_actor(actor_type: str, name: str, location: List[float] = None, rotation: List[float] = None) -> Dict[str, Any]:
    """
    (Description loaded from prompts.json)
    """
    conn = get_unreal_connection()
    editor_client = UMGEditor.UMGEditor(conn)
    return editor_client.spawn_actor(actor_type, name, location, rotation)

# =============================================================================
#  Category: Blueprint (New)
# =============================================================================

@register_tool("create_blueprint", "Creates a Blueprint.")
def create_blueprint(name: str, parent_class: str = "AActor") -> Dict[str, Any]:
    """
    (Description loaded from prompts.json)
    """
    conn = get_unreal_connection()
    bp_client = UMGBlueprint.UMGBlueprint(conn)
    return bp_client.create_blueprint(name, parent_class)

@register_tool("compile_blueprint", "Compiles a Blueprint.")
def compile_blueprint(blueprint_name: str) -> Dict[str, Any]:
    """
    (Description loaded from prompts.json)
    """
    conn = get_unreal_connection()
    bp_client = UMGBlueprint.UMGBlueprint(conn)
    return bp_client.compile_blueprint(blueprint_name)

# =============================================================================



# =============================================================================

# =============================================================================
#  Category: Dynamic Prompts (Loaded from JSON)
# =============================================================================

def load_prompts():
    """Load prompts from the JSON file and register them with MCP."""
    try:
        # Resolve path relative to this script
        current_dir = os.path.dirname(os.path.abspath(__file__))
        json_path = os.path.abspath(os.path.join(current_dir, "..", "prompts.json"))
        
        if not os.path.exists(json_path):
            logger.warning(f"prompts.json not found at {json_path}")
            return

        with open(json_path, 'r', encoding='utf-8') as f:
            data = json.load(f)

        # 1. Register Action Prompts (from specific categories)
        target_categories = ["UMG Editor", "Blueprint Logic"]
        
        for category in target_categories:
            if category in data:
                for item in data[category]:
                    name = item.get("name", "Unknown")
                    prompt_text = item.get("prompt", "")
                    
                    # Store in a closure to capture 'prompt_text'
                    def make_prompt_func(text_content):
                        def p_func():
                            return text_content
                        return p_func

                    # Register with MCP
                    # Sanitize name for ID if needed, but FastMCP usually allows spaces in decorator? 
                    # FastMCP uses the function name as the tool name by default, OR the 'name' argument.
                    # We'll use the 'name' argument.
                    safe_name = name.lower().replace(" ", "_")
                    mcp.prompt(name=safe_name, description=name)(make_prompt_func(prompt_text))
                    logger.info(f"Registered prompt: {safe_name}")

        # 2. Register System Info (Documentation)
        # We look for "Server Documentation" -> "UMG Info"
        doc_category = data.get("Server Documentation", [])
        for item in doc_category:
            if item.get("name") == "UMG Info":
                info_text = item.get("prompt", "No documentation found.")
                
                @mcp.prompt(name="info")
                def info():
                    """Run method:"tools/list" to get more details."""
                    return info_text
                
                logger.info("Registered 'info' prompt from JSON.")
                break

    except Exception as e:
        logger.error(f"Failed to load prompts from JSON: {e}")

# Load prompts at module level (or just before run)
load_prompts()

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