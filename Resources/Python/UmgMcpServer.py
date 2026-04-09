"""
UmgMcp Server - Your AI UMG Design Partner (v3.0 - Context-Aware)

This MCP server is designed exclusively for interacting with the UmgMcp plugin
in Unreal Engine 5. It provides a complete and verified suite of tools for an AI
assistant to sense, understand, and manipulate UMG assets programmatically.

This version introduces a context-aware workflow. The AI first sets a target
UMG asset using the 'Attention' tools, and all subsequent 'Sensing' and 'Action'
tools operate implicitly on that target.
"""

# Force unbuffered stdout to ensure MCP JSON-RPC messages are flushed immediately
# This fixes the issue where Gemini CLI (via pipe) waits for buffer fill before processing responses.
import sys
try:
    if hasattr(sys, 'stdout') and hasattr(sys.stdout, 'reconfigure'):
        sys.stdout.reconfigure(line_buffering=True)
except Exception as e:
    pass

import logging
import asyncio
import json
import os

from contextlib import asynccontextmanager
from typing import AsyncIterator, Dict, Any, Optional, List
from mcp.server.fastmcp import FastMCP

# Import configuration from our dedicated config file
from mcp_config import UNREAL_HOST, UNREAL_PORT, SOCKET_TIMEOUT

# Import the modular clients
from FileManage import UMGAttention
from Widget import UMGGet
from Widget import UMGSet
from FileManage import UMGFileTransformation
from Bridge import UMGHTMLParser
from Editor import UMGEditor
from Material import UMGMaterial



# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - [%(filename)s:%(lineno)d] - %(message)s',
    handlers=[ logging.FileHandler('umg_mcp_server.log') ]
)
logger = logging.getLogger("UmgMcpServer")

class UnrealConnection:
    """Manages the async socket connection to the UmgMcp plugin running inside Unreal Engine."""
    def __init__(self):
        logger.info(f"Unreal Motion Graphics UI Designer Mode Context Process Launching... Connecting to UmgMcp plugin at {UNREAL_HOST}:{UNREAL_PORT}...")
    
    async def send_command(self, command: str, params: Dict[str, Any] = None) -> Optional[Dict[str, Any]]:
        """Send a command to Unreal Engine and get the response (Async Short-Lived)."""
        reader = None
        writer = None
        
        try:
            logger.info(f"Connecting to Unreal at {UNREAL_HOST}:{UNREAL_PORT}...")
            # Async Open Connection (IOCP on Windows)
            reader, writer = await asyncio.open_connection(UNREAL_HOST, UNREAL_PORT)
            
            # Match Unity's command format exactly
            command_obj = {
                "command": command,
                "params": params or {}
            }
            
            # Send with null delimiter
            command_json = json.dumps(command_obj)
            logger.info(f"[UMGMCP-Message] Sending: {command_json.strip()[:200]}... (truncated)")
            
            # DEBUG-SOCKET:
            sys.stderr.write(f"DEBUG: Async Sending {len(command_json)} bytes...\n")
            sys.stderr.flush()

            # Write data + null byte
            writer.write(command_json.encode('utf-8') + b'\0')
            await writer.drain()
            
            # DEBUG-SOCKET:
            sys.stderr.write("DEBUG: Async Drain completed.\n")
            sys.stderr.flush()

            # Force Flush / Shutdown Write to signal end of stream
            if writer.can_write_eof():
                writer.write_eof()
                sys.stderr.write("DEBUG: Async write_eof() called.\n")
            
            # Read response
            sys.stderr.write("DEBUG: Async Waiting for response (read 4096 chunks)...\n")
            sys.stderr.flush()
            
            chunks = []
            while True:
                # Read chunk
                chunk = await reader.read(4096)
                if not chunk:
                    break # EOF
                
                if b'\x00' in chunk:
                    chunks.append(chunk[:chunk.find(b'\x00')])
                    break
                chunks.append(chunk)
            
            response_data = b"".join(chunks)
            
            sys.stderr.write(f"DEBUG: Async Received {len(response_data)} bytes.\n")
            sys.stderr.flush()

            response_str = response_data.decode('utf-8')
            logger.info(f"[UMGMCP-Message] Received: {response_str}")
            
            if not response_str:
                raise ConnectionError("Empty response from Unreal")

            response = json.loads(response_str)
            
            # Log complete response for debugging
            logger.info(f"Complete response from Unreal: {response}")
            
            # Check for error formats
            if response.get("status") == "error":
                error_message = response.get("error") or response.get("message", "Unknown Unreal error")
                logger.error(f"Unreal error (status=error): {error_message}")
                if "error" not in response:
                    response["error"] = error_message
            elif response.get("success") is False:
                error_message = response.get("error") or response.get("message", "Unknown Unreal error")
                logger.error(f"Unreal error (success=false): {error_message}")
                response = {
                    "status": "error",
                    "error": error_message
                }
            
            return response
            
        except Exception as e:
            logger.error(f"Error sending command: {e}")
            return {"status": "error", "error": str(e)}
            
        finally:
            # Clean Close
            if writer:
                try:
                    writer.close()
                    await writer.wait_closed()
                    sys.stderr.write("DEBUG: Async Socket closed.\n")
                except:
                    pass
            sys.stderr.flush()

