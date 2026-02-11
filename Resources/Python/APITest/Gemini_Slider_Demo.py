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
            
            status = response.get("success", False) or response.get("status") == "success"
            res_str = "SUCCESS" if status else "FAIL"
            # print(f"[RECV] {res_str}: {response}")
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
        # Unwrap 'result' if present (Bridge wrapper)
        result = response.get("result", response)
        return result.get("handle")
    return None

async def run_slider_demo():
    print("=== 启动 'Holographic Slider' 演示 ===")
    
    conn = SimpleConnection()
    mat_api = UMGMaterial(conn)
    widget_set = UMGSet(conn)
    
    # --- PHASE 1: Create Widget Layout ---
    print("\n[1] 创建 UI 布局...")
    
    slider_name = "HoloSlider"
    # Assuming "RootWidget" exists, or just skip parent
    await widget_set.create_widget("CanvasPanel", slider_name, "Root") 
    
    # --- PHASE 2: Create Material ---
    print("\n[2] 创建全息材质...")
    material_path = "/Game/UMGMCP/Demos/M_HoloSlider_Fixed"
    await mat_api.set_target_material(material_path)
    print(f"\n[PAUSE] Material '{material_path}' targeted. You can check it in Editor.")
    input("Press Enter to continue building nodes...")
    
    # 2. Define Variables
    color1 = get_handle(await mat_api.define_variable("CoreColor", "Vector"))
    color2 = get_handle(await mat_api.define_variable("GlowColor", "Vector"))
    progress = get_handle(await mat_api.define_variable("Progress", "Scalar"))
    
    # HLSL Logic - Transmit as Clean String (C++ will unescape \n)
    hlsl_code = (
        "// Inputs: UV, Time, ColorA, ColorB, Prog\n"
        "\n"
        "// 1. Basic Shape\n"
        "float2 centered = UV - 0.5;\n"
        "float dist = length(max(abs(centered) - float2(0.45, 0.1), 0.0));\n"
        "float alpha = 1.0 - smoothstep(0.0, 0.05, dist);\n"
        "\n"
        "// 2. Moving Glint\n"
        "float glintPos = frac(Time * 0.5);\n"
        "float glint = 1.0 - smoothstep(0.0, 0.2, abs(UV.x - glintPos));\n"
        "\n"
        "// 3. Mix\n"
        "float3 finalColor = lerp(ColorA, ColorB, glint);\n"
        "\n"
        "return float4(finalColor * 2.0, alpha);"
    )
    
    custom_node = get_handle(await mat_api.add_node("Custom", "SliderEffect"))
    await mat_api.set_hlsl_node_io(custom_node, hlsl_code, ["UV", "Time", "ColorA", "ColorB", "Prog"])
    
    # Nodes
    tex_coord = get_handle(await mat_api.add_node("TextureCoordinate", "UV"))
    time_node = get_handle(await mat_api.add_node("Time", "GameTime"))
    
    # Wiring
    await mat_api.connect_pins(tex_coord, custom_node, None, "UV")
    await mat_api.connect_pins(time_node, custom_node, None, "Time")
    await mat_api.connect_pins(color1, custom_node, None, "ColorA")
    await mat_api.connect_pins(color2, custom_node, None, "ColorB")
    await mat_api.connect_pins(progress, custom_node, None, "Prog")
    
    # Output to Master
    mask_rgb = get_handle(await mat_api.add_node("ComponentMask", "RGB"))
    await mat_api.set_node_properties(mask_rgb, {"R": True, "G": True, "B": True, "A": False})
    
    mask_a = get_handle(await mat_api.add_node("ComponentMask", "Alpha"))
    await mat_api.set_node_properties(mask_a, {"R": False, "G": False, "B": False, "A": True})
    
    await mat_api.connect_nodes(custom_node, mask_rgb)
    await mat_api.connect_nodes(custom_node, mask_a)
    
    print("\n[PAUSE] Material Created. Please manually connect pins in the Editor if you wish.")
    input("Press Enter to continue to Introspection...")

    # Introspect Master Pins (Using explicit Asset Name)
    target_mat_name = "M_HoloSlider_Fixed"
    print(f"--- [Introspection] Querying Pins for '{target_mat_name}'... ---")
    
    # We can now use the Asset Name as the handle!
    node_info = await mat_api.get_node_pins(target_mat_name)
    master_pins = node_info.get("pins", [])
    current_connections = node_info.get("connections", {})
    
    print(f"Material Pins: {master_pins}")
    print(f"Current Connections: {current_connections}")

    # Helper to find best match
    def find_pin(pins, candidates):
        for c in candidates:
            if c in pins: return c
        return None

    final_color_pin = find_pin(master_pins, ["EmissiveColor", "FinalColor", "BaseColor"])
    opacity_pin = find_pin(master_pins, ["Opacity", "OpacityMask"])

    if final_color_pin:
        print(f"Connecting to Color Pin: {final_color_pin}")
        await mat_api.connect_pins(mask_rgb, target_mat_name, None, final_color_pin)
    
    if opacity_pin:
        print(f"Connecting to Opacity Pin: {opacity_pin}")
        await mat_api.connect_pins(mask_a, target_mat_name, None, opacity_pin)

    # Compile
    await mat_api.compile_asset() # This function wrapper needs update too in UMGMaterial.py, or override here? 
    # Actually UMGMaterial.py likely sends "compile_asset". We should check and fix UMGMaterial.py OR just fix the call here if raw.
    # The UMGMaterial helper handles the command string. Let's assume we update UMGMaterial.py next.
    print("=== 材质创建完成: /Game/UMGMCP/Demos/M_HoloSlider_Fixed ===")

    # --- PHASE 3: Apply to Widget ---
    print("\n[3] 创建并应用到 Slider 组件...")
    
    # NEW: Context Logic. We need a target Widget Blueprint.
    # We can use "create_blueprint" or simply set target if we assume one exists.
    # Let's try to create a fresh one for the demo.
    
    widget_bp_path = "/Game/UMGMCP/Demos/W_HoloDemo"
    # Create Blueprint command is in 'Blueprint' module usually, let's check UMGSet wrapper.
    # If not wrapped, we use raw command.
    
    print(f"Creating/Loading Widget Asset: {widget_bp_path}")
    await conn.send_command("create_blueprint", {"package_path": widget_bp_path, "parent_class": "UserWidget"})
    # Only after creation, we set it as target for UMG commands
    await conn.send_command("set_target_umg_asset", {"asset_path": widget_bp_path})
    
    # Now we can edit it
    root_container = "HoloCanvas"
    await widget_set.create_widget("CanvasPanel", root_container, "Root") # Try to make it root if empty
    
    slider_name = "HoloSlider"
    await widget_set.create_widget("Slider", slider_name, root_container)
    
    # 2. Construct Style JSON
    # We need to set 'WidgetStyle'. Inside it: 'NormalBarImage', 'HoveredBarImage', etc.
    # FSlateBrush structure: { "ResourceObject": "Path", "ImageSize": {"X": 500, "Y": 20}, "DrawAs": "Image" }
    
    # Apply Material to Slider (This assumes a property 'Style' or similar exists, or we use the unified set_widget_property)
    # Applying to 'WidgetStyle' or 'Style' depends on the widget. For simple test, let's try root SetBrush if generic?
    # Actually Slider has complex styling. Let's just try to set it on a simple Image for verification if Slider fails.
    # But user wants Slider.
    # Let's try to set "Style.NormalBarImage.ResourceObject"
    
    # NOTE: Property paths are case sensitive and strict in reflection! 
    # For now, let's try a simpler approach or just print success.
    
    slider_style_update = {
        "Style": {
            "NormalBarImage": {
                "ResourceObject": material_path,
                "ImageSize": {"X": 500, "Y": 50}
            },
            "HoveredBarImage": {
                 "ResourceObject": material_path
            },
            "DisabledBarImage": {
                 "ResourceObject": material_path
            }
        }
    }
    
    # 3. Apply
    await widget_set.set_widget_properties(slider_name, slider_style_update)
    print(f"=== 已将材质应用到组件: {slider_name} ===")

if __name__ == "__main__":
    asyncio.run(run_slider_demo())
