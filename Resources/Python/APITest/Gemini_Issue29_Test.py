import asyncio
import sys
import os
import json

# Add parent directory to path to import modules
current_dir = os.path.dirname(os.path.abspath(__file__))
parent_dir = os.path.dirname(current_dir)
sys.path.append(parent_dir)

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
            return json.loads(response_data)
        except Exception as e:
            return {"success": False, "error": str(e)}
        finally:
            if writer:
                writer.close()
                await writer.wait_closed()

async def run_issue29_test():
    conn = SimpleConnection()
    asset_path = "/Game/UMGMCP/Demos/W_Issue29_Test"
    
    print(f"--- 1. 准备测试资产: {asset_path} ---")
    resp = await conn.send_command("create_blueprint", {"package_path": asset_path, "parent_class": "UserWidget"})
    if resp.get("status") == "error" and "already exists" in resp.get("error", ""):
        print(f"ℹ️ 资源已存在，将使用现有资源进行测试。")
    elif resp.get("status") == "error":
        print(f"⚠️ 创建资源失败: {resp.get('error')}")
        
    await conn.send_command("set_target_umg_asset", {"asset_path": asset_path})
    
    print("\n--- 2. 准备 UI 结构 ---")
    # 创建 Root Canvas
    await conn.send_command("create_widget", {"widget_type": "CanvasPanel", "new_widget_name": "RootCanvas"})
    # 创建 VerticalBox
    await conn.send_command("create_widget", {"widget_type": "VerticalBox", "new_widget_name": "MyVerticalBox", "parent_name": "RootCanvas"})
    # 创建 VerticalBox 的子 Widget (按钮)
    await conn.send_command("create_widget", {"widget_type": "Button", "new_widget_name": "BoxBtn", "parent_name": "MyVerticalBox"})
    # 创建 CanvasPanel 的子 Widget (按钮，用于测试 Canvas 别名)
    await conn.send_command("create_widget", {"widget_type": "Button", "new_widget_name": "CanvasBtn", "parent_name": "RootCanvas"})
    
    print("\n--- 3. 测试 VerticalBoxSlot: Slot.Size 结构体常规设置 (FSlateChildSize) ---")
    set_params = {
        "widget_name": "BoxBtn",
        "properties": {
            "Slot.Size": {
                "value": 1.0,
                "sizeRule": "Fill"
            }
        }
    }
    set_resp = await conn.send_command("set_widget_properties", set_params)
    print(f"Set Response: {set_resp}")
    
    # 验证查询
    query_resp = await conn.send_command("query_widget_properties", {
        "widget_name": "BoxBtn",
        "properties": ["Slot.Size"]
    })
    print(f"Query BoxBtn Slot.Size: {query_resp}")
    
    size_data = query_resp.get("Slot.Size")
    assert size_data is not None, "❌ 无法查询到 BoxBtn Slot.Size 属性"
    assert size_data.get("sizeRule") == "Fill", f"❌ sizeRule 应该为 'Fill'，但实际为 {size_data.get('sizeRule')}"
    print("✅ BoxSlot 常规结构体设置验证成功！")
    
    print("\n--- 4. 测试 VerticalBoxSlot: Slot.Size 点号级联深度设置 ---")
    set_dot_params = {
        "widget_name": "BoxBtn",
        "properties": {
            "Slot.Size.Value": 2.5,
            "Slot.Size.SizeRule": "Fill"
        }
    }
    set_dot_resp = await conn.send_command("set_widget_properties", set_dot_params)
    print(f"Set Dot Response: {set_dot_resp}")
    
    query_dot_resp = await conn.send_command("query_widget_properties", {
        "widget_name": "BoxBtn",
        "properties": ["Slot.Size"]
    })
    print(f"Query BoxBtn Slot.Size (After Dot Set): {query_dot_resp}")
    
    size_dot_data = query_dot_resp.get("Slot.Size")
    assert size_dot_data is not None, "❌ 无法查询到 BoxBtn Slot.Size 属性"
    assert abs(size_dot_data.get("value") - 2.5) < 1e-4, f"❌ value 应该为 2.5，但实际为 {size_dot_data.get('value')}"
    assert size_dot_data.get("sizeRule") == "Fill", f"❌ sizeRule 应该为 'Fill'，但实际为 {size_dot_data.get('sizeRule')}"
    print("✅ BoxSlot 点号级联设置验证成功！")
    
    print("\n--- 5. 测试 CanvasPanelSlot: Slot.Size 降级数组别名设置 ---")
    set_canvas_params = {
        "widget_name": "CanvasBtn",
        "properties": {
            "Slot.Size": [400.0, 150.0],
            "Slot.Position": [200.0, 100.0]
        }
    }
    set_canvas_resp = await conn.send_command("set_widget_properties", set_canvas_params)
    print(f"Set Canvas Response: {set_canvas_resp}")
    
    query_canvas_resp = await conn.send_command("query_widget_properties", {
        "widget_name": "CanvasBtn",
        "properties": ["Slot.Size", "Slot.Position"]
    })
    print(f"Query CanvasBtn Slot.Size & Position: {query_canvas_resp}")
    
    canvas_size = query_canvas_resp.get("Slot.Size")
    canvas_pos = query_canvas_resp.get("Slot.Position")
    assert canvas_size == [400.0, 150.0], f"❌ Canvas Size 应该为 [400.0, 150.0]，但实际为 {canvas_size}"
    assert canvas_pos == [200.0, 100.0], f"❌ Canvas Position 应该为 [200.0, 100.0]，但实际为 {canvas_pos}"
    print("✅ CanvasPanelSlot 别名数组设置与降级兼容验证成功！")
    
    await conn.send_command("save_asset")
    print("\n🎉 [ALL TESTS PASSED] 智能反射、失败降级、小数点直接访问及 CanvasPanel 别名兼容性均表现完美！")

if __name__ == "__main__":
    asyncio.run(run_issue29_test())
