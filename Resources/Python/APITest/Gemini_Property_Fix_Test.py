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

async def run_property_fix_test():
    conn = SimpleConnection()
    asset_path = "/Game/UMGMCP/Demos/W_PropertyFixTest"
    
    print(f"--- 1. 准备测试资源: {asset_path} ---")
    # 尝试创建资源，如果已存在则忽略错误 (C++ 端现在会返回错误，但我们可以继续)
    resp = await conn.send_command("create_blueprint", {"package_path": asset_path, "parent_class": "UserWidget"})
    if resp.get("status") == "error" and "already exists" in resp.get("error", ""):
        print(f"ℹ️ 资源已存在，将使用现有资源进行测试。")
    elif resp.get("status") == "error":
        print(f"⚠️ 创建资源失败: {resp.get('error')} (这可能是正常的，如果该命令不支持 UserWidget 路径)")
    
    await conn.send_command("set_target_umg_asset", {"asset_path": asset_path})
    
    print("\n--- 2. 准备 UI 结构 ---")
    # create_widget 现在在 C++ 端增加了查重逻辑，如果已存在则直接返回，不会重复创建导致崩溃
    await conn.send_command("create_widget", {"widget_type": "CanvasPanel", "new_widget_name": "RootCanvas"})
    await conn.send_command("create_widget", {"widget_type": "Button", "new_widget_name": "TestBtn", "parent_name": "RootCanvas"})
    
    print("\n--- 3. 测试 set_widget_properties (设置初始位置) ---")
    set_params = {
        "widget_name": "TestBtn",
        "properties": {
            "Slot.Position": [500, 300],
            "Slot.LayoutData.Anchors": {
                "Minimum": [0.5, 0.5],
                "Maximum": [0.5, 0.5]
            },
            "Slot.Alignment": [0.5, 0.5]
        }
    }
    await conn.send_command("set_widget_properties", set_params)
    print("✅ 初始位置已设置 (Slot.Position=[500, 300])")
    
    input("\n[PAUSE] 请在 UE 编辑器中检查按钮位置。按下回车后将修改按钮大小...")

    print("\n--- 4. 修改按钮大小 (测试 Slot.Size 别名) ---")
    await conn.send_command("set_widget_properties", {
        "widget_name": "TestBtn",
        "properties": {
            "Slot.Size": [400, 100]
        }
    })
    print("✅ 按钮大小已修改 (Slot.Size=[400, 100])")
    
    await conn.send_command("save_asset")

    print("\n--- 5. 验证 query_widget_properties ---")
    query_params = {
        "widget_name": "TestBtn",
        "properties": ["Slot.Position", "Slot.Size", "Slot.Alignment"]
    }
    query_resp = await conn.send_command("query_widget_properties", query_params)
    print(f"Query Response: {json.dumps(query_resp, indent=2, ensure_ascii=False)}")
    
    if query_resp.get("success") == False:
        print(f"❌ 查询失败: {query_resp.get('error')}")
    else:
        pos = query_resp.get("Slot.Position")
        size = query_resp.get("Slot.Size")
        print(f"\n结果验证:")
        print(f"  Position: {pos}")
        print(f"  Size:     {size}")
        
        if pos == [500.0, 300.0] and size == [400.0, 100.0]:
            print("\n✅ [SUCCESS] 别名映射与点号路径解析验证通过！")
        else:
            print("\n⚠️ [WARNING] 返回数据与预期不完全匹配 (可能是由于浮点数精度或 Slot 类型问题)。")

if __name__ == "__main__":
    asyncio.run(run_property_fix_test())

if __name__ == "__main__":
    asyncio.run(run_property_fix_test())
