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
    A simple async connection helper compatible with the Server's UnrealConnection interface.
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
            
            # Send (mimicking UnrealConnection logic)
            writer.write(json_str.encode('utf-8') + b'\0')
            await writer.drain()
            
            if writer.can_write_eof():
                writer.write_eof()

            # Read
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
            
            # Simple log
            status = response.get("success", False) or response.get("status") == "success"
            res_str = "SUCCESS" if status else "FAIL"
            print(f"[RECV] {res_str}: {response}")
            
            return response
            
        except Exception as e:
            print(f"[ERROR] {e}")
            return {"success": False, "error": str(e)}
        finally:
            if writer:
                writer.close()
                await writer.wait_closed()

async def run_demo():
    print("=== 启动 'Holographic Terrain' 材质生成演示 (Architecture Compliant) ===")
    
    conn = SimpleConnection()
    mat_api = UMGMaterial(conn)
    
    # 1. Context
    path = "/Game/UMGMCP/Demos/M_HoloTerrain_Arch"
    res = await mat_api.set_target_material(path)
    if not res.get("success"): return

    # 2. Inputs
    h_scale = (await mat_api.define_variable("HeightScale", "Scalar")).get("handle")
    speed = (await mat_api.define_variable("ScrollSpeed", "Scalar")).get("handle")
    tiling = (await mat_api.define_variable("GridTiling", "Scalar")).get("handle")
    
    # 3. Nodes
    world_pos = (await mat_api.add_node("WorldPosition", "AbsoluteWorldPos")).get("handle")
    time_node = (await mat_api.add_node("Time", "GameTime")).get("handle")
    
    # 4. HLSL
    hlsl_code = """
    // Inputs: WorldPos, Time, Scale, Speed, Tiling
    // Architecture Test Pattern
    
    float2 UV = WorldPos.xy * Tiling + float2(Time * Speed, 0);
    float Height = sin(UV.x) * cos(UV.y) * Scale;
    float Grid = step(0.9, frac(UV.x * 10)) + step(0.9, frac(UV.y * 10));
    
    return float4(Height, Grid, 0, 1);
    """
    
    logic = (await mat_api.add_node("Custom", "TerrainLogic")).get("handle")
    await mat_api.set_hlsl_node_io(logic, hlsl_code, ["WorldPos", "Time", "Scale", "Speed", "Tiling"])

    # 5. Connect (Using Helper which wraps send_command)
    if logic:
        await mat_api.connect_pins(world_pos, logic, None, "WorldPos")
        await mat_api.connect_pins(time_node, logic, None, "Time")
        await mat_api.connect_pins(h_scale, logic, None, "Scale")
        await mat_api.connect_pins(speed, logic, None, "Speed")
        await mat_api.connect_pins(tiling, logic, None, "Tiling")
        
        # Connect to Master
        # logic -> BaseColor
        await mat_api.connect_nodes(logic, "Master:BaseColor")
        # logic -> Emissive
        await mat_api.connect_nodes(logic, "Master:EmissiveColor")

    # 6. Compile
    await mat_api.compile_asset()
    print("=== 演示完成 ===")

if __name__ == "__main__":
    asyncio.run(run_demo())