class ContextManager:
    """Manages the 'attention' context for the AI."""
    def __init__(self):
        self._target_asset_path: Optional[str] = None
        self._target_widget_name: Optional[str] = None

    def set_target(self, asset_path: Optional[str] = None, widget_name: Optional[str] = None):
        if asset_path is not None:
            self._target_asset_path = asset_path
            logger.info(f"Context asset set to: {asset_path}")
        if widget_name is not None:
            self._target_widget_name = widget_name
            logger.info(f"Context widget set to: {widget_name}")

    def get_target(self) -> Optional[str]:
        return self._target_asset_path

    def get_target_widget(self) -> Optional[str]:
        return self._target_widget_name

    def clear_target(self):
        self._target_asset_path = None
        self._target_widget_name = None
        logger.info("Context cleared")

# Global Context Manager Instance
context_manager = ContextManager()

# Global connection state
_unreal_connection: UnrealConnection = None

def get_unreal_connection() -> Optional[UnrealConnection]:
    """Get the connection to Unreal Engine."""
    global _unreal_connection
    if _unreal_connection is None:
        _unreal_connection = UnrealConnection()
    return _unreal_connection

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
TOOLS_CONFIG = {}

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
async def get_widget_schema(widget_type: str) -> Dict[str, Any]:
    """
    (Description loaded from prompts.json)
    """
    conn = get_unreal_connection()
    umg_get_client = UMGGet.UMGGet(conn)
    return await umg_get_client.get_widget_schema(widget_type)

@register_tool("get_creatable_widget_types", "Returns a list of creatable widget types.")
async def get_creatable_widget_types() -> Dict[str, Any]:
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
async def get_target_umg_asset() -> Dict[str, Any]:
    """
    (Description loaded from prompts.json)
    """
    conn = get_unreal_connection()
    umg_attention_client = UMGAttention.UMGAttention(conn)
    return await umg_attention_client.get_target_umg_asset()

@register_tool("get_target_widget", "Gets the focused widget within the current target asset.")
async def get_target_widget() -> Dict[str, Any]:
    """
    (Description loaded from prompts.json)
    """
    conn = get_unreal_connection()
    umg_attention_client = UMGAttention.UMGAttention(conn)
    resp = await umg_attention_client.get_target_widget()
    if (resp.get("status") == "success" or resp.get("success")) and resp.get("widget_name"):
        context_manager.set_target(context_manager.get_target(), resp.get("widget_name"))
    return resp

@register_tool("get_last_edited_umg_asset", "Gets the last edited UMG asset path.")
async def get_last_edited_umg_asset() -> Dict[str, Any]:
    """
    (Description loaded from prompts.json)
    """
    conn = get_unreal_connection()
    umg_attention_client = UMGAttention.UMGAttention(conn)
    return await umg_attention_client.get_last_edited_umg_asset()

@register_tool("get_recently_edited_umg_assets", "Gets a list of recently edited assets.")
async def get_recently_edited_umg_assets(max_count: int = 5) -> Dict[str, Any]:
    """
    (Description loaded from prompts.json)
    """
    conn = get_unreal_connection()
    umg_attention_client = UMGAttention.UMGAttention(conn)
    return await umg_attention_client.get_recently_edited_umg_assets(max_count)

def normalize_project_path(path: str) -> str:
    """
    Normalizes a project path to the Unreal /Game/ format, actively
    combating directory traversal (../) and enforcing valid roots.
    Example: 'Content/MyWidget' -> '/Game/MyWidget'
    """
    import posixpath
    path = path.replace('\\', '/')
    if path.startswith('Content/'):
        path = '/Game/' + path[8:]
        
    if not path.startswith('/'):
        path = '/Game/' + path

    resolved = posixpath.normpath(path)
    if not resolved.startswith('/'):
        resolved = '/' + resolved
        
    valid_roots = ('/Game/', '/Engine/', '/Script/', '/Plugin/')
    if not any(resolved.startswith(root) for root in valid_roots):
        resolved = '/Game/' + resolved.lstrip('/')
        
    return resolved

