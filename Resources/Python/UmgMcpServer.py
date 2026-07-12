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
import re
import uuid
from pathlib import Path

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

DEBUG_SOCKET = os.environ.get("UMG_MCP_DEBUG_SOCKET", "").lower() in ("1", "true", "yes")


def debug_socket(message: str) -> None:
    if not DEBUG_SOCKET:
        return
    sys.stderr.write(message)
    sys.stderr.flush()

class UnrealConnection:
    """Manages the async socket connection to the UmgMcp plugin running inside Unreal Engine."""
    def __init__(self):
        self.host = os.environ.get("UMG_MCP_HOST", UNREAL_HOST)
        self.port = int(os.environ.get("UMG_MCP_PORT", UNREAL_PORT))
        self.client_id = os.environ.get("UMG_MCP_CLIENT_ID", str(uuid.uuid4()))
        self.display_name = os.environ.get("UMG_MCP_CLIENT_NAME", f"AI-{self.client_id[:8]}")
        self.exclusive = os.environ.get("UMG_MCP_EXCLUSIVE", "true").lower() not in ("0", "false", "no")
        self._connected = False
        self._command_lock = asyncio.Lock()
        logger.info(f"Unreal Motion Graphics UI Designer Mode Context Process Launching... Connecting to UmgMcp plugin at {self.host}:{self.port} as {self.client_id}...")

    def disconnect(self) -> None:
        """Short-lived socket connections are closed per command."""
        return None

    async def connect_to(self, host: str, port: int, target: Optional[str] = None,
                         exclusive: bool = True, display_name: Optional[str] = None) -> Dict[str, Any]:
        """Atomically move this AI session to a selected UE editor instance."""
        async with self._command_lock:
            if self._connected:
                await self._send_command_unlocked("disconnect", {})
            self.host = host
            self.port = int(port)
            self.exclusive = exclusive
            if display_name:
                self.display_name = display_name
            payload = {
                "display_name": self.display_name,
                "exclusive": self.exclusive,
            }
            if target:
                payload["target"] = target
            response = await self._send_command_unlocked("connect", payload)
            self._connected = bool(response and response.get("status") != "error")
            return response
    
    async def send_command(self, command: str, params: Dict[str, Any] = None) -> Optional[Dict[str, Any]]:
        """Send a command to Unreal Engine and get the response (Async Short-Lived)."""
        async with self._command_lock:
            if command != "connect" and not self._connected:
                connected = await self._send_command_unlocked("connect", {
                    "display_name": self.display_name,
                    "exclusive": self.exclusive,
                })
                if not connected or connected.get("status") == "error":
                    return connected or {"status": "error", "error": "Unable to establish an UmgMcp session"}
                self._connected = True
            response = await self._send_command_unlocked(command, params)
            if command == "connect" and response and response.get("status") != "error":
                self._connected = True
            elif command == "disconnect":
                self._connected = False
            return response

    async def _send_command_unlocked(self, command: str, params: Dict[str, Any] = None) -> Optional[Dict[str, Any]]:
        """Wire-level request. Caller holds _command_lock so request order is deterministic."""
        reader = None
        writer = None

        if self.port <= 0:
            servers = await _discover_live_instances()
            if len(servers) == 1:
                self.host = str(servers[0]["host"])
                self.port = int(servers[0]["port"])
            else:
                return {
                    "status": "error",
                    "code": "endpoint_selection_required",
                    "error": "No unique UmgMcp endpoint is selected. Call list_umg_mcp_servers and connect_umg_mcp with the chosen port.",
                    "servers": servers,
                }
        
        try:
            logger.info(f"Connecting to Unreal at {self.host}:{self.port}...")
            # Async Open Connection (IOCP on Windows)
            reader, writer = await asyncio.wait_for(
                asyncio.open_connection(self.host, self.port), timeout=SOCKET_TIMEOUT)
            
            # Match Unity's command format exactly
            command_obj = {
                "command": command,
                "params": params or {},
                "client_id": self.client_id,
                "request_id": str(uuid.uuid4())
            }
            
            # Send with null delimiter
            command_json = json.dumps(command_obj)
            logger.info(f"[UMGMCP-Message] Sending: {command_json.strip()[:200]}... (truncated)")
            
            debug_socket(f"DEBUG: Async Sending {len(command_json)} bytes...\n")

            # Write data + null byte
            writer.write(command_json.encode('utf-8') + b'\0')
            await writer.drain()
            
            debug_socket("DEBUG: Async Drain completed.\n")

            # Force Flush / Shutdown Write to signal end of stream
            if writer.can_write_eof():
                writer.write_eof()
                debug_socket("DEBUG: Async write_eof() called.\n")
            
            # Read response
            debug_socket("DEBUG: Async Waiting for response (read 4096 chunks)...\n")
            
            chunks = []
            while True:
                # Read chunk
                chunk = await asyncio.wait_for(reader.read(4096), timeout=max(SOCKET_TIMEOUT, 30))
                if not chunk:
                    break # EOF
                
                if b'\x00' in chunk:
                    chunks.append(chunk[:chunk.find(b'\x00')])
                    break
                chunks.append(chunk)
            
            response_data = b"".join(chunks)
            
            debug_socket(f"DEBUG: Async Received {len(response_data)} bytes.\n")

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
                    debug_socket("DEBUG: Async Socket closed.\n")
                except:
                    pass
            if DEBUG_SOCKET:
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


def _discover_instance_records() -> List[Dict[str, Any]]:
    """Read discovery records published by local UE editors."""
    candidates: List[Path] = []
    configured = os.environ.get("UMG_MCP_DISCOVERY_DIR")
    if configured:
        candidates.append(Path(configured))
    local_app_data = os.environ.get("LOCALAPPDATA")
    if local_app_data:
        candidates.append(Path(local_app_data) / "UmgMcp" / "instances")
    here = Path(__file__).resolve()
    for parent in here.parents:
        if any(parent.glob("*.uproject")):
            candidates.append(parent / "Saved" / "UmgMcp" / "instances")
            break

    records: List[Dict[str, Any]] = []
    seen = set()
    for directory in candidates:
        if not directory.exists():
            continue
        for record_file in directory.glob("*.json"):
            try:
                record = json.loads(record_file.read_text(encoding="utf-8"))
                key = record.get("server_instance_id") or str(record_file)
                if key not in seen:
                    record["discovery_file"] = str(record_file)
                    records.append(record)
                    seen.add(key)
            except (OSError, ValueError) as exc:
                logger.warning("Ignoring invalid UmgMcp discovery record %s: %s", record_file, exc)
    return records


