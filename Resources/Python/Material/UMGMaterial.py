import logging

logger = logging.getLogger("UmgMcpServer")

class UMGMaterial:
    """
    Client for the 'Material' category of commands (5 Pillars API).
    """
    def __init__(self, connection):
        self.connection = connection

    async def set_target_material(self, path: str, create_if_not_found: bool = True) -> dict:
        # Note: We keep signature for compatibility but C++ side now defaults create=true
        params = {"path": path}
        # We can still pass it if we want, but user asked to simplify.
        # Let's just pass path.
        return await self.connection.send_command("material_set_target", params)

    async def define_variable(self, name: str, type: str) -> dict:
        return await self.connection.send_command("material_define_variable", {"name": name, "type": type})

    async def add_node(self, symbol: str, handle: str = None) -> dict:
        params = {"symbol": symbol}
        if handle:
            params["handle"] = handle
        return await self.connection.send_command("material_add_node", params)
    
    async def delete_node(self, handle: str) -> dict:
        return await self.connection.send_command("material_delete", {"handle": handle})

    async def connect_nodes(self, from_handle: str, to_handle: str) -> dict:
        return await self.connection.send_command("material_connect_nodes", {"from": from_handle, "to": to_handle})

    async def connect_pins(self, source: str, target: str, source_pin: str = None, target_pin: str = None) -> dict:
        params = {
            "source": source,
            "target": target
        }
        if source_pin: params["source_pin"] = source_pin
        if target_pin: params["target_pin"] = target_pin
        return await self.connection.send_command("material_connect_pins", params)

    async def set_hlsl_node_io(self, handle: str, code: str, inputs: list) -> dict:
        return await self.connection.send_command("material_set_hlsl_node_io", {
            "handle": handle,
            "code": code,
            "inputs": inputs
        })

    async def set_node_properties(self, handle: str, properties: dict) -> dict:
        return await self.connection.send_command("material_set_node_properties", {
            "handle": handle,
            "properties": properties
        })

    async def compile_asset(self) -> dict:
        """
        Compiles the currently targeted material.
        """
        return await self.connection.send_command("material_compile_asset", {})

    async def get_node_pins(self, handle: str) -> dict:
        """
        Introspects the available pins for a given node or 'Master'.
        """
        return await self.connection.send_command("material_get_pins", {"handle": handle})

    async def get_graph(self) -> dict:
        """
        Retrieves the full graph topology (nodes and connections).
        """
        return await self.connection.send_command("material_get_graph", {})

    async def send_command(self, command: str, params: dict) -> dict:
        """
        Generic command sender exposed for advanced usage.
        """
        return await self.connection.send_command(command, params)