@register_tool("set_target_umg_asset", "Sets the target UMG asset.")
async def set_target_umg_asset(asset_path: str, widget_name: Optional[str] = None) -> Dict[str, Any]:
    """
    (Description loaded from prompts.json)
    """
    def split_target(raw_path: str, widget_hint: Optional[str]) -> (Optional[str], Optional[str]):
        raw = (raw_path or "").strip()
        target_widget = widget_hint.strip() if widget_hint else None
        target_path = None

        if ":" in raw:
            left, right = raw.split(":", 1)
            if left.strip():
                target_path = normalize_project_path(left.strip())
            if right.strip():
                target_widget = right.strip()
        elif raw:
            if raw.startswith("/") or raw.startswith("Content/") or raw.startswith("content/"):
                target_path = normalize_project_path(raw)
            else:
                target_widget = target_widget or raw

        return target_path, target_widget

    def command_failed(payload: Dict[str, Any]) -> bool:
        if payload.get("status") == "error":
            return True
        if "success" in payload and payload.get("success") is False:
            return True
        return False

    target_path, target_widget = split_target(asset_path, widget_name)
    conn = get_unreal_connection()
    umg_attention_client = UMGAttention.UMGAttention(conn)
    result: Dict[str, Any] = {"status": "success", "success": True}

    resolved_path = target_path or context_manager.get_target()
    # Resolve path from engine if nothing local is available
    if not resolved_path:
        resp = await umg_attention_client.get_target_umg_asset()
        if not command_failed(resp):
            resolved_path = resp.get("asset_path")
            if resolved_path:
                context_manager.set_target(resolved_path)

    if target_path:
        resp = await umg_attention_client.set_target_umg_asset(target_path, target_widget)
        if command_failed(resp):
            return resp
        resolved_path = resp.get("asset_path") or target_path
        result.update(resp)
    elif not resolved_path:
        return {"status": "error", "error": "No target UMG asset set. Provide an asset path or set one first."}

    if target_widget:
        focus_resp = await umg_attention_client.set_target_widget(target_widget)
        if command_failed(focus_resp):
            result["focus_warning"] = focus_resp.get("error")
        result["widget_name"] = target_widget

    context_manager.set_target(resolved_path, target_widget)
    result.setdefault("asset_path", resolved_path)
    return result

@register_tool("set_target_widget", "Sets the focused widget within the current target asset.")
async def set_target_widget(widget_name: str) -> Dict[str, Any]:
    """
    (Description loaded from prompts.json)
    """
    conn = get_unreal_connection()
    umg_attention_client = UMGAttention.UMGAttention(conn)
    resp = await umg_attention_client.set_target_widget(widget_name)
    if resp.get("status") == "success" or resp.get("success"):
        context_manager.set_target(context_manager.get_target(), widget_name)
    return resp

# =============================================================================
#  Category: Sensing
# =============================================================================

@register_tool("get_widget_tree", "Fetches the complete widget hierarchy.")
async def get_widget_tree() -> Dict[str, Any]:
    """
    (Description loaded from prompts.json)
    """
    conn = get_unreal_connection()
    umg_get_client = UMGGet.UMGGet(conn)
    
    # Note: We rely on the implicit context on the server/plugin side now.
    # But for robustness, we can still check if we have a local context to warn early,
    # or just let the plugin handle it.
    # Given the user's strong preference for implicit defaults, we just call the method.
    
    return await umg_get_client.get_widget_tree()

@register_tool("query_widget_properties", "Queries specific properties of a widget.")
async def query_widget_properties(widget_name: str, properties: List[str]) -> Dict[str, Any]:
    """
    (Description loaded from prompts.json)
    """
    conn = get_unreal_connection()
    umg_get_client = UMGGet.UMGGet(conn)
    return await umg_get_client.query_widget_properties(widget_name, properties)

@register_tool("get_layout_data", "Gets bounding boxes for widgets.")
async def get_layout_data(resolution_width: int = 1920, resolution_height: int = 1080) -> Dict[str, Any]:
    """
    (Description loaded from prompts.json)
    """
    conn = get_unreal_connection()
    umg_get_client = UMGGet.UMGGet(conn)
    return await umg_get_client.get_layout_data(resolution_width, resolution_height)

@register_tool("check_widget_overlap", "Checks for widget overlap.")
async def check_widget_overlap(widget_names: Optional[List[str]] = None) -> Dict[str, Any]:
    """
    (Description loaded from prompts.json)
    """
    conn = get_unreal_connection()
    umg_get_client = UMGGet.UMGGet(conn)
    return await umg_get_client.check_widget_overlap(widget_names)

# =============================================================================
#  Category: Action
# =============================================================================

@register_tool("create_widget", "Creates a new widget.")
async def create_widget(parent_name: str, widget_type: str, new_widget_name: str) -> Dict[str, Any]:
    """
    (Description loaded from prompts.json)
    """
    conn = get_unreal_connection()
    umg_set_client = UMGSet.UMGSet(conn)
    result = await umg_set_client.create_widget(widget_type, new_widget_name, parent_name)
    
    # Invalidate Cache on Modification
    if result.get("status") == "success":
        # context_manager.invalidate_cache() # If we had one
        pass
        
    return result