async def _probe_instance(record: Dict[str, Any]) -> Optional[Dict[str, Any]]:
    """Validate a discovery record against the server that currently owns it."""
    host = str(record.get("host") or "127.0.0.1")
    try:
        port = int(record.get("port") or 0)
    except (TypeError, ValueError):
        return None
    if port <= 0:
        return None
    writer = None
    try:
        reader, writer = await asyncio.wait_for(asyncio.open_connection(host, port), timeout=SOCKET_TIMEOUT)
        request = {
            "command": "server_info",
            "params": {},
            "client_id": "discovery-probe",
            "request_id": str(uuid.uuid4()),
        }
        writer.write(json.dumps(request).encode("utf-8") + b"\0")
        await writer.drain()
        raw = await asyncio.wait_for(reader.readuntil(b"\0"), timeout=SOCKET_TIMEOUT)
        state = json.loads(raw[:-1].decode("utf-8"))
        expected = record.get("server_instance_id")
        if state.get("status") == "error" or (expected and state.get("server_instance_id") != expected):
            return None
        live = dict(record)
        live.update({"host": host, "port": port, "available": True})
        return live
    except (OSError, asyncio.TimeoutError, asyncio.IncompleteReadError, ValueError):
        return None
    finally:
        if writer is not None:
            writer.close()
            try:
                await writer.wait_closed()
            except OSError:
                pass


async def _discover_live_instances() -> List[Dict[str, Any]]:
    probes = await asyncio.gather(*(_probe_instance(record) for record in _discover_instance_records()))
    return [record for record in probes if record is not None]


@mcp.tool(name="list_umg_mcp_servers", description="Lists local UE editor UmgMcp instances available for an explicit connection.")
async def list_umg_mcp_servers() -> Dict[str, Any]:
    return {"status": "success", "servers": await _discover_live_instances()}


@mcp.tool(name="connect_umg_mcp", description="Connects this AI to one UE UmgMcp endpoint and optionally binds an exclusive target.")
async def connect_umg_mcp(host: str = "127.0.0.1", port: int = UNREAL_PORT,
                          target: Optional[str] = None, exclusive: bool = True,
                          display_name: Optional[str] = None) -> Dict[str, Any]:
    if port <= 0:
        servers = await _discover_live_instances()
        if len(servers) != 1:
            return {
                "status": "error",
                "code": "endpoint_selection_required",
                "error": "Discovery did not resolve exactly one UE editor; select a server and pass its port.",
                "servers": servers,
            }
        host = str(servers[0]["host"])
        port = int(servers[0]["port"])
    conn = get_unreal_connection()
    response = await conn.connect_to(host, port, target, exclusive, display_name)
    if response.get("status") != "error" and target:
        target_response = await conn.send_command("set_target_umg_asset", {"asset_path": target})
        if target_response.get("status") == "error":
            return target_response
        context_manager.set_target(target)
        response["target"] = target
    return response


@mcp.tool(name="get_umg_mcp_connection", description="Shows this AI's active UE endpoint, session id, and server-side connection state.")
async def get_umg_mcp_connection() -> Dict[str, Any]:
    conn = get_unreal_connection()
    state = await conn.send_command("server_info", {})
    state.update({"host": conn.host, "port": conn.port, "client_id": conn.client_id})
    return state

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
    # This encourages the AI to experiment based on its knowledge.
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


