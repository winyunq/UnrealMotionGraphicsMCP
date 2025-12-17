# UMGEditor.py

from typing import Dict, Any, List, Optional

class UMGEditor:
    def __init__(self, client):
        self.client = client

    def get_actors_in_level(self) -> Dict[str, Any]:
        """Gets a list of all actors in the current level."""
        return self.client.send_command("get_actors_in_level")

    def find_actors_by_name(self, pattern: str) -> Dict[str, Any]:
        """Finds actors whose names contain the given pattern."""
        return self.client.send_command("find_actors_by_name", {"pattern": pattern})

    def spawn_actor(self, actor_type: str, name: str, location: List[float] = None, 
                   rotation: List[float] = None, scale: List[float] = None, 
                   static_mesh: str = None) -> Dict[str, Any]:
        """Spawns an actor in the level."""
        params = {
            "type": actor_type,
            "name": name
        }
        if location: params["location"] = location
        if rotation: params["rotation"] = rotation
        if scale: params["scale"] = scale
        if static_mesh: params["static_mesh"] = static_mesh
        
        return self.client.send_command("spawn_actor", params)

    def delete_actor(self, name: str) -> Dict[str, Any]:
        """Deletes an actor by name."""
        return self.client.send_command("delete_actor", {"name": name})

    def set_actor_transform(self, name: str, location: List[float] = None, 
                           rotation: List[float] = None, scale: List[float] = None) -> Dict[str, Any]:
        """Sets the transform of an actor."""
        params = {"name": name}
        if location: params["location"] = location
        if rotation: params["rotation"] = rotation
        if scale: params["scale"] = scale
        
        return self.client.send_command("set_actor_transform", params)

    def list_assets(self, class_name: str = None, package_path: str = None, 
                   max_count: int = 100) -> Dict[str, Any]:
        """Lists assets in the project matching the criteria."""
        params = {
            "max_count": max_count
        }
        if class_name:
            params["class_name"] = class_name
        if package_path:
            params["package_path"] = package_path
        
        # We implicitly default to recursive=true for both paths and classes in the C++ layer
        # unless specified otherwise, but for simplicity we don't even expose false here
        # to keep the AI interface clean.
            
        return self.client.send_command("list_assets", params)

    def refresh_asset_registry(self, paths: List[str] = None) -> Dict[str, Any]:
        """Refreshes the asset registry to discover new assets."""
        params = {}
        if paths:
            params["paths"] = paths
        return self.client.send_command("refresh_asset_registry", params)
