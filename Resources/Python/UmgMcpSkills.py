import asyncio
import json
import logging
import os
import sys
from typing import Dict, Any, List, Optional

# Setup logging
logging.basicConfig(level=logging.INFO, stream=sys.stderr)
logger = logging.getLogger("UmgMcpSkills")

# Import mcp_config from parent dir
CURRENT_DIR = os.path.dirname(os.path.abspath(__file__))
sys.path.append(CURRENT_DIR)
try:
    from mcp_config import UNREAL_HOST, UNREAL_PORT
except ImportError:
    UNREAL_HOST = "127.0.0.1"
    UNREAL_PORT = 7999

class UnrealConnection:
    """Connection helper for Skill mode."""
    async def send_command(self, command: str, params: Dict[str, Any] = None) -> Dict[str, Any]:
        try:
            reader, writer = await asyncio.open_connection(UNREAL_HOST, UNREAL_PORT)
            payload = json.dumps({"command": command, "params": params or {}})
            writer.write(payload.encode('utf-8') + b'\0')
            await writer.drain()
            
            if writer.can_write_eof():
                writer.write_eof()
                
            data = await reader.read(1024 * 1024) # 1MB buffer
            response_str = data.decode('utf-8').split('\0')[0]
            writer.close()
            await writer.wait_closed()
            return json.loads(response_str)
        except Exception as e:
            return {"status": "error", "error": str(e)}

_conn = UnrealConnection()

def run_async(coro):
    """Helper to run async in sync skill context."""
    return asyncio.run(coro)

# =============================================================================
#  Dynamic Skill Loading from prompts.json
# =============================================================================

def load_skills():
    """Reads prompts.json and registers functions to the global namespace."""
    json_path = os.path.abspath(os.path.join(CURRENT_DIR, "..", "prompts.json"))
    if not os.path.exists(json_path):
        logger.warning(f"prompts.json not found at {json_path}")
        return

    try:
        with open(json_path, 'r', encoding='utf-8') as f:
            data = json.load(f)
            tools = data.get("tools", [])
            
            for tool in tools:
                if not tool.get("enabled", True):
                    continue
                
                name = tool["name"]
                desc = tool.get("description", "No description provided.")
                
                # Create a wrapper function
                # We use a closure to capture the correct tool name
                def create_wrapper(command_name):
                    def skill_wrapper(**kwargs):
                        return run_async(_conn.send_command(command_name, kwargs))
                    return skill_wrapper
                
                func = create_wrapper(name)
                func.__name__ = name
                func.__doc__ = desc
                
                # Register in globals so Gemini CLI can see it as an export
                globals()[name] = func
                logger.info(f"Registered Skill: {name}")

    except Exception as e:
        logger.error(f"Error loading skills: {e}")

# Load all skills on module import
load_skills()