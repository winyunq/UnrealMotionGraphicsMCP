"""
UmgSequencer Server - Your AI UMG Animation Partner

This MCP server is designed exclusively for interacting with the Sequencer and Animation
systems within the UmgMcp plugin in Unreal Engine 5.
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
from Animation import UMGSequencer

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - [%(filename)s:%(lineno)d] - %(message)s',
    handlers=[ logging.FileHandler('umg_sequencer_server.log') ]
)
logger = logging.getLogger("UmgSequencerServer")

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
    logger.info("UmgSequencer server starting up")
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
        logger.info("UmgSequencer server shut down")

mcp = FastMCP(
    "UmgSequencer",
    lifespan=server_lifespan
)

# =============================================================================
#  Dynamic Tool Configuration (Loaded from prompts.json)
# =============================================================================

TOOLS_CONFIG = {}

def load_tool_config():
    """Load tool definitions from prompts.json."""
    global TOOLS_CONFIG
    try:
        current_dir = os.path.dirname(os.path.abspath(__file__))
        json_path = os.path.abspath(os.path.join(current_dir, "..", "prompts.json"))
        
        if os.path.exists(json_path):
            with open(json_path, 'r', encoding='utf-8') as f:
                data = json.load(f)
                for tool in data.get("tools", []):
                    TOOLS_CONFIG[tool["name"]] = tool
            logger.info(f"Loaded {len(TOOLS_CONFIG)} tool definitions from prompts.json")
        else:
            logger.warning(f"prompts.json not found at {json_path}")
    except Exception as e:
        logger.error(f"Error loading prompts.json: {e}")

load_tool_config()

def register_tool(name: str, default_desc: str):
    """
    Decorator to register a tool with MCP only if it is enabled in prompts.json.
    """
    def decorator(func):
        config = TOOLS_CONFIG.get(name, {})
        is_enabled = config.get("enabled", True)
        if is_enabled:
            description = config.get("description", default_desc)
            return mcp.tool(name=name, description=description)(func)
        else:
            return func
    return decorator

# =============================================================================
#  Category: Attention & Context
# =============================================================================

@register_tool("set_animation_scope", "Sets the current animation scope.")
def set_animation_scope(animation_name: str) -> Dict[str, Any]:
    """
    (Description loaded from prompts.json)
    """
    conn = get_unreal_connection()
    sequencer_client = UMGSequencer.UMGSequencer(conn)
    return sequencer_client.set_animation_scope(animation_name)

@register_tool("set_widget_scope", "Sets the current widget scope.")
def set_widget_scope(widget_name: str) -> Dict[str, Any]:
    """
    (Description loaded from prompts.json)
    """
    conn = get_unreal_connection()
    sequencer_client = UMGSequencer.UMGSequencer(conn)
    return sequencer_client.set_widget_scope(widget_name)

# =============================================================================
#  Category: Read (Sensing)
# =============================================================================

@register_tool("get_all_animations", "Lists all animations.")
def get_all_animations() -> Dict[str, Any]:
    """
    (Description loaded from prompts.json)
    """
    conn = get_unreal_connection()
    
    # Resolve Path (Implicitly handled by C++ or UmgMcp context, but we pass None to indicate 'current')
    # Note: In this new design, we rely on the Engine's context.
    
    sequencer_client = UMGSequencer.UMGSequencer(conn)
    return sequencer_client.get_all_animations()

@register_tool("get_animation_keyframes", "Gets keyframes for an animation.")
def get_animation_keyframes(animation_name: str) -> Dict[str, Any]:
    """
    (Description loaded from prompts.json)
    """
    conn = get_unreal_connection()
    sequencer_client = UMGSequencer.UMGSequencer(conn)
    return sequencer_client.get_animation_keyframes(animation_name)

@register_tool("get_animated_widgets", "Gets widgets affected by animation.")
def get_animated_widgets(animation_name: str) -> Dict[str, Any]:
    """
    (Description loaded from prompts.json)
    """
    conn = get_unreal_connection()
    sequencer_client = UMGSequencer.UMGSequencer(conn)
    return sequencer_client.get_animated_widgets(animation_name)

@register_tool("get_animation_full_data", "Gets complete animation data.")
def get_animation_full_data(animation_name: str) -> Dict[str, Any]:
    """
    (Description loaded from prompts.json)
    """
    conn = get_unreal_connection()
    sequencer_client = UMGSequencer.UMGSequencer(conn)
    return sequencer_client.get_animation_full_data(animation_name)

@register_tool("get_widget_animation_data", "Gets data for a specific widget in animation.")
def get_widget_animation_data(animation_name: str, widget_name: str) -> Dict[str, Any]:
    """
    (Description loaded from prompts.json)
    """
    conn = get_unreal_connection()
    sequencer_client = UMGSequencer.UMGSequencer(conn)
    return sequencer_client.get_widget_animation_data(animation_name, widget_name)

# =============================================================================
#  Category: Write (Action)
# =============================================================================

@register_tool("create_animation", "Creates a new animation.")
def create_animation(animation_name: str) -> Dict[str, Any]:
    """
    (Description loaded from prompts.json)
    """
    conn = get_unreal_connection()
    sequencer_client = UMGSequencer.UMGSequencer(conn)
    return sequencer_client.create_animation(animation_name)

@register_tool("delete_animation", "Deletes an animation.")
def delete_animation(animation_name: str) -> Dict[str, Any]:
    """
    (Description loaded from prompts.json)
    """
    conn = get_unreal_connection()
    sequencer_client = UMGSequencer.UMGSequencer(conn)
    return sequencer_client.delete_animation(animation_name)

@register_tool("set_property_keys", "Sets keyframes for a property.")
def set_property_keys(property_name: str, keys: List[Dict[str, Any]]) -> Dict[str, Any]:
    """
    (Description loaded from prompts.json)
    """
    conn = get_unreal_connection()
    sequencer_client = UMGSequencer.UMGSequencer(conn)
    return sequencer_client.set_property_keys(property_name, keys)

@register_tool("remove_property_track", "Removes a property track.")
def remove_property_track(property_name: str) -> Dict[str, Any]:
    """
    (Description loaded from prompts.json)
    """
    conn = get_unreal_connection()
    sequencer_client = UMGSequencer.UMGSequencer(conn)
    return sequencer_client.remove_property_track(property_name)

@register_tool("remove_keys", "Removes specific keys.")
def remove_keys(property_name: str, times: List[float]) -> Dict[str, Any]:
    """
    (Description loaded from prompts.json)
    """
    conn = get_unreal_connection()
    sequencer_client = UMGSequencer.UMGSequencer(conn)
    return sequencer_client.remove_keys(property_name, times)

# =============================================================================
#  Category: Dynamic Prompts (Loaded from JSON)
# =============================================================================

def load_prompts():
    """Load prompts from the JSON file and register them with MCP."""
    try:
        current_dir = os.path.dirname(os.path.abspath(__file__))
        json_path = os.path.abspath(os.path.join(current_dir, "..", "prompts.json"))
        
        if not os.path.exists(json_path):
            logger.warning(f"prompts.json not found at {json_path}")
            return

        with open(json_path, 'r', encoding='utf-8') as f:
            data = json.load(f)

        # 1. Register Sequencer Action Prompts
        if "Animation Sequencer" in data:
            for item in data["Animation Sequencer"]:
                name = item.get("name", "Unknown")
                prompt_text = item.get("prompt", "")
                
                def make_prompt_func(text_content):
                    def p_func():
                        return text_content
                    return p_func

                safe_name = name.lower().replace(" ", "_")
                mcp.prompt(name=safe_name, description=name)(make_prompt_func(prompt_text))
                logger.info(f"Registered prompt: {safe_name}")

        # 2. Register Sequencer Info (Documentation)
        doc_category = data.get("Server Documentation", [])
        for item in doc_category:
            if item.get("name") == "Sequencer Info":
                info_text = item.get("prompt", "No documentation found.")
                
                @mcp.prompt(name="info")
                def info():
                    """Run method:"tools/list" to get more details."""
                    return info_text
                
                logger.info("Registered 'info' prompt from JSON.")
                break

    except Exception as e:
        logger.error(f"Failed to load prompts from JSON: {e}")

load_prompts()

if __name__ == "__main__":
    port = UNREAL_PORT
    try:
        # Step 1: Start the main server and begin listening for connections from Gemini CLI.
        logger.info(f"Starting UmgSequencer Server on port {port}. Waiting for connections...")
        mcp.run(transport='stdio')

    except Exception as e:
        logger.error(f"A critical error occurred while starting the UmgSequencer Server: {e}")
        # If we failed, print an error code that the parent process might see.
        print("MCP_SERVER_START_FAILED")
