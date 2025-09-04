"""
Actor utility functions for Unreal MCP Server.
Contains helper functions for spawning and managing actors.
"""
from typing import Dict, Any, List
import logging

logger = logging.getLogger(__name__)


def spawn_blueprint_actor(
    unreal_connection,
    blueprint_name: str,
    actor_name: str,
    location: List[float] = [0, 0, 0],
    rotation: List[float] = [0, 0, 0]
) -> Dict[str, Any]:
    """
    Spawn an actor from a Blueprint using the provided Unreal connection.
    
    Args:
        unreal_connection: The Unreal Engine connection object
        blueprint_name: Name of the blueprint to spawn
        actor_name: Name to give the spawned actor
        location: [x, y, z] position to spawn at
        rotation: [roll, pitch, yaw] rotation to apply
        
    Returns:
        Dict containing success status and result data
    """
    try:
        if not unreal_connection:
            return {"success": False, "message": "No Unreal connection provided"}
        
        params = {
            "blueprint_name": blueprint_name,
            "actor_name": actor_name,
            "location": location,
            "rotation": rotation
        }
        
        response = unreal_connection.send_command("spawn_blueprint_actor", params)
        return response or {"success": False, "message": "No response from Unreal"}
        
    except Exception as e:
        logger.error(f"spawn_blueprint_actor helper error: {e}")
        return {"success": False, "message": str(e)}