@register_tool("set_widget_properties", "Sets properties on a widget.")
async def set_widget_properties(widget_name: str, properties: Dict[str, Any]) -> Dict[str, Any]:
    """
    (Description loaded from prompts.json)
    """
    conn = get_unreal_connection()
    umg_set_client = UMGSet.UMGSet(conn)
    return await umg_set_client.set_widget_properties(widget_name, properties)

@register_tool("delete_widget", "Deletes a widget.")
async def delete_widget(widget_name: str) -> Dict[str, Any]:
    """
    (Description loaded from prompts.json)
    """
    conn = get_unreal_connection()
    umg_set_client = UMGSet.UMGSet(conn)
    return await umg_set_client.delete_widget(widget_name)

@register_tool("reparent_widget", "Moves a widget to a new parent.")
async def reparent_widget(widget_name: str, new_parent_name: str) -> Dict[str, Any]:
    """
    (Description loaded from prompts.json)
    """
    conn = get_unreal_connection()
    umg_set_client = UMGSet.UMGSet(conn)
    return await umg_set_client.reparent_widget(widget_name, new_parent_name)

@register_tool("save_asset", "Saves the UMG asset.")
async def save_asset() -> Dict[str, Any]:
    """
    (Description loaded from prompts.json)
    """
    conn = get_unreal_connection()
    umg_set_client = UMGSet.UMGSet(conn)
    return await umg_set_client.save_asset()

# =============================================================================
#  Category: File Transformation (Explicit Path)
# =============================================================================

@register_tool("export_umg_to_json", "Decompiles UMG to JSON.")
async def export_umg_to_json(asset_path: str, widget_name: str = "Root") -> Dict[str, Any]:
    """
    (Description loaded from prompts.json)
    """
    asset_path = normalize_project_path(asset_path)
    conn = get_unreal_connection()
    umg_file_client = UMGFileTransformation.UMGFileTransformation(conn)
    return await umg_file_client.export_umg_to_json(asset_path, widget_name)

@register_tool("apply_layout", "Applies a layout logic to a UMG asset.")
async def apply_layout(layout_content: str, widget_name: Optional[str] = None) -> Dict[str, Any]:
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
        resp = await conn.send_command("get_target_umg_asset")
        if resp.get("status") != "error" and resp.get("success") is not False:
            final_path = resp.get("asset_path")
            if final_path:
                context_manager.set_target(final_path)
    
    if not final_path:
        return {"status": "error", "error": "No target UMG asset set. Use 'set_target_umg_asset' first."}
    
    logger.info(f"Resolved target asset path: {final_path}")
    
    target_widget = widget_name if widget_name is not None else (context_manager.get_target_widget() or "Root")

    context_manager.set_target(final_path, target_widget)

    umg_trans_client = UMGFileTransformation.UMGFileTransformation(conn)
    return await umg_trans_client.apply_json_to_umg(final_path, json_data, target_widget)

@mcp.tool()
async def apply_json_to_umg(asset_path: str, json_data: dict, widget_name: Optional[str] = None) -> Dict[str, Any]:
    """Applies a JSON definition to a UMG asset. (Maintained for backward compatibility and specialized agent workflows)"""
    asset_path = normalize_project_path(asset_path)
    target_widget = widget_name if widget_name is not None else (context_manager.get_target_widget() or "Root")
    conn = get_unreal_connection()
    umg_trans_client = UMGFileTransformation.UMGFileTransformation(conn)
    return await umg_trans_client.apply_json_to_umg(asset_path, json_data, target_widget)

@mcp.tool()
async def apply_html_to_umg(asset_path: str, html_content: str, widget_name: Optional[str] = None) -> Dict[str, Any]:
    """Applies an HTML definition to a UMG asset. (Maintained for backward compatibility)"""
    target_widget = widget_name
    parsed_path = asset_path
    if ":" in asset_path and widget_name is None:
        left, right = asset_path.split(":", 1)
        parsed_path = left
        if right.strip():
            target_widget = right.strip()

    await set_target_umg_asset(parsed_path, target_widget)
    return await apply_layout(html_content, target_widget)



# =============================================================================
#  Category: Editor & Level (New)
# =============================================================================

@register_tool("refresh_asset_registry", "Forces asset registry rescan.")
async def refresh_asset_registry(paths: Optional[List[str]] = None) -> Dict[str, Any]:
    """
    (Description loaded from prompts.json)
    """
    conn = get_unreal_connection()
    editor_client = UMGEditor.UMGEditor(conn)
    return await editor_client.refresh_asset_registry(paths)