@register_tool("get_target_blueprint_asset", "Gets the active Widget or general Blueprint asset used by BlueCode.")
async def get_target_blueprint_asset() -> Dict[str, Any]:
    conn = get_unreal_connection()
    response = await conn.send_command("get_target_blueprint_asset", {})
    if response and response.get("status") != "error" and response.get("success") is not False and response.get("asset_path"):
        context_manager.set_target(response.get("asset_path"))
    return response

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
    Normalizes user-friendly project paths without changing explicit package
    mount roots. Relative paths still default to /Game, but absolute Unreal
    paths such as /FogOfWar/... must remain in their requested namespace.
    """
    import posixpath
    path = path.replace('\\', '/')
    if any(segment == '..' for segment in path.split('/')):
        raise ValueError("Unreal asset paths must not contain '..' segments.")

    if path.startswith('Content/'):
        path = '/Game/' + path[8:]
        
    if not path.startswith('/'):
        path = '/Game/' + path

    resolved = posixpath.normpath(path)
    if not resolved.startswith('/'):
        resolved = '/' + resolved
        
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


@register_tool("set_target_blueprint_asset", "Sets an existing Widget, Actor, Object, or other Blueprint as the active BlueCode target.")
async def set_target_blueprint_asset(asset_path: str) -> Dict[str, Any]:
    normalized_path = normalize_project_path(asset_path)
    conn = get_unreal_connection()
    response = await conn.send_command("set_target_blueprint_asset", {"asset_path": normalized_path})
    if response and response.get("status") != "error" and response.get("success") is not False:
        context_manager.set_target(response.get("asset_path") or normalized_path)
    return response

# =============================================================================
#  Category: Sensing
# =============================================================================

@register_tool("get_widget_tree", "Fetches a compact widget tree from the focused widget target, or root if no widget is focused.")
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
async def create_widget(widget_type: str, new_widget_name: str, parent_name: str = "") -> Dict[str, Any]:
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

@register_tool("delete_widget", "Deletes a widget (requires confirm_delete=true).")
async def delete_widget(widget_name: str, confirm_delete: bool = False) -> Dict[str, Any]:
    """
    (Description loaded from prompts.json)
    """
    conn = get_unreal_connection()
    umg_set_client = UMGSet.UMGSet(conn)
    return await umg_set_client.delete_widget(widget_name, confirm_delete)

@register_tool("reorder_widget_tree", "Union-style sibling reorder from a partial widget tree.")
async def reorder_widget_tree(tree: Any, root: str = "") -> Dict[str, Any]:
    """
    (Description loaded from prompts.json)
    """
    conn = get_unreal_connection()
    umg_set_client = UMGSet.UMGSet(conn)
    return await umg_set_client.reorder_widget_tree(tree, root)

@register_tool("reparent_widget", "Compatibility structural replacement/wrap command.")
async def reparent_widget(widget_name: str, new_parent_widget: Any) -> Dict[str, Any]:
    """
    (Description loaded from prompts.json)
    """
    conn = get_unreal_connection()
    umg_set_client = UMGSet.UMGSet(conn)
    return await umg_set_client.reparent_widget(widget_name, new_parent_widget)

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

@register_tool("apply_json_to_umg", "Compatibility bulk JSON apply command. Prefer apply_layout.")
async def apply_json_to_umg(asset_path: str, json_data: dict, widget_name: Optional[str] = None) -> Dict[str, Any]:
    """Applies a JSON definition to a UMG asset. (Maintained for backward compatibility and specialized agent workflows)"""
    asset_path = normalize_project_path(asset_path)
    target_widget = widget_name if widget_name is not None else (context_manager.get_target_widget() or "Root")
    conn = get_unreal_connection()
    umg_trans_client = UMGFileTransformation.UMGFileTransformation(conn)
    return await umg_trans_client.apply_json_to_umg(asset_path, json_data, target_widget)

@register_tool("apply_html_to_umg", "Compatibility bulk HTML apply command. Prefer apply_layout.")
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



@register_tool("compile_blueprint", "Compiles and saves a Blueprint.")
async def compile_blueprint(blueprint_name: str = None) -> Dict[str, Any]:
    """
    (Description loaded from prompts.json)
    """
    conn = get_unreal_connection()
    return await conn.send_command("compile_blueprint", {"blueprint_name": blueprint_name})


def _bluecode_strip_fences(code: str) -> str:
    lines: List[str] = []
    in_fence = False
    for raw_line in (code or "").replace("\r\n", "\n").splitlines():
        line = raw_line.strip()
        if line.startswith("```"):
            in_fence = not in_fence
            continue
        lines.append(raw_line)
    return "\n".join(lines)


def _bluecode_statements(code: str) -> List[str]:
    statements: List[str] = []
    for raw_line in _bluecode_strip_fences(code).splitlines():
        line = raw_line.strip()
        if not line or line in ("{", "}"):
            continue
        if line.startswith("#") or line.startswith("//"):
            continue
        lowered = line.lower()
        if lowered in ("main()", "main:", "end", "return"):
            continue
        if line.endswith(";"):
            line = line[:-1].strip()
        statements.append(line)
    return statements


_BLUECODE_MATH_READBACK = {
    "Add_IntInt": "+",
    "Add_FloatFloat": "+",
    "Subtract_IntInt": "-",
    "Subtract_FloatFloat": "-",
    "Multiply_IntInt": "*",
    "Multiply_FloatFloat": "*",
    "Divide_IntInt": "/",
    "Divide_FloatFloat": "/",
}


_BLUECODE_WIDGET_EVENTS = {
    "Button": ["OnClicked", "OnPressed", "OnReleased", "OnHovered", "OnUnhovered"],
    "CheckBox": ["OnCheckStateChanged"],
    "ComboBoxString": ["OnSelectionChanged", "OnOpening"],
    "EditableTextBox": ["OnTextChanged", "OnTextCommitted"],
    "MultiLineEditableTextBox": ["OnTextChanged", "OnTextCommitted"],
    "Slider": ["OnValueChanged", "OnMouseCaptureBegin", "OnMouseCaptureEnd", "OnControllerCaptureBegin", "OnControllerCaptureEnd"],
    "SpinBox": ["OnValueChanged", "OnValueCommitted", "OnBeginSliderMovement", "OnEndSliderMovement"],
    "ExpandableArea": ["OnExpansionChanged"],
}


def _bluecode_normalize_widget_class(class_name: str) -> str:
    normalized = (class_name or "").strip()
    if normalized.startswith("U") and len(normalized) > 1 and normalized[1].isupper():
        normalized = normalized[1:]
    if normalized.endswith("_C"):
        normalized = normalized[:-2]
    return normalized


def _bluecode_parse_widget_tree(tree_text: str) -> List[Dict[str, str]]:
    widgets: List[Dict[str, str]] = []
    for raw_line in (tree_text or "").splitlines():
        match = re.match(r"^\s*(?:-\s*)?([A-Za-z_][A-Za-z0-9_]*)\s+\[([^\]]+)\]", raw_line.strip())
        if not match:
            continue
        widget_name = match.group(1)
        widget_class = _bluecode_normalize_widget_class(match.group(2))
        if widget_class == "WidgetBlueprint":
            continue
        widgets.append({"widget": widget_name, "class": widget_class})
    return widgets


def _bluecode_available_events(widgets: List[Dict[str, str]]) -> List[Dict[str, str]]:
    events: List[Dict[str, str]] = []
    for widget in widgets:
        widget_name = widget.get("widget", "")
        widget_class = widget.get("class", "")
        for event_name in _BLUECODE_WIDGET_EVENTS.get(widget_class, []):
            target = f"{widget_name}.{event_name}"
            events.append({
                "widget": widget_name,
                "class": widget_class,
                "event": event_name,
                "target": target,
                "set_function": f'bluecode_set_function("{target}")',
            })
    return events


def _bluecode_bound_events(nodes: List[Dict[str, Any]]) -> List[Dict[str, Any]]:
    events: List[Dict[str, Any]] = []
    seen = set()
    for node in nodes or []:
        kind = node.get("kind")
        if kind not in ("component_event", "custom_event", "event"):
            continue

        label = str(node.get("member") or node.get("title") or node.get("name") or "")
        event_name = label.replace(" ", "")
        widget_name = ""
        target = event_name

        label_match = re.match(r"^\s*(.*?)\s*\((.*?)\)\s*$", label)
        if kind == "component_event" and label_match:
            event_name = label_match.group(1).replace(" ", "")
            widget_name = label_match.group(2).strip()
            target = f"{widget_name}.{event_name}" if widget_name else event_name

        key = (kind, target, node.get("id"))
        if key in seen:
            continue
        seen.add(key)
        events.append({
            "kind": kind,
            "widget": widget_name,
            "event": event_name,
            "target": target,
            "node_id": node.get("id"),
            "title": node.get("title") or label,
        })
    return events


def _bluecode_ok(result: Any) -> bool:
    return isinstance(result, dict) and (result.get("success") is True or result.get("status") == "success")


def _bluecode_format_arg(pin: Dict[str, Any]) -> Optional[str]:
    if pin.get("linked_to"):
        linked = pin["linked_to"][0]
        label = linked.get("member") or linked.get("title") or linked.get("node") or linked.get("node_id")
        return str(label) if label else None
    value = pin.get("default")
    if value is None or value == "":
        return None
    category = str(pin.get("category", "")).lower()
    if category in ("string", "text", "name") and not (str(value).startswith('"') and str(value).endswith('"')):
        return json.dumps(str(value), ensure_ascii=False)
    return str(value)


def _bluecode_expr_from_pin(pin: Dict[str, Any], by_id: Dict[str, Dict[str, Any]], seen: Optional[set] = None) -> Optional[str]:
    linked_to = pin.get("linked_to") or []
    if linked_to:
        linked = linked_to[0]
        linked_id = linked.get("node_id")
        if linked_id and linked_id in by_id:
            return _bluecode_expr_from_node(by_id[linked_id], by_id, seen)
    return _bluecode_format_arg(pin)


def _bluecode_expr_from_node(node: Dict[str, Any], by_id: Dict[str, Dict[str, Any]], seen: Optional[set] = None) -> str:
    seen = seen or set()
    node_id = node.get("id")
    if node_id in seen:
        return str(node.get("member") or node.get("title") or node.get("name") or node_id)
    if node_id:
        seen.add(node_id)

    kind = node.get("kind")
    member = str(node.get("member") or node.get("title") or node.get("name") or "")
    if kind == "variable_get":
        return member

    args: List[str] = []
    for pin in node.get("inputs", []):
        pin_name = str(pin.get("name", "")).lower()
        if pin.get("is_exec") or pin_name in ("execute", "then", "self", "worldcontextobject"):
            continue
        arg = _bluecode_expr_from_pin(pin, by_id, seen)
        if arg is not None:
            args.append(arg)

    if kind == "call" and member in _BLUECODE_MATH_READBACK and len(args) >= 2:
        return f"{args[0]} {_BLUECODE_MATH_READBACK[member]} {args[1]}"
    if kind == "call" and member:
        return f"{member}({', '.join(args)})"
    return _bluecode_format_arg({"linked_to": [{"member": member}]}) or member


def _bluecode_node_line(node: Dict[str, Any], by_id: Optional[Dict[str, Dict[str, Any]]] = None) -> Optional[str]:
    by_id = by_id or {}
    kind = node.get("kind") or node.get("class") or "node"
    if kind in ("event", "component_event", "custom_event", "function_entry"):
        return None
    if kind == "branch":
        condition = None
        for pin in node.get("inputs", []):
            if str(pin.get("name", "")).lower() == "condition":
                condition = _bluecode_expr_from_pin(pin, by_id)
                break
        return f"if {condition or 'Condition'}:"
    member = node.get("member") or node.get("title") or node.get("name")
    if not member:
        return None
    if kind in ("call", "variable_get", "variable_set", "node"):
        args: List[str] = []
        for pin in node.get("inputs", []):
            if pin.get("is_exec") or str(pin.get("name", "")).lower() in ("execute", "then", "self", "worldcontextobject"):
                continue
            arg = _bluecode_expr_from_pin(pin, by_id)
            if arg is not None:
                args.append(arg)
        if kind == "variable_get":
            return str(member)
        if kind == "variable_set":
            value = args[0] if args else ""
            return f"{member} = {value}".rstrip()
        return f"{member}({', '.join(args)})"
    return str(node.get("title") or node.get("name"))


def _bluecode_is_raw_node_dump_line(statement: str) -> bool:
    text = str(statement or "").strip()
    if text.endswith("()"):
        text = text[:-2].strip()
    return bool(re.match(r"^(K2Node|EdGraphNode|MaterialGraphNode|AnimGraphNode)_", text))


def _bluecode_pin_summary(pin: Dict[str, Any]) -> Dict[str, Any]:
    summary: Dict[str, Any] = {
        "name": pin.get("name"),
        "direction": pin.get("direction"),
        "category": pin.get("category"),
        "is_exec": bool(pin.get("is_exec")),
    }
    if pin.get("subType"):
        summary["subType"] = pin.get("subType")
    if pin.get("default") not in (None, ""):
        summary["default"] = pin.get("default")
    linked_to = pin.get("linked_to") or []
    if linked_to:
        summary["linked_to"] = [
            {
                "node_id": link.get("node_id"),
                "pin": link.get("pin"),
                "member": link.get("member"),
                "title": link.get("title"),
            }
            for link in linked_to
        ]
    return summary


def _bluecode_add_unique(items: List[Dict[str, Any]], item: Dict[str, Any]) -> None:
    key = json.dumps(item, sort_keys=True, ensure_ascii=False)
    if not any(json.dumps(existing, sort_keys=True, ensure_ascii=False) == key for existing in items):
        items.append(item)


def _bluecode_expansion_entry(
    line_number: int,
    statement: str,
    node: Dict[str, Any],
    by_id: Dict[str, Dict[str, Any]],
) -> Dict[str, Any]:
    node_id = node.get("id")
    pins = [_bluecode_pin_summary(pin) for pin in ((node.get("inputs") or []) + (node.get("outputs") or []))]
    exec_edges = [
        {
            "from_node_id": node_id,
            "to_node_id": next_id,
            "inference": "exec edge recovered from output exec pin",
        }
        for next_id in node.get("exec_next", []) or []
    ]
    data_edges: List[Dict[str, Any]] = []
    default_pins: List[Dict[str, Any]] = []

    for pin in node.get("inputs", []) or []:
        pin_name = pin.get("name")
        pin_lower = str(pin_name or "").lower()
        if pin.get("is_exec") or pin_lower in ("execute", "then", "self", "worldcontextobject"):
            continue
        if pin.get("default") not in (None, ""):
            default_pins.append({
                "pin": pin_name,
                "value": pin.get("default"),
                "category": pin.get("category"),
                "inference": "literal/default pin retained as compact value",
            })
        for linked in pin.get("linked_to", []) or []:
            _bluecode_add_unique(data_edges, {
                "from_node_id": linked.get("node_id"),
                "from_pin": linked.get("pin"),
                "to_node_id": node_id,
                "to_pin": pin_name,
                "inference": "data edge recovered from linked input pin",
            })

    compacted_nodes: List[Dict[str, Any]] = []
    for dep in node.get("data_dependencies", []) or []:
        dep_node = by_id.get(dep.get("node_id"))
        _bluecode_add_unique(data_edges, {
            "from_node_id": dep.get("node_id"),
            "to_node_id": node_id,
            "to_pin": dep.get("pin"),
            "inference": "data dependency compressed into statement expression",
        })
        if dep_node:
            compacted_nodes.append({
                "node_id": dep_node.get("id"),
                "kind": dep_node.get("kind"),
                "member": dep_node.get("member"),
                "expression": _bluecode_expr_from_node(dep_node, by_id),
                "to_pin": dep.get("pin"),
            })

    inference_sources = ["node kind/member"]
    if exec_edges:
        inference_sources.append("exec_next")
    if data_edges:
        inference_sources.append("linked pins/data_dependencies")
    if default_pins:
        inference_sources.append("pin defaults")
    if compacted_nodes:
        inference_sources.append("compacted dependency nodes")
    if any(str(pin.get("name", "")).lower() in ("self", "worldcontextobject") for pin in node.get("inputs", []) or []):
        inference_sources.append("implicit self/world context")

    source_node_ids = {node_id} if node_id else set()
    source_node_ids.update(dep.get("node_id") for dep in node.get("data_dependencies", []) or [] if dep.get("node_id"))

    return {
        "line": line_number,
        "statement": statement,
        "node_id": node_id,
        "kind": node.get("kind") or node.get("class") or "node",
        "member": node.get("member"),
        "source_node_ids": sorted(source_node_ids),
        "pins": pins,
        "exec_edges": exec_edges,
        "data_edges": data_edges,
        "default_pins": default_pins,
        "compacted_nodes": compacted_nodes,
        "inference_sources": inference_sources,
    }


def _bluecode_order_nodes(nodes: List[Dict[str, Any]]) -> List[Dict[str, Any]]:
    by_id = {node.get("id"): node for node in nodes if node.get("id")}
    starts = [
        node for node in nodes
        if node.get("kind") in ("event", "component_event", "custom_event", "function_entry")
    ]
    ordered: List[Dict[str, Any]] = []
    seen = set()

    def visit(node: Dict[str, Any]) -> None:
        node_id = node.get("id")
        if node_id in seen:
            return
        if node_id:
            seen.add(node_id)
        ordered.append(node)
        for next_id in node.get("exec_next", []):
            next_node = by_id.get(next_id)
            if next_node:
                visit(next_node)

    for start in starts:
        visit(start)
    for node in nodes:
        if node.get("id") not in seen:
            visit(node)
    return ordered


def _bluecode_connections_from_nodes(nodes: List[Dict[str, Any]]) -> List[Dict[str, Any]]:
    connections: List[Dict[str, Any]] = []
    seen = set()
    for node in nodes or []:
        source_node_id = node.get("id") or ""
        source_title = node.get("title") or node.get("member") or node.get("name") or ""
        for pin in node.get("outputs", []) or []:
            source_pin = pin.get("name") or ""
            source_endpoint = f"{source_node_id}:{source_pin}"
            kind = "exec" if pin.get("is_exec") else "data"
            for linked in pin.get("linked_to", []) or []:
                target_node_id = linked.get("node_id") or ""
                target_pin = linked.get("pin") or ""
                target_endpoint = f"{target_node_id}:{target_pin}"
                key = f"{source_endpoint}->{target_endpoint}"
                if key in seen:
                    continue
                seen.add(key)
                connections.append({
                    "source": source_endpoint,
                    "target": target_endpoint,
                    "source_node_id": source_node_id,
                    "source_title": source_title,
                    "source_pin": source_pin,
                    "target_node_id": target_node_id,
                    "target_title": linked.get("title") or linked.get("member") or linked.get("node") or "",
                    "target_pin": target_pin,
                    "kind": kind,
                    "connect": f"{source_endpoint} -> {target_endpoint}",
                    "delete_target": {
                        "kind": "connection",
                        "source": source_endpoint,
                        "target": target_endpoint,
                    },
                })
    return connections


async def _bluecode_read_payload(conn: UnrealConnection, detail: str = "", include_connections: bool = False) -> Dict[str, Any]:
    backend_payload: Dict[str, Any] = {"subAction": "bluecode_read_function"}
    if detail:
        backend_payload["detail"] = detail
    if include_connections:
        backend_payload["include_connections"] = True
    backend_result = await conn.send_command("manage_blueprint_graph", backend_payload)
    if _bluecode_ok(backend_result) and isinstance(backend_result, dict) and backend_result.get("code"):
        return backend_result

    graph = await conn.send_command("get_target_graph", {})
    graph_name = (graph or {}).get("target_graph") or "EventGraph"
    raw_nodes = await conn.send_command("manage_blueprint_graph", {"subAction": "get_nodes"})
    nodes = (raw_nodes or {}).get("nodes", [])
    by_id = {node.get("id"): node for node in nodes if node.get("id")}
    dependency_node_ids = {
        dep.get("node_id")
        for node in nodes
        for dep in (node.get("data_dependencies", []) or [])
        if dep.get("node_id")
    }
    ordered = _bluecode_order_nodes(nodes)
    body_lines = []
    body_entries = []
    dependencies = []
    seen_deps = set()
    for node in ordered:
        if node.get("id") in dependency_node_ids and not node.get("isExec"):
            continue
        line = _bluecode_node_line(node, by_id)
        if line:
            body_lines.append(f"  {line}")
            body_entries.append({"statement": line, "node": node})
        for dep in node.get("data_dependencies", []):
            dep_key = json.dumps(dep, sort_keys=True)
            if dep_key not in seen_deps:
                seen_deps.add(dep_key)
                dependencies.append(dep)
    code = "\n".join(["main()"] + body_lines + ["  end"])
    degraded_lines = [
        entry["statement"]
        for entry in body_entries
        if _bluecode_is_raw_node_dump_line(entry["statement"])
    ]
    semantic_read_degraded = bool(degraded_lines)
    result: Dict[str, Any] = {
        "success": _bluecode_ok(raw_nodes) and not semantic_read_degraded,
        "function": graph_name,
        "entry": "main",
        "exit": "end",
        "code": code,
        "representation": "compressed_blueprint_node_graph",
        "exec_paths": max(1, sum(1 for node in nodes if node.get("kind") == "branch") + 1),
        "data_dependencies": dependencies,
        "debug_nodes_available": True,
        "fallback_used": True,
    }
    connections = _bluecode_connections_from_nodes(nodes)
    result["connection_count"] = len(connections)
    result["connections_available"] = True
    if include_connections or detail in ("debug", "roundtrip"):
        result["connections"] = connections
        result["connections_usage"] = "Use bluecode_connect to add missing links and bluecode_delete(kind='connection', confirm_delete=true) to remove specific existing links."
    if semantic_read_degraded:
        result["semantic_read_degraded"] = True
        result["error"] = "Backend bluecode_read_function did not return semantic code; legacy get_nodes fallback only produced raw node names."
        result["raw_node_lines"] = degraded_lines
    if isinstance(backend_result, dict) and not _bluecode_ok(backend_result):
        result["backend_error"] = backend_result
    elif isinstance(backend_result, dict):
        result["backend_warning"] = backend_result
    if isinstance(raw_nodes, dict) and not _bluecode_ok(raw_nodes):
        result["raw_error"] = raw_nodes
    if detail == "debug":
        result["expansion"] = [
            _bluecode_expansion_entry(index, entry["statement"], entry["node"], by_id)
            for index, entry in enumerate(body_entries, start=2)
        ]
        result["nodes"] = nodes
        result["raw"] = raw_nodes
    return result


@register_tool("bluecode_set_function", "Sets the active Blueprint function/event for bluecode.")
async def bluecode_set_function(function_name: str) -> Dict[str, Any]:
    """
    (Description loaded from prompts.json)
    """
    conn = get_unreal_connection()
    return await conn.send_command("set_edit_function", {"function_name": function_name})


@register_tool("bluecode_read_function", "Reads the active Blueprint function as compact bluecode.")
async def bluecode_read_function(detail: str = "", include_connections: bool = False) -> Dict[str, Any]:
    """
    (Description loaded from prompts.json)
    """
    conn = get_unreal_connection()
    return await _bluecode_read_payload(conn, detail, include_connections)


@register_tool("bluecode_read_variables", "Reads Blueprint member variables as compact bluecode data.")
async def bluecode_read_variables() -> Dict[str, Any]:
    """
    (Description loaded from prompts.json)
    """
    conn = get_unreal_connection()
    result = await conn.send_command("manage_blueprint_graph", {"subAction": "get_variables"})
    variables = []
    for var in (result or {}).get("variables", []):
        variables.append({
            "name": var.get("name"),
            "type": var.get("type"),
            "subType": var.get("subType", ""),
        })
    return {"success": bool(result and result.get("success", True)), "variables": variables}


@register_tool("bluecode_read_events", "Reads Blueprint event candidates and currently scoped bound events.")
async def bluecode_read_events() -> Dict[str, Any]:
    """
    (Description loaded from prompts.json)
    """
    conn = get_unreal_connection()
    tree_result = await conn.send_command("get_widget_tree", {})
    payload: Dict[str, Any] = {"subAction": "get_events"}
    if (tree_result or {}).get("scope") == "target_widget" and (tree_result or {}).get("root_widget"):
        payload["scope_widget"] = tree_result.get("root_widget")

    events_result = await conn.send_command("manage_blueprint_graph", payload)
    if _bluecode_ok(events_result) and (
        "available_events" in events_result or "bound_events" in events_result
    ):
        return events_result

    graph = await conn.send_command("get_target_graph", {})
    graph_name = (graph or {}).get("target_graph") or "EventGraph"
    raw_nodes = await conn.send_command("manage_blueprint_graph", {"subAction": "get_nodes"})

    tree_text = (tree_result or {}).get("widget_tree", "")
    widgets = _bluecode_parse_widget_tree(tree_text)
    return {
        "success": bool((tree_result and tree_result.get("success", True)) or (raw_nodes and raw_nodes.get("success", True))),
        "bound_scope": graph_name,
        "widgets": widgets,
        "available_events": _bluecode_available_events(widgets),
        "bound_events": _bluecode_bound_events((raw_nodes or {}).get("nodes", [])),
        "notes": [
            "available_events are inferred from common UMG widget delegate names",
            "bound_events are read from the current bluecode graph scope, not a full side-effect-free EventGraph scan",
        ],
    }


@register_tool("bluecode_apply", "Applies bluecode statements as union-only Blueprint graph edits.")
async def bluecode_apply(code: str, anchor: str = "end", mode: str = "append", member_classes: Dict[str, str] = None, action_handles: Dict[str, str] = None, action_hints: Dict[str, Dict[str, Any]] = None, expression_hints: Dict[str, Dict[str, Any]] = None, action_hints_by_line: List[Dict[str, Any]] = None, node_aliases: Dict[str, str] = None, aliases: Dict[str, str] = None, node_properties: Dict[str, Dict[str, Any]] = None) -> Dict[str, Any]:
    """
    (Description loaded from prompts.json)
    """
    conn = get_unreal_connection()
    if mode not in ("append", "upsert"):
        return {"success": False, "error": "bluecode_apply supports mode='append' or mode='upsert' only"}

    backend_payload: Dict[str, Any] = {
        "subAction": "bluecode_apply",
        "code": code,
        "anchor": anchor,
        "mode": mode,
    }
    for key, value in (
        ("member_classes", member_classes),
        ("action_handles", action_handles),
        ("action_hints", action_hints),
        ("expression_hints", expression_hints),
        ("action_hints_by_line", action_hints_by_line),
        ("node_aliases", node_aliases),
        ("aliases", aliases),
        ("node_properties", node_properties),
    ):
        if value is not None:
            backend_payload[key] = value
    return await conn.send_command("manage_blueprint_graph", backend_payload)


@register_tool("bluecode_apply_variables", "Adds Blueprint variables as union-only bluecode data edits.")
async def bluecode_apply_variables(variables: List[Dict[str, Any]]) -> Dict[str, Any]:
    """
    (Description loaded from prompts.json)
    """
    conn = get_unreal_connection()
    operations = []
    for variable in variables or []:
        payload = {
            "subAction": "add_variable",
            "name": variable.get("name"),
            "type": variable.get("type"),
        }
        if variable.get("subType"):
            payload["subType"] = variable.get("subType")
        result = await conn.send_command("manage_blueprint_graph", payload)
        operations.append({"variable": variable, "result": result})
    return {
        "success": all(_bluecode_ok(op.get("result")) for op in operations),
        "applied_count": sum(1 for op in operations if _bluecode_ok(op.get("result"))),
        "operations": operations,
        "deleted_count": 0,
    }


@register_tool("bluecode_connect", "Adds explicit Blueprint pin connections as a BlueCode escape hatch.")
async def bluecode_connect(connects: List[Any] = None, source: str = "", target: str = "", aliases: Dict[str, str] = None) -> Dict[str, Any]:
    """
    (Description loaded from prompts.json)
    """
    conn = get_unreal_connection()
    payload: Dict[str, Any] = {"subAction": "bluecode_connect"}
    if connects is not None:
        payload["connects"] = connects
    elif source or target:
        payload["connects"] = [{"source": source, "target": target}]
    else:
        payload["connects"] = []
    if aliases:
        payload["aliases"] = aliases
    return await conn.send_command("manage_blueprint_graph", payload)


@register_tool("bluecode_delete", "Explicitly deletes Blueprint bluecode targets with confirmation.")
async def bluecode_delete(targets: List[Any], confirm_delete: bool = False, aliases: Dict[str, str] = None) -> Dict[str, Any]:
    """
    (Description loaded from prompts.json)
    """
    if not confirm_delete:
        return {"success": False, "error": "bluecode_delete requires confirm_delete=true"}

    conn = get_unreal_connection()
    payload: Dict[str, Any] = {
        "subAction": "bluecode_delete",
        "targets": targets or [],
        "confirm_delete": True,
    }
    if aliases:
        payload["aliases"] = aliases
    return await conn.send_command("manage_blueprint_graph", payload)


@register_tool("bluecode_compile", "Compiles and saves the active Blueprint, returning concise diagnostics.")
async def bluecode_compile(blueprint_name: str = None) -> Dict[str, Any]:
    """
    (Description loaded from prompts.json)
    """
    conn = get_unreal_connection()
    return await conn.send_command("compile_blueprint", {"blueprint_name": blueprint_name})


@register_tool("bluecode_search_nodes", "Searches context-valid Blueprint menu nodes and returns stable action handles.")
async def bluecode_search_nodes(query: str = "", category: str = "", node_class: str = "", node_class_path: str = "", max_count: int = 50, include_pins: bool = False, context_sensitive: bool = True, exact: bool = False) -> Dict[str, Any]:
    """
    (Description loaded from prompts.json)
    """
    conn = get_unreal_connection()
    payload = {
        "subAction": "bluecode_search_nodes",
        "query": query,
        "max_count": max_count,
        "include_pins": include_pins,
        "context_sensitive": context_sensitive,
        "exact": exact
    }
    if category:
        payload["category"] = category
    if node_class:
        payload["node_class"] = node_class
    if node_class_path:
        payload["node_class_path"] = node_class_path
    return await conn.send_command("manage_blueprint_graph", payload)


@register_tool("add_step", "Adds an Executable Node to the current Program Counter.")
async def add_step(name: str = "", args: List[Any] = None, comment: str = None, input_wires: Dict[str, Any] = None, member_class: str = None, action_handle: str = "", category: str = "", node_class: str = "") -> Dict[str, Any]:
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

    if member_class:
        payload["memberClass"] = member_class

    if action_handle:
        payload["action_handle"] = action_handle
    if category:
        payload["category"] = category
    if node_class:
        payload["node_class"] = node_class

    # We reuse 'manage_blueprint_graph' for all graph ops
    return await conn.send_command("manage_blueprint_graph", payload)


@register_tool("prepare_value", "Places a Non-Executable Data Node (e.g. variable getter, literal).")
async def prepare_value(name: str = "", args: List[Any] = None, action_handle: str = "", category: str = "", node_class: str = "") -> Dict[str, Any]:
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
    if action_handle:
        payload["action_handle"] = action_handle
    if category:
        payload["category"] = category
    if node_class:
        payload["node_class"] = node_class
        
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

@register_tool("animation_widget_properties", "Gets animated properties for a widget (timeline view).")
async def animation_widget_properties(animation_name: str = "", widget_name: str = "", property_name: str = "") -> Dict[str, Any]:
    conn = get_unreal_connection()
    sequencer_client = UMGSequencer.UMGSequencer(conn)
    return await sequencer_client.get_widget_properties(animation_name, widget_name, property_name)

@register_tool("animation_time_properties", "Gets property values at specific times.")
async def animation_time_properties(times: List[float], animation_name: str = "", widget_name: str = "", property_name: str = "") -> Dict[str, Any]:
    conn = get_unreal_connection()
    sequencer_client = UMGSequencer.UMGSequencer(conn)
    return await sequencer_client.get_time_properties(times, animation_name, widget_name, property_name)

@register_tool("animation_overview", "Summarizes keyframes and tracks for the animation.")
async def animation_overview(animation_name: str = "", widget_name: str = "", property_name: str = "") -> Dict[str, Any]:
    conn = get_unreal_connection()
    sequencer_client = UMGSequencer.UMGSequencer(conn)
    return await sequencer_client.get_animation_overview(animation_name, widget_name, property_name)

# --- Write (Action) ---

@register_tool("create_animation", "Creates a new animation.")
async def create_animation(animation_name: str) -> Dict[str, Any]:
    """
    (Description loaded from prompts.json)
    """
    conn = get_unreal_connection()
    sequencer_client = UMGSequencer.UMGSequencer(conn)
    return await sequencer_client.create_animation(asset_path=None, animation_name=animation_name)

@register_tool("delete_animation", "Deletes an animation (requires confirm_delete=true).")
async def delete_animation(animation_name: str, confirm_delete: bool = False) -> Dict[str, Any]:
    """
    (Description loaded from prompts.json)
    """
    conn = get_unreal_connection()
    sequencer_client = UMGSequencer.UMGSequencer(conn)
    return await sequencer_client.delete_animation(asset_path=None, animation_name=animation_name, confirm_delete=confirm_delete)

@register_tool("set_property_keys", "Sets keyframes for a property.")
async def set_property_keys(property_name: str, keys: List[Dict[str, Any]]) -> Dict[str, Any]:
    """
    (Description loaded from prompts.json)
    """
    conn = get_unreal_connection()
    sequencer_client = UMGSequencer.UMGSequencer(conn)
    return await sequencer_client.set_property_keys(property_name, keys)

@register_tool("remove_property_track", "Removes a property track (confirm_delete required).")
async def remove_property_track(property_name: str, confirm_delete: bool = False) -> Dict[str, Any]:
    """
    (Description loaded from prompts.json)
    """
    conn = get_unreal_connection()
    sequencer_client = UMGSequencer.UMGSequencer(conn)
    return await sequencer_client.remove_property_track(property_name, confirm_delete)

@register_tool("remove_keys", "Removes specific keys (confirm_delete required).")
async def remove_keys(property_name: str, times: List[float], confirm_delete: bool = False) -> Dict[str, Any]:
    """
    (Description loaded from prompts.json)
    """
    conn = get_unreal_connection()
    sequencer_client = UMGSequencer.UMGSequencer(conn)
    return await sequencer_client.remove_keys(property_name, times, confirm_delete)

@register_tool("animation_append_widget_tracks", "Append/overwrite property keys from the widget perspective.")
async def animation_append_widget_tracks(widget_name: str, tracks: List[Dict[str, Any]], animation_name: str = "") -> Dict[str, Any]:
    conn = get_unreal_connection()
    sequencer_client = UMGSequencer.UMGSequencer(conn)
    return await sequencer_client.append_widget_tracks(widget_name, tracks, animation_name)

@register_tool("animation_append_time_slice", "Append a time-slice of widget properties (diff recommended).")
async def animation_append_time_slice(time: float, widgets: List[Dict[str, Any]], animation_name: str = "") -> Dict[str, Any]:
    conn = get_unreal_connection()
    sequencer_client = UMGSequencer.UMGSequencer(conn)
    return await sequencer_client.append_time_slice(time, widgets, animation_name)

@register_tool("animation_delete_widget_keys", "Delete keys for a widget/property at specific times (confirm_delete required).")
async def animation_delete_widget_keys(property_name: str, times: List[float], widget_name: str = "", animation_name: str = "", confirm_delete: bool = False) -> Dict[str, Any]:
    conn = get_unreal_connection()
    sequencer_client = UMGSequencer.UMGSequencer(conn)
    return await sequencer_client.delete_widget_keys(property_name, times, widget_name, animation_name, confirm_delete)


# =============================================================================
#  Category: Material
# =============================================================================

@register_tool("material_set_target", "Specify target asset (path or parent). Required for stateful editing.")
async def material_set_target(path: str, create_if_not_found: bool = True) -> Dict[str, Any]:
    """
    (Description loaded from prompts.json)
    """
    conn = get_unreal_connection()
    material_client = UMGMaterial.UMGMaterial(conn)
    return await material_client.set_target_material(path, create_if_not_found)

@register_tool("material_modify_type", "Modify the active material type for non-UMG materials.")
async def material_modify_type(
    path: Optional[str] = None,
    domain: Optional[str] = None,
    blend_mode: Optional[str] = None,
    shading_model: Optional[str] = None,
    two_sided: Optional[bool] = None,
    refresh_hlsl_wiring: bool = True
) -> Dict[str, Any]:
    """
    (Description loaded from prompts.json)
    """
    conn = get_unreal_connection()
    material_client = UMGMaterial.UMGMaterial(conn)
    return await material_client.modify_type(path, domain, blend_mode, shading_model, two_sided, refresh_hlsl_wiring)

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
async def hlsl_set_target(
    path: str,
    confirm_overwrite: bool = False,
    create_if_not_found: bool = True,
    domain: Optional[str] = None,
    blend_mode: Optional[str] = None,
    shading_model: Optional[str] = None,
    two_sided: Optional[bool] = None
) -> Dict[str, Any]:
    """
    (Description loaded from prompts.json)
    """
    conn = get_unreal_connection()
    material_client = UMGMaterial.UMGMaterial(conn)
    return await material_client.hlsl_set_target(path, confirm_overwrite, create_if_not_found, domain, blend_mode, shading_model, two_sided)


@register_tool("hlsl_get", "Read current HLSL code and parameter definitions.")
async def hlsl_get() -> Dict[str, Any]:
    """
    (Description loaded from prompts.json)
    """
    conn = get_unreal_connection()
    material_client = UMGMaterial.UMGMaterial(conn)
    return await material_client.hlsl_get()


@register_tool("hlsl_set", "Incrementally apply HLSL code, parameters, and/or outputs; union-only, no deletion.")
async def hlsl_set(
    hlsl: Optional[str] = None,
    parameters: Optional[List[Dict[str, Any]]] = None,
    outputs: Optional[List[Any]] = None
) -> Dict[str, Any]:
    """
    (Description loaded from prompts.json)
    """
    conn = get_unreal_connection()
    material_client = UMGMaterial.UMGMaterial(conn)
    return await material_client.hlsl_set(hlsl, parameters, outputs)


@register_tool("hlsl_delete_parameter", "Explicitly delete HLSL parameters from the active HLSL material target.")
async def hlsl_delete_parameter(names: List[str], confirm_delete: bool = False) -> Dict[str, Any]:
    """
    (Description loaded from prompts.json)
    """
    conn = get_unreal_connection()
    material_client = UMGMaterial.UMGMaterial(conn)
    return await material_client.hlsl_delete_parameter(names, confirm_delete)


@register_tool("hlsl_delete", "Explicitly delete HLSL parameters or outputs from the active HLSL material target.")
async def hlsl_delete(names: List[str], confirm_delete: bool = False, kind: Optional[str] = None) -> Dict[str, Any]:
    """
    (Description loaded from prompts.json)
    """
    conn = get_unreal_connection()
    material_client = UMGMaterial.UMGMaterial(conn)
    return await material_client.hlsl_delete(names, confirm_delete, kind)


@register_tool("hlsl_delete_output", "Explicitly delete HLSL outputs from the active HLSL material target.")
async def hlsl_delete_output(names: List[str], confirm_delete: bool = False) -> Dict[str, Any]:
    """
    (Description loaded from prompts.json)
    """
    conn = get_unreal_connection()
    material_client = UMGMaterial.UMGMaterial(conn)
    return await material_client.hlsl_delete_output(names, confirm_delete)


@register_tool("hlsl_compile", "Compile and save current HLSL material target and return concise diagnostics.")
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
        sys.stderr.write("MCP_SERVER_START_FAILED\n")
        sys.stderr.flush()
