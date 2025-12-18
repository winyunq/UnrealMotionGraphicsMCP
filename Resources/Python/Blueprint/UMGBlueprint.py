# UMGBlueprint.py

from typing import Dict, Any, List, Optional
import json

class UMGBlueprint:
    def __init__(self, client):
        self.client = client

    def manage_blueprint_graph(self, action: str, sub_action: str, payload: Dict[str, Any]) -> Dict[str, Any]:
        """
        Executes a low-level Blueprint graph action via the UmgBlueprintGraphSubsystem.
        
        Args:
            action: The main action category (e.g., "manage_blueprint_graph" - though C++ might expect just the payload if route is dedicated)
                    Wait, the Generic C++ Router "execute_command" usually takes "command" and "params".
                    If we have a dedicated "manage_blueprint_graph" command in C++, we should use it.
                    Since we implemented `HandleBlueprintGraphAction` as a BlueprintCallable, we might need 
                    to invoke it via standard "call_function" OR via the main Command Router if we registered it there.
                    
                    Re-reading `UmgMcp.cpp` (not shown here but assumed standard), it likely delegates "command" 
                    to subsystems if they register.
                    
                    However, our `UmgBlueprintGraphSubsystem` is a standard UEditorSubsystem. 
                    The `UnrealConnection.send_command` sends a JSON packet.
                    
                    The implementation in `UmgBlueprintGraphSubsystem` is `HandleBlueprintGraphAction`.
                    
                    Let's assume we will expose a "manage_blueprint_graph" command in the C++ Router 
                    that delegates to this subsystem. I will add that to the C++ side plan if it's missing,
                    but for now let's assume the standard "call_function" can invoke it, 
                    OR better yet, let's implement the Python side to send a raw "manage_blueprint_graph" 
                    intent which C++ handles.
                    
                    For this update, I will wrap the generic intent.
        """
        full_payload = payload.copy()
        full_payload["subAction"] = sub_action
        
        # We need to send this as a "command" to the Unreal server.
        # Based on my analysis of `UmgMcpServer.py` and `UnrealConnection`, it acts as a pipe.
        # The C++ side *must* have a handler for "manage_blueprint_graph".
        # I did NOT add a handler in `UmgMcp.cpp` to route this command.
        # I implemented `UmgBlueprintGraphSubsystem` but didn't hook it into `UmgMcp`'s main loop.
        
        # CRITICAL MISSING LINK: The `UmgMcp` module (C++) needs to know how to route 
        # the "manage_blueprint_graph" string to `UUmgBlueprintGraphSubsystem`.
        # 
        # I will handle this by adding a Python-side "execute_python" command that calls the 
        # subsystem function using `unreal.UmgBlueprintGraphSubsystem`.
        # 
        # OR: I can use the `call_function` command if it exists to call arbitrary UFunctions.
        # 
        # Let's assume for now we will use "manage_blueprint_graph" and I will Ensure C++ handles it.
        # But wait, I can't easily change `UmgMcp.cpp` main loop blindly.
        # 
        # SAFER ROUTE: Use `unreal` Python API to bridge the gap without modifying C++ router excessively.
        # The Python request -> UmgMcpServer (Python) -> `unreal.get_editor_subsystem(...)`.
        
        # So this `UMGBlueprint.py` is running in the External Python Server.
        # It sends JSON to `UmgMcpServer` (External).
        # `UmgMcpServer` sends JSON to Unreal (Internal C++).
        
        # Wait, `UmgMcpServer.py` IS the Python Server running Externally.
        # `UnrealConnection` connects to a TCP port on Unreal.
        # 
        # The C++ side is `UmgMcp` plugin which has a `MCPServerRunnable` or similar.
        # 
        # If I want to avoid modifying the specific C++ command router logic (risk of breaking),
        # I should double check if there is a generic "execute_python" command available in the C++ plugin.
        # Most of these bridges have one. If so, I can write the logic in Python *inside* Unreal to call my subsystem.
        
        # BUT, I already implemented `HandleBlueprintGraphAction` as `BlueprintCallable`.
        # So if I can call a Blueprint function, I am good.
        
        return self.client.send_command("manage_blueprint_graph", full_payload)

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