@register_tool("get_actors_in_level", "Lists actors in level.")
async def get_actors_in_level() -> Dict[str, Any]:
    """
    (Description loaded from prompts.json)
    """
    conn = get_unreal_connection()
    editor_client = UMGEditor.UMGEditor(conn)
    return await editor_client.get_actors_in_level()

@register_tool("spawn_actor", "Spawns an actor.")
async def spawn_actor(actor_type: str, name: str, location: List[float] = None, rotation: List[float] = None) -> Dict[str, Any]:
    """
    (Description loaded from prompts.json)
    """
    conn = get_unreal_connection()
    editor_client = UMGEditor.UMGEditor(conn)
    return await editor_client.spawn_actor(actor_type, name, location, rotation)

@register_tool("list_assets", "Lists assets in the project content.")
async def list_assets(class_name: str = None, package_path: str = None, max_count: int = 50) -> Dict[str, Any]:
    """
    (Description loaded from prompts.json)
    """
    # Default path logic handled in Client/C++ to allow cleaner API here
    # If package_path is None, the system defaults to root /Game
    conn = get_unreal_connection()
    editor_client = UMGEditor.UMGEditor(conn)
    return await editor_client.list_assets(class_name=class_name, package_path=package_path, max_count=max_count)

# =============================================================================
#  Category: Blueprint (New)
# =============================================================================

# =============================================================================
#  Category: Blueprint (New)
# =============================================================================



@register_tool("compile_blueprint", "Compiles a Blueprint.")
async def compile_blueprint(blueprint_name: str = None) -> Dict[str, Any]:
    """
    (Description loaded from prompts.json)
    """
    conn = get_unreal_connection()
    return await conn.send_command("compile_blueprint", {"blueprint_name": blueprint_name})


@register_tool("add_step", "Adds an Executable Node to the current Program Counter.")
@register_tool("add_step", "Adds an Executable Node to the current Program Counter.")
async def add_step(name: str, args: List[Any] = None, comment: str = None, input_wires: Dict[str, Any] = None) -> Dict[str, Any]:
    """
    (Description loaded from prompts.json)
    """
    conn = get_unreal_connection()
    
    # Direct Protocol Construction
    payload = {
        "subAction": "add_function_step",
        "nodeName": name,  # C++ will now handle "PrintString" mapping internally
        "comment": comment
    }
    
    if args:
         payload["extraArgs"] = args
         
    if input_wires:
        payload["inputWires"] = input_wires

    # We reuse 'manage_blueprint_graph' for all graph ops
    return await conn.send_command("manage_blueprint_graph", payload)


@register_tool("prepare_value", "Places a Non-Executable Data Node (e.g. variable getter, literal).")
async def prepare_value(name: str, args: List[Any] = None) -> Dict[str, Any]:
    """
    (Description loaded from prompts.json)
    """
    conn = get_unreal_connection()
    
    payload = {
        "subAction": "create_node",
        # Pass 'name' as nodeName to trigger C++ Natural Inference (simulating add_step behavior)
        "nodeName": name, 
        "autoConnectToNodeId": "" # Explicitly disable auto-connect for pure nodes
    }
    if args:
        payload["extraArgs"] = args
        
    return await conn.send_command("manage_blueprint_graph", payload)


@register_tool("connect_data_to_pin", "Connects a Data Node to a target pin.")
async def connect_data_to_pin(source: str, target: str) -> Dict[str, Any]:
    """
    (Description loaded from prompts.json)
    """
    conn = get_unreal_connection()
    
    # Parse Source (NodeID:Pin)
    if ":" in source:
        source_id, source_pin = source.split(":", 1)
    else:
        source_id = source
        source_pin = "Return Value"

    # Parse Target (NodeID:Pin)
    if ":" in target:
        target_id, target_pin = target.split(":", 1)
    else:
        target_id = target
        target_pin = "InPin"

    payload = {
        "subAction": "connect_pins",
        "nodeIdA": source_id,
        "pinNameA": source_pin,
        "nodeIdB": target_id,
        "pinNameB": target_pin
    }
    return await conn.send_command("manage_blueprint_graph", payload)


@register_tool("get_function_nodes", "Lists all nodes in the active function.")
async def get_function_nodes() -> Dict[str, Any]:
    """
    (Description loaded from prompts.json)
    """
    conn = get_unreal_connection()
    return await conn.send_command("manage_blueprint_graph", {"subAction": "get_nodes"})


@register_tool("add_variable", "Adds a member variable.")
async def add_variable(name: str, type: str, subType: str = None) -> Dict[str, Any]:
    """
    (Description loaded from prompts.json)
    """
    conn = get_unreal_connection()
    
    payload = {
        "subAction": "add_variable",
        "name": name,
        "type": type
    }
    if subType:
        payload["subType"] = subType
    
    return await conn.send_command("manage_blueprint_graph", payload)


