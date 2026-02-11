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
    print("=== 启动 'Cyberpunk Slider' 顶级演示 ===")
    
    conn = SimpleConnection()
    mat_api = UMGMaterial(conn)
    widget_set = UMGSet(conn)
    
    # --- PHASE 1: Design Material ---
    print("\n[1] 设计顶级渲染材质 (Cyberpunk Style)...")
    material_path = "/Game/UMGMCP/Demos/M_Cyberpunk_Slider"
    await mat_api.set_target_material(material_path)
    
    # 配置材质域为 UI，混合模式为半透明 (MD_UI, BLEND_Translucent)
    await mat_api.set_node_properties("Output", {
        "MaterialDomain": "MD_UI",
        "BlendMode": "BLEND_Translucent"
    })
    
    # 定义交互变量
    progress = get_handle(await mat_api.define_variable("Progress", "Scalar"))
    hover_pos = get_handle(await mat_api.define_variable("HoverPos", "Vector"))
    
    # 核心 HLSL 逻辑
    hlsl_code = (
        "// Inputs: UV, Time, Progress, HoverPos\n"
        "float2 pos = UV;\n"
        "\n"
        "// 1. 赛博网格底纹\n"
        "float2 gridUV = pos * 25.0;\n"
        "float grid = step(0.96, frac(gridUV.x)) + step(0.96, frac(gridUV.y));\n"
        "float3 bgColor = float3(0.0, 0.02, 0.08) + grid * float3(0.0, 0.4, 0.8) * 0.15;\n"
        "\n"
        "// 2. 扫描线与动态干扰\n"
        "float scanline = sin(pos.y * 150.0 + Time * 3.0) * 0.02;\n"
        "float noise = frac(sin(dot(pos.xy ,float2(12.9898,78.233))) * 43758.5453) * 0.05;\n"
        "bgColor += scanline + noise;\n"
        "\n"
        "// 3. 进度感知色彩偏置 (Cold Blue -> Intense Red)\n"
        "float3 cold = float3(0.0, 0.6, 1.0);\n"
        "float3 hot = float3(1.0, 0.1, 0.1);\n"
        "float3 accentColor = lerp(cold, hot, Progress);\n"
        "\n"
        "// 4. 背景板交互光晕 (根据 HoverPos 响应)\n"
        "float dist = length(pos - HoverPos.xy);\n"
        "float glow = 1.0 - smoothstep(0.0, 0.4, dist);\n"
        "float3 interactionColor = accentColor * glow * 1.2;\n"
        "\n"
        "// 5. 进度条视觉化 (在背景板中心位置显示)\n"
        "float barS = 0.48; float barE = 0.52;\n"
        "float mask = step(barS, pos.y) * step(pos.y, barE);\n"
        "float fill = step(pos.x, Progress) * mask;\n"
        "float3 barColor = accentColor * fill * (0.8 + 0.2 * sin(Time * 5.0));\n"
        "\n"
        "// 6. 最终合成\n"
        "float3 finalRGB = bgColor + barColor + interactionColor;\n"
        "float finalAlpha = 0.85 + 0.1 * sin(Time * 2.0);\n"
        "\n"
        "return float4(finalRGB, finalAlpha);"
    )
    
    custom_node = get_handle(await mat_api.add_node("Custom", "VisualEngine"))
    # 设置输出类型为 Float4，否则会导致 Mask 报错 (Not enough components)
    await mat_api.set_node_properties(custom_node, {"OutputType": "CMOT_Float4"})
    await mat_api.set_hlsl_node_io(custom_node, hlsl_code, ["UV", "Time", "Progress", "HoverPos"])
    
    # 连接环境节点
    tex_coord = get_handle(await mat_api.add_node("TextureCoordinate", "UV"))
    time_node = get_handle(await mat_api.add_node("Time", "Clock"))
    
    await mat_api.connect_pins(tex_coord, "Output", custom_node, "UV")
    await mat_api.connect_pins(time_node, "Output", custom_node, "Time")
    await mat_api.connect_pins(progress, "Output", custom_node, "Progress")
    await mat_api.connect_pins(hover_pos, "Output", custom_node, "HoverPos")
    
    # 核心组件：分离 RGB (颜色) 与 Alpha (透明度) 以达到最佳专业度
    mask_rgb = get_handle(await mat_api.add_node("ComponentMask", "RGB_Mask"))
    await mat_api.set_node_properties(mask_rgb, {"R": True, "G": True, "B": True, "A": False})
    
    mask_a = get_handle(await mat_api.add_node("ComponentMask", "Alpha_Mask"))
    await mat_api.set_node_properties(mask_a, {"R": False, "G": False, "B": False, "A": True})
    
    # 连接 Custom 到 Mask
    await mat_api.connect_pins(custom_node, "Output", mask_rgb, "Input")
    await mat_api.connect_pins(custom_node, "Output", mask_a, "Input")
    
    # 战略节点连接：使用 "Output" 别名代表最终输出节点
    print("[Wiring] 自动连接至材质导出节点 (FinalColor & Opacity)...")
    await mat_api.connect_pins(mask_rgb, "Output", "Output", " FinalColor")  # 映射到 EmissiveColor
    await mat_api.connect_pins(mask_a, "Output", "Output", "Opacity")        # 映射到 Opacity
    
    await mat_api.compile_asset()
    print("--- 材质创建与编译完成 ---")

    # --- PHASE 2: Create UI Layout ---
    print("\n[2] 构建全屏 UI 布局...")
    widget_path = "/Game/UMGMCP/Demos/W_CyberSlider"
    await conn.send_command("create_blueprint", {"package_path": widget_path, "parent_class": "UserWidget"})
    await conn.send_command("set_target_umg_asset", {"asset_path": widget_path})
    
    root_id = "MainCanvas"
    await widget_set.create_widget("CanvasPanel", root_id, "Root")
    
    # 背景板：全屏填充 (Anchors: 0,0 to 1,1)
    bg_id = "BackgroundBoard"
    await widget_set.create_widget("Image", bg_id, root_id)
    await widget_set.set_widget_properties(bg_id, {
        "Brush": {
            "ResourceObject": material_path,
            "DrawAs": "Image"
        },
        "Slot": {
            "Anchors": {"Minimum": {"X": 0, "Y": 0}, "Maximum": {"X": 1, "Y": 1}},
            "Offsets": {"Left": 0, "Top": 0, "Right": 0, "Bottom": 0}
        }
    })
    
    # 滑条：底部居中
    slider_id = "InteractiveSlider"
    await widget_set.create_widget("Slider", slider_id, root_id)
    await widget_set.set_widget_properties(slider_id, {
        "Value": 0.5,
        "Slot": {
            "Anchors": {"Minimum": {"X": 0.5, "Y": 0.9}, "Maximum": {"X": 0.5, "Y": 0.9}},
            "Alignment": {"X": 0.5, "Y": 0.5},
            "Offsets": {"Left": 0, "Top": 0, "Right": 600, "Bottom": 60}
        }
    })
    
    print(f"\n[SUCCESS] 演示创建完毕: {widget_path}")
    print("提示：在编辑器中调整 M_Cyberpunk_Slider 的 Progress 参数，即可看到全屏背景板的色彩切换、网格效果与交互光晕！")

if __name__ == "__main__":
    asyncio.run(run_fancy_slider_demo())
