import asyncio
import sys
import os
import json

# Add parent directory to path to import modules
current_dir = os.path.dirname(os.path.abspath(__file__))
parent_dir = os.path.dirname(current_dir)
sys.path.append(parent_dir)

from Material.UMGMaterial import UMGMaterial
from Widget.UMGSet import UMGSet
from Widget.UMGGet import UMGGet
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

async def run_fancy_slider_demo():
    print("=== 启动 'Cyberpunk Searchlight' 终极演示 ===")
    
    conn = SimpleConnection()
    mat_api = UMGMaterial(conn)
    widget_set = UMGSet(conn)
    
    # --- PHASE 1: Design Background Material (Searchlight) ---
    print("\n[1] 设计搜索灯背景材质...")
    bg_mat_path = "/Game/UMGMCP/Demos/M_CyberBackground"
    await mat_api.set_target_material(bg_mat_path)
    
    await mat_api.set_node_properties("Output", {
        "MaterialDomain": "MD_UI",
        "BlendMode": "BLEND_Opaque"
    })
    
    # 动态参数：由滑条控制旋转位置
    rotation_pos = get_handle(await mat_api.define_variable("RotationPos", "Scalar"))
    
    bg_hlsl = (
        "// Inputs: UV, iTime, RotationPos\n"
        "float2 pos = UV - 0.5; // 中心对齐\n"
        "\n"
        "// 1. 基础网格\n"
        "float2 gridUV = UV * 40.0;\n"
        "float grid = step(0.97, frac(gridUV.x)) + step(0.97, frac(gridUV.y));\n"
        "float3 color = float3(0.0, 0.04, 0.08) + grid * float3(0.0, 0.4, 0.8) * 0.1;\n"
        "\n"
        "// 2. 旋转搜索灯 (Exposure Point)\n"
        "float angle = RotationPos * 6.28318;\n"
        "float2 searchCenter = float2(cos(angle), sin(angle)) * 0.35;\n"
        "float dist = length(pos - searchCenter);\n"
        "float searchlight = exp(-dist * 12.0); // 使用指数衰减获得更自然的光晕\n"
        "color += searchlight * float3(0.7, 0.9, 1.0) * 2.0; // 强光中心\n"
        "\n"
        "// 3. 扫描线 (使用 iTime 参数)\n"
        "color += sin(UV.y * 150.0 + iTime * 4.0) * 0.02;\n"
        "\n"
        "return float4(color, 1.0);"
    )
    
    bg_engine = get_handle(await mat_api.add_node("Custom", "BG_Engine"))
    await mat_api.set_node_properties(bg_engine, {"OutputType": "CMOT_Float4"})
    # 使用 iTime 代替 Time 以避免 SM6 关键字冲突
    await mat_api.set_hlsl_node_io(bg_engine, bg_hlsl, ["UV", "iTime", "RotationPos"])
    
    tex_coord = get_handle(await mat_api.add_node("TextureCoordinate", "UV"))
    time_node = get_handle(await mat_api.add_node("Time", "Clock"))
    
    await mat_api.connect_pins(tex_coord, "Output", bg_engine, "UV")
    await mat_api.connect_pins(time_node, "Output", bg_engine, "iTime")
    await mat_api.connect_pins(rotation_pos, "Output", bg_engine, "RotationPos")
    await mat_api.connect_pins(bg_engine, "Output", "Output", "FinalColor")
    
    await mat_api.compile_asset()

    # --- PHASE 2: Design Slider Material ---
    print("\n[2] 设计高亮滑条材质...")
    slider_mat_path = "/Game/UMGMCP/Demos/M_CyberSliderBar"
    await mat_api.set_target_material(slider_mat_path)
    
    await mat_api.set_node_properties("Output", {
        "MaterialDomain": "MD_UI",
        "BlendMode": "BLEND_Translucent"
    })
    
    slider_hlsl = (
        "// 高亮发光滑条 (使用 iTime 参数)\n"
        "float3 base = float3(0.0, 0.8, 1.0);\n"
        "float pulse = 0.85 + 0.15 * sin(iTime * 8.0);\n"
        "return float4(base * pulse, 0.95);"
    )
    
    slider_engine = get_handle(await mat_api.add_node("Bar_Logic", "Custom"))
    await mat_api.set_node_properties(slider_engine, {"OutputType": "CMOT_Float4"})
    await mat_api.set_hlsl_node_io(slider_engine, slider_hlsl, ["iTime"])
    
    await mat_api.connect_pins(time_node, "Output", slider_engine, "iTime")
    await mat_api.connect_pins(slider_engine, "Output", "Output", "FinalColor")
    await mat_api.connect_pins(slider_engine, "Output", "Output", "Opacity")
    
    await mat_api.compile_asset()

    # --- PHASE 3: Build UI Layout ---
    print("\n[3] 构建双材质响应式 UMG...")
    widget_path = "/Game/UMGMCP/Demos/W_CyberSearchlight"
    await conn.send_command("create_blueprint", {"package_path": widget_path, "parent_class": "UserWidget"})
    await conn.send_command("set_target_umg_asset", {"asset_path": widget_path})
    
    root_id = "MainOverlay"
    await widget_set.create_widget("Overlay", root_id, "Root")
    
    # 底部背景：使用探照灯材质
    bg_id = "MainBackground"
    await widget_set.create_widget("Image", bg_id, root_id)
    await widget_set.set_widget_properties(bg_id, {
        "Brush": {
            "ResourceObject": bg_mat_path,
            "DrawAs": "Image"
        },
        "Slot": {
            "HorizontalAlignment": "HAlign_Fill",
            "VerticalAlignment": "VAlign_Fill"
        }
    })
    
    # 交互滑条
    slider_id = "RotationController"
    await widget_set.create_widget("Slider", slider_id, root_id)
    
    await widget_set.set_widget_properties(slider_id, {
        "Value": 0.5,
        "WidgetStyle": {
            "NormalThumbImage": { "ResourceObject": slider_mat_path, "ImageSize": {"X": 40, "Y": 40} },
            "HoveredThumbImage": { "ResourceObject": slider_mat_path, "ImageSize": {"X": 45, "Y": 45} }
        },
        "Slot": {
            "Padding": {"Bottom": 100, "Left": 0, "Right": 0, "Top": 0},
            "HorizontalAlignment": "HAlign_Center",
            "VerticalAlignment": "VAlign_Bottom"
        }
    })
    
    print(f"\n[SUCCESS] 演示创建完毕: {widget_path}")
    print("✨ 已解决 SM6 关键字冲突，渲染效率更高，光效更细腻！")

if __name__ == "__main__":
    asyncio.run(run_fancy_slider_demo())