@register_tool("delete_variable", "Deletes a member variable.")
async def delete_variable(name: str) -> Dict[str, Any]:
    """
    (Description loaded from prompts.json)
    """
    conn = get_unreal_connection()
    return await conn.send_command("manage_blueprint_graph", {"subAction": "delete_variable", "name": name})


@register_tool("get_variables", "Lists member variables.")
async def get_variables() -> Dict[str, Any]:
    """
    (Description loaded from prompts.json)
    """
    conn = get_unreal_connection()
    return await conn.send_command("manage_blueprint_graph", {"subAction": "get_variables"})


@register_tool("delete_node", "Deletes a node.")
async def delete_node(node_id: str) -> Dict[str, Any]:
    """
    (Description loaded from prompts.json)
    """
    conn = get_unreal_connection()
    return await conn.send_command("manage_blueprint_graph", {"subAction": "delete_node", "nodeId": node_id})


# --- Stateful Blueprint Tools ---

@register_tool("set_edit_function", "Sets the active function/graph context for editing.")
async def set_edit_function(function_name: str) -> Dict[str, Any]:
    """
    (Description loaded from prompts.json)
    """
    conn = get_unreal_connection()
    # Direct Attention Command
    return await conn.send_command("set_edit_function", {"function_name": function_name})


@register_tool("set_cursor_node", "Explicitly sets the 'Program Counter'.")
async def set_cursor_node(node_id: str) -> Dict[str, Any]:
    """
    (Description loaded from prompts.json)
    """
    conn = get_unreal_connection()
    return await conn.send_command("set_cursor_node", {"node_id": node_id})


@register_tool("search_function_library", "Searches for callable functions.")
async def search_function_library(query: str = "", class_name: str = "") -> Dict[str, Any]:
    """
    (Description loaded from prompts.json)
    """
    conn = get_unreal_connection()
    
    payload = {}
    if query:
        payload["query"] = query
    if class_name:
        payload["className"] = class_name
        
    return await conn.send_command("manage_blueprint_graph", {"subAction": "search_function_library", **payload})

# =============================================================================
#  Category: Animation & Sequencer (Merged from UmgSequencerServer)
# =============================================================================

from Animation import UMGSequencer

# --- Attention & Context ---

@register_tool("set_animation_scope", "Sets the current animation scope.")
async def set_animation_scope(animation_name: str) -> Dict[str, Any]:
    """
    (Description loaded from prompts.json)
    """
    conn = get_unreal_connection()
    sequencer_client = UMGSequencer.UMGSequencer(conn)
    # The client method returns the coroutine from conn.send_command
    return await sequencer_client.set_animation_scope(animation_name)

@register_tool("set_widget_scope", "Sets the current widget scope.")
async def set_widget_scope(widget_name: str) -> Dict[str, Any]:
    """
    (Description loaded from prompts.json)
    """
    conn = get_unreal_connection()
    sequencer_client = UMGSequencer.UMGSequencer(conn)
    return await sequencer_client.set_widget_scope(widget_name)

# --- Read (Sensing) ---

@register_tool("get_all_animations", "Lists all animations.")
async def get_all_animations() -> Dict[str, Any]:
    """
    (Description loaded from prompts.json)
    """
    conn = get_unreal_connection()
    sequencer_client = UMGSequencer.UMGSequencer(conn)
    return await sequencer_client.get_all_animations()

@register_tool("get_animation_keyframes", "Gets keyframes for an animation.")
async def get_animation_keyframes(animation_name: str) -> Dict[str, Any]:
    """
    (Description loaded from prompts.json)
    """
    conn = get_unreal_connection()
    sequencer_client = UMGSequencer.UMGSequencer(conn)
    return await sequencer_client.get_animation_keyframes(animation_name)

@register_tool("get_animated_widgets", "Gets widgets affected by animation.")
async def get_animated_widgets(animation_name: str) -> Dict[str, Any]:
    """
    (Description loaded from prompts.json)
    """
    conn = get_unreal_connection()
    sequencer_client = UMGSequencer.UMGSequencer(conn)
    return await sequencer_client.get_animated_widgets(animation_name)

@register_tool("get_animation_full_data", "Gets complete animation data.")
async def get_animation_full_data(animation_name: str) -> Dict[str, Any]:
    """
    (Description loaded from prompts.json)
    """
    conn = get_unreal_connection()
    sequencer_client = UMGSequencer.UMGSequencer(conn)
    return await sequencer_client.get_animation_full_data(animation_name)

@register_tool("get_widget_animation_data", "Gets data for a specific widget in animation.")
async def get_widget_animation_data(animation_name: str, widget_name: str) -> Dict[str, Any]:
    """
    (Description loaded from prompts.json)
    """
    conn = get_unreal_connection()
    sequencer_client = UMGSequencer.UMGSequencer(conn)
    return await sequencer_client.get_widget_animation_data(animation_name, widget_name)

