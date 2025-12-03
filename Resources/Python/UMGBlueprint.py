# UMGBlueprint.py

from typing import Dict, Any, List, Optional

class UMGBlueprint:
    def __init__(self, client):
        self.client = client

    def create_blueprint(self, name: str, parent_class: str = "AActor") -> Dict[str, Any]:
        """Creates a new Blueprint asset."""
        return self.client.send_command("create_blueprint", {
            "name": name,
            "parent_class": parent_class
        })

    def add_component_to_blueprint(self, blueprint_name: str, component_type: str, 
                                  component_name: str, location: List[float] = None, 
                                  rotation: List[float] = None, scale: List[float] = None) -> Dict[str, Any]:
        """Adds a component to a Blueprint."""
        params = {
            "blueprint_name": blueprint_name,
            "component_type": component_type,
            "component_name": component_name
        }
        if location: params["location"] = location
        if rotation: params["rotation"] = rotation
        if scale: params["scale"] = scale
        
        return self.client.send_command("add_component_to_blueprint", params)

    def set_physics_properties(self, blueprint_name: str, component_name: str, 
                              simulate_physics: bool = None, mass: float = None, 
                              linear_damping: float = None, angular_damping: float = None) -> Dict[str, Any]:
        """Sets physics properties for a component in a Blueprint."""
        params = {
            "blueprint_name": blueprint_name,
            "component_name": component_name
        }
        if simulate_physics is not None: params["simulate_physics"] = simulate_physics
        if mass is not None: params["mass"] = mass
        if linear_damping is not None: params["linear_damping"] = linear_damping
        if angular_damping is not None: params["angular_damping"] = angular_damping
        
        return self.client.send_command("set_physics_properties", params)

    def compile_blueprint(self, blueprint_name: str) -> Dict[str, Any]:
        """Compiles a Blueprint."""
        return self.client.send_command("compile_blueprint", {"blueprint_name": blueprint_name})

    def set_static_mesh_properties(self, blueprint_name: str, component_name: str, 
                                  static_mesh: str = None, material: str = None) -> Dict[str, Any]:
        """Sets static mesh properties for a component."""
        params = {
            "blueprint_name": blueprint_name,
            "component_name": component_name
        }
        if static_mesh: params["static_mesh"] = static_mesh
        if material: params["material"] = material
        
        return self.client.send_command("set_static_mesh_properties", params)

    def spawn_blueprint_actor(self, blueprint_name: str, actor_name: str, 
                             location: List[float] = None, rotation: List[float] = None) -> Dict[str, Any]:
        """Spawns an actor from a Blueprint."""
        params = {
            "blueprint_name": blueprint_name,
            "actor_name": actor_name
        }
        if location: params["location"] = location
        if rotation: params["rotation"] = rotation
        
        return self.client.send_command("spawn_blueprint_actor", params)

    def set_mesh_material_color(self, blueprint_name: str, component_name: str, 
                               color: List[float], material_slot: int = 0, 
                               parameter_name: str = "BaseColor", material_path: str = None) -> Dict[str, Any]:
        """Sets a vector parameter (color) on a mesh component's material."""
        params = {
            "blueprint_name": blueprint_name,
            "component_name": component_name,
            "color": color,
            "material_slot": material_slot,
            "parameter_name": parameter_name
        }
        if material_path: params["material_path"] = material_path
        
        return self.client.send_command("set_mesh_material_color", params)

    def get_available_materials(self, search_path: str = "", include_engine_materials: bool = True) -> Dict[str, Any]:
        """Gets a list of available materials."""
        params = {
            "search_path": search_path,
            "include_engine_materials": include_engine_materials
        }
        return self.client.send_command("get_available_materials", params)

    def apply_material_to_actor(self, actor_name: str, material_path: str, slot_index: int = 0) -> Dict[str, Any]:
        """Applies a material to an actor in the level."""
        return self.client.send_command("apply_material_to_actor", {
            "actor_name": actor_name,
            "material_path": material_path,
            "slot_index": slot_index
        })

    def apply_material_to_blueprint(self, blueprint_name: str, component_name: str, 
                                   material_path: str, slot_index: int = 0) -> Dict[str, Any]:
        """Applies a material to a component in a Blueprint."""
        return self.client.send_command("apply_material_to_blueprint", {
            "blueprint_name": blueprint_name,
            "component_name": component_name,
            "material_path": material_path,
            "slot_index": slot_index
        })

    def get_actor_material_info(self, actor_name: str) -> Dict[str, Any]:
        """Gets material information for an actor."""
        return self.client.send_command("get_actor_material_info", {"actor_name": actor_name})
