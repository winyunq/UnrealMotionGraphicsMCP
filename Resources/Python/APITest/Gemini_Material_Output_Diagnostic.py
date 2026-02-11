import asyncio
import sys
import os
import json

# Add parent directory to path
current_dir = os.path.dirname(os.path.abspath(__file__))
parent_dir = os.path.dirname(current_dir)
sys.path.append(parent_dir)

from Material.UMGMaterial import UMGMaterial
from mcp_config import UNREAL_HOST, UNREAL_PORT

class SimpleConnection:
    async def send_command(self, command: str, params: dict = None) -> dict:
        reader = None
        writer = None
        try:
            reader, writer = await asyncio.open_connection(UNREAL_HOST, UNREAL_PORT)
            payload = {"command": command, "params": params or {}}
            json_str = json.dumps(payload)
            writer.write(json_str.encode('utf-8') + b'\0')
            await writer.drain()
            
            chunks = []
            while True:
                chunk = await reader.read(4096)
                if not chunk: break
                if b'\x00' in chunk:
                    chunks.append(chunk[:chunk.find(b'\x00')])
                    break
                chunks.append(chunk)
            
            response_data = b"".join(chunks).decode('utf-8')
            full_res = json.loads(response_data)
            # Bridge wraps in "result"
            if "result" in full_res:
                return full_res["result"]
            return full_res
        except Exception as e:
            return {"success": False, "error": str(e)}
        finally:
            if writer:
                writer.close()
                await writer.wait_closed()

async def run_diagnostic():
    print("=== Material Output Node Introspection Diagnostic ===")
    conn = SimpleConnection()
    mat_api = UMGMaterial(conn)
    
    material_path = "/Game/UMGMCP/Demos/Gemini_Output_Test"
    print(f"Targeting material: {material_path}")
    await mat_api.set_target_material(material_path)
    
    # 1. Query Master Node Info
    print("\n[1] Querying Master Node (Root) Connections...")
    res = await conn.send_command("material_get_pins", {"handle": "Master"})
    if res.get("success"):
        pins = res.get("pins", [])
        connections = res.get("connections", {})
        print("DEBUG: Master Node introspection successful.")
        print("Available Pins (Stable IDs):")
        for p in pins:
            print(f"  - {p.get('id'):<20} ({p.get('name')})")
        
        print("\nCURRENT CONNECTIONS to Output Node:")
        if not connections:
            print("  (No connections found)")
        for target_pin, source_node in connections.items():
            print(f"  [CONNECTED] {source_node}  ----->  Master.{target_pin}")
    else:
        print(f"ERROR: Failed to get master pins. {res.get('error')}")
        return

    # 2. Create a test node and connect to verify
    print("\n[2] Creating test node to verify connection tracking...")
    color_node = await mat_api.add_node("Constant3Vector", "DiagnosticColor")
    color_handle = color_node.get("handle")
    
    if color_handle:
        print(f"Connecting {color_handle} to Master.BaseColor...")
        await mat_api.connect_pins(color_handle, "Master", None, "EmissiveColor")
        
        # 3. Final verification of introspection
        print("\n[3] Final Introspection after connection:")
        res = await conn.send_command("material_get_pins", {"handle": "Master"})
        final_conns = res.get("connections", {})
        for target_pin, source_node in final_conns.items():
            print(f"  [VERIFIED] {source_node}  ----->  Master.{target_pin}")
    else:
        print("ERROR: Could not create test node.")

if __name__ == "__main__":
    asyncio.run(run_diagnostic())