# --- Write (Action) ---

@register_tool("create_animation", "Creates a new animation.")
async def create_animation(animation_name: str) -> Dict[str, Any]:
    """
    (Description loaded from prompts.json)
    """
    conn = get_unreal_connection()
    sequencer_client = UMGSequencer.UMGSequencer(conn)
    return await sequencer_client.create_animation(asset_path=None, animation_name=animation_name)

@register_tool("delete_animation", "Deletes an animation.")
async def delete_animation(animation_name: str) -> Dict[str, Any]:
    """
    (Description loaded from prompts.json)
    """
    conn = get_unreal_connection()
    sequencer_client = UMGSequencer.UMGSequencer(conn)
    return await sequencer_client.delete_animation(asset_path=None, animation_name=animation_name)

@register_tool("set_property_keys", "Sets keyframes for a property.")
async def set_property_keys(property_name: str, keys: List[Dict[str, Any]]) -> Dict[str, Any]:
    """
    (Description loaded from prompts.json)
    """
    conn = get_unreal_connection()
    sequencer_client = UMGSequencer.UMGSequencer(conn)
    return await sequencer_client.set_property_keys(property_name, keys)

@register_tool("remove_property_track", "Removes a property track.")
async def remove_property_track(property_name: str) -> Dict[str, Any]:
    """
    (Description loaded from prompts.json)
    """
    conn = get_unreal_connection()
    sequencer_client = UMGSequencer.UMGSequencer(conn)
    return await sequencer_client.remove_property_track(property_name)

@register_tool("remove_keys", "Removes specific keys.")
async def remove_keys(property_name: str, times: List[float]) -> Dict[str, Any]:
    """
    (Description loaded from prompts.json)
    """
    conn = get_unreal_connection()
    sequencer_client = UMGSequencer.UMGSequencer(conn)
    return await sequencer_client.remove_keys(property_name, times)


# =============================================================================
#  Category: Material (New - 5 Pillars API)
# =============================================================================

@register_tool("material_set_target", "Specify target asset (path or parent). Required for stateful editing.")
async def material_set_target(path: str, create_if_not_found: bool = True) -> Dict[str, Any]:
    """
    (Description loaded from prompts.json)
    """
    conn = get_unreal_connection()
    material_client = UMGMaterial.UMGMaterial(conn)
    return await material_client.set_target_material(path, create_if_not_found)

@register_tool("material_define_variable", "Define external interface parameters (Scalar, Vector, Texture).")
async def material_define_variable(name: str, type: str) -> Dict[str, Any]:
    """
    (Description loaded from prompts.json)
    """
    conn = get_unreal_connection()
    material_client = UMGMaterial.UMGMaterial(conn)
    return await material_client.define_variable(name, type)

@register_tool("material_add_node", "Place a symbol from lib into graph and assign a unique instance handle.")
async def material_add_node(symbol: str, handle: str = None) -> Dict[str, Any]:
    """
    (Description loaded from prompts.json)
    """
    conn = get_unreal_connection()
    material_client = UMGMaterial.UMGMaterial(conn)
    return await material_client.add_node(symbol, handle)

@register_tool("material_delete", "Delete node instances or clean up logic by unique handle.")
async def material_delete(handle: str) -> Dict[str, Any]:
    """
    (Description loaded from prompts.json)
    """
    conn = get_unreal_connection()
    material_client = UMGMaterial.UMGMaterial(conn)
    return await material_client.delete_node(handle)

@register_tool("material_connect_nodes", "Establish node-to-node functional flow (Natural Connection).")
async def material_connect_nodes(from_handle: str, to_handle: str) -> Dict[str, Any]:
    """
    (Description loaded from prompts.json)
    """
    conn = get_unreal_connection()
    material_client = UMGMaterial.UMGMaterial(conn)
    return await material_client.connect_nodes(from_handle, to_handle)

@register_tool("material_connect_pins", "Manually connect specific pins for complex topologies.")
async def material_connect_pins(source: str, source_pin: str, target: str, target_pin: str) -> Dict[str, Any]:
    """
    (Description loaded from prompts.json)
    """
    conn = get_unreal_connection()
    material_client = UMGMaterial.UMGMaterial(conn)
    return await material_client.connect_pins(source, source_pin, target, target_pin)

@register_tool("material_set_hlsl_node_io", "Inject HLSL into Custom nodes and sync IO pins.")
async def material_set_hlsl_node_io(handle: str, code: str, inputs: List[str]) -> Dict[str, Any]:
    """
    (Description loaded from prompts.json)
    """
    conn = get_unreal_connection()
    material_client = UMGMaterial.UMGMaterial(conn)
    return await material_client.set_hlsl_node_io(handle, code, inputs)

