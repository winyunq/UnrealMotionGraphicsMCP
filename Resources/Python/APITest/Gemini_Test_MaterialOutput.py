import asyncio
import sys
import os
import json

# Add parent directory to path to import modules
current_dir = os.path.dirname(os.path.abspath(__file__))
parent_dir = os.path.dirname(current_dir)
sys.path.append(parent_dir)

from Material.UMGMaterial import UMGMaterial
from mcp_config import UNREAL_HOST, UNREAL_PORT

class SimpleConnection:
    """
    Async connection helper.
    """
    async def send_command(self, command: str, params: dict = None) -> dict:
        reader = None
        writer = None
        try:
            reader, writer = await asyncio.open_connection(UNREAL_HOST, UNREAL_PORT)
            
            payload = {
                "command": command,
                "params": params or {}
            }
            
            json_str = json.dumps(payload)
            print(f"[SEND] {command} : {json.dumps(params or {})}")
            
            writer.write(json_str.encode('utf-8') + b'\0')
            await writer.drain()
            
            if writer.can_write_eof():
                writer.write_eof()

            chunks = []
            while True:
                chunk = await reader.read(4096)
                if not chunk: break
                if b'\x00' in chunk:
                    chunks.append(chunk[:chunk.find(b'\x00')])
                    break
                chunks.append(chunk)
            
            response_data = b"".join(chunks).decode('utf-8')
            response = json.loads(response_data)
            return response
            
        except Exception as e:
            print(f"[ERROR] {e}")
            return {"success": False, "error": str(e)}
        finally:
            if writer:
                writer.close()
                await writer.wait_closed()

def get_handle(response):
    if response and isinstance(response, dict):
        result = response.get("result", response)
        return result.get("handle")
    return None

async def run_output_test():
    print("=== Testing material_set_output_node ===")
    
    conn = SimpleConnection()
    mat_api = UMGMaterial(conn)
    
    # 1. Create a Test Material
    material_path = "/Game/Gemini/Trans/M_Gemini_TestOutput_V1"
    print(f"\n[1] Creating Material: {material_path}")
    await mat_api.set_target_material(material_path)
    
    # 2. Add a Vector Parameter (Green Color)
    print("\n[2] Adding Color Output Node...")
    color_node = get_handle(await mat_api.define_variable("OutputColor", "Vector"))
    
    # Set it to Green
    await mat_api.set_node_properties(color_node, {"R": 0.0, "G": 1.0, "B": 0.0, "A": 1.0})
    
    # 3. Connect to Output
    print(f"\n[3] Setting Output Node: {color_node}")
    # Raw command since UMGMaterial wrapper might not be updated yet
    res = await conn.send_command("material_set_output_node", {"handle": color_node})
    if res.get("success"):
        print("SUCCESS: Output Node Set.")
    else:
        print(f"FAIL: {res.get('error')}")

    # 4. Introspect to Verify
    print("\n[4] Verifying Connection...")
    info = await mat_api.get_node_pins("Master") # or explicit name
    connections = info.get("connections", {})
    
    print(f"Master Connections: {connections}")
    
    emissive = connections.get("EmissiveColor")
    base_color = connections.get("BaseColor")
    
    # Check if either is connected to our node
    # Note: GetNodeInfo return name might be "OutputColor" or "MaterialExpressionVectorParameter_0"
    # We should match loosely or by exact name if we knew it.
    
    # Let's see what the log says.
    
    print("\n=== Test Complete ===")

if __name__ == "__main__":
    asyncio.run(run_output_test())