@register_tool("material_set_node_properties", "Set internal properties for regular nodes.")
async def material_set_node_properties(handle: str, properties: Dict[str, Any]) -> Dict[str, Any]:
    """
    (Description loaded from prompts.json)
    """
    conn = get_unreal_connection()
    material_client = UMGMaterial.UMGMaterial(conn)
    return await material_client.set_node_properties(handle, properties)

@register_tool("material_compile_asset", "Submit changes and analyze HLSL compilation errors.")
async def material_compile_asset() -> Dict[str, Any]:
    """
    (Description loaded from prompts.json)
    """
    conn = get_unreal_connection()
    material_client = UMGMaterial.UMGMaterial(conn)
    return await material_client.compile_asset()

@register_tool("material_get_pins", "Introspects the available pins for a given node or 'Master'.")
async def material_get_pins(handle: str) -> Dict[str, Any]:
    """
    (Description loaded from prompts.json)
    """
    conn = get_unreal_connection()
    material_client = UMGMaterial.UMGMaterial(conn)
    return await material_client.get_node_pins(handle)

@register_tool("material_get_graph", "Retrieves the full graph topology.")
async def material_get_graph() -> Dict[str, Any]:
    """
    (Description loaded from prompts.json)
    """
    conn = get_unreal_connection()
    material_client = UMGMaterial.UMGMaterial(conn)
    return await material_client.get_graph()


@register_tool("hlsl_set_target", "Set HLSL editing target material; supports create/overwrite flow.")
async def hlsl_set_target(path: str, confirm_overwrite: bool = False, create_if_not_found: bool = True) -> Dict[str, Any]:
    """
    (Description loaded from prompts.json)
    """
    conn = get_unreal_connection()
    material_client = UMGMaterial.UMGMaterial(conn)
    return await material_client.hlsl_set_target(path, confirm_overwrite, create_if_not_found)


@register_tool("hlsl_get", "Read current HLSL code and parameter definitions.")
async def hlsl_get() -> Dict[str, Any]:
    """
    (Description loaded from prompts.json)
    """
    conn = get_unreal_connection()
    material_client = UMGMaterial.UMGMaterial(conn)
    return await material_client.hlsl_get()


@register_tool("hlsl_set", "Incrementally update HLSL code and/or parameter list with explicit delete semantics.")
async def hlsl_set(hlsl: Optional[str] = None, parameters: Optional[List[Dict[str, Any]]] = None) -> Dict[str, Any]:
    """
    (Description loaded from prompts.json)
    """
    conn = get_unreal_connection()
    material_client = UMGMaterial.UMGMaterial(conn)
    return await material_client.hlsl_set(hlsl, parameters)


@register_tool("hlsl_compile", "Compile current HLSL material target and return concise diagnostics.")
async def hlsl_compile() -> Dict[str, Any]:
    """
    (Description loaded from prompts.json)
    """
    conn = get_unreal_connection()
    material_client = UMGMaterial.UMGMaterial(conn)
    return await material_client.hlsl_compile()

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
        # Added "Animation Sequencer" and "Material Logic" to the list
        target_categories = ["UMG Editor", "Blueprint Logic", "Animation Sequencer", "Material Logic"]
        
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
                    safe_name = name.lower().replace(" ", "_")
                    mcp.prompt(name=safe_name, description=name)(make_prompt_func(prompt_text))
                    logger.info(f"Registered prompt: {safe_name}")

        # 2. Register System Info (Documentation)
        # We look for "Server Documentation" -> "UMG Info" or "Sequencer Info"
        # We can combine them or just expose the main one.
        # User requested merging, so let's try to expose Sequencer Info as well if possible, or append it.
        # For now, I'll stick to the original structure but check if Sequencer Info is there and maybe register it separately
        # or just assume the user updates prompts.json to include everything in "UMG Info" if they want a single prompt.
        # But wait, I can register a separate prompt for sequencer info.
        
        doc_category = data.get("Server Documentation", [])
        for item in doc_category:
            if item.get("name") == "UMG Info":
                info_text = item.get("prompt", "No documentation found.")
                
                @mcp.prompt(name="info")
                def info():
                    """Run method:"tools/list" to get more details."""
                    return info_text
                
                logger.info("Registered 'info' prompt from JSON.")
            
            # Register sequencer info as 'sequencer_info' if available
            elif item.get("name") == "Sequencer Info":
                seq_info_text = item.get("prompt", "")
                
                @mcp.prompt(name="sequencer_info")
                def sequencer_info():
                    """Detailed documentation for Sequencer tools."""
                    return seq_info_text
                
                logger.info("Registered 'sequencer_info' prompt from JSON.")

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
