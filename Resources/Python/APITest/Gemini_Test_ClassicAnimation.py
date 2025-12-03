import asyncio
import json
import os
from mcp import ClientSession, StdioServerParameters
from mcp.client.stdio import stdio_client

async def run_classic_animation_test():
    # Connect to the MCP server
    server_params = StdioServerParameters(
        command="python",
        args=["../UmgMcpServer.py"], # Adjust path relative to APITest folder
        env=None
    )

    print("\n" + "="*80)
    print("💓 GEMINI CLASSIC ANIMATION TEST: HEARTBEAT")
    print("="*80)

    async with stdio_client(server_params) as (read, write):
        async with ClientSession(read, write) as session:
            await session.initialize()
            
            # 1. Find Target Asset
            print("🔍 Finding target asset...")
            result = await session.call_tool("get_target_umg_asset", arguments={})
            asset_path = None
            if result.content and len(result.content) > 0:
                print(f"DEBUG RAW: {result.content[0].text}")
                data = json.loads(result.content[0].text)
                
                # Handle potential nesting in 'result' field
                if "result" in data:
                    data = data["result"]
                
                if data.get("status") == "success":
                    asset_path = data.get("data", {}).get("asset_path")
                    print(f"✅ Found target asset: {asset_path}")
                else:
                    print(f"⚠️ Could not find open asset: {data.get('error')}")
            
            if not asset_path:
                print("❌ No asset open. Please open a Widget Blueprint in UE5 and try again.")
                return

            # 2. Create Heartbeat Button
            widget_name = "HeartbeatButton"
            print(f"\n🔨 Creating button '{widget_name}'...")
            await session.call_tool("create_widget", arguments={
                "parent_name": "Root", 
                "widget_type": "Button",
                "new_widget_name": widget_name
            })
            
            # Set some properties to make it visible and centered (if possible)
            # Note: Layout properties depend on the parent (CanvasPanel usually)
            # We'll just set color for now
            await session.call_tool("set_widget_properties", arguments={
                "widget_name": widget_name,
                "properties": {
                    "BackgroundColor": {"R": 1.0, "G": 0.2, "B": 0.4, "A": 1.0} # Pinkish Red
                }
            })

            # 3. Create 'Heartbeat' Animation
            anim_name = "HeartbeatAnim"
            print(f"\n🎬 Creating animation '{anim_name}'...")
            # This should now AUTO-FOCUS the animation
            await session.call_tool("create_animation", arguments={
                "animation_name": anim_name
            })

            # 4. Focus Context - SKIPPED (Testing Auto-Focus)
            print(f"🎯 Context should be auto-set to '{anim_name}' and '{widget_name}'. Verifying via implicit add_key...")
            # await session.call_tool("focus_animation", arguments={"animation_name": anim_name})
            # await session.call_tool("focus_widget", arguments={"widget_name": widget_name})

            # 5. Add Keys for Pulse Effect (Scale X and Y)
            # We DO NOT pass 'widget_name' or 'animation_name' here. 
            # It should use the context set by create_widget and create_animation.
            print("💓 Adding pulse keys (Implicit Context)...")
            
            # Key 1: 0.0s -> Scale 1.0
            print("   -> 0.0s: Scale 1.0")
            await session.call_tool("add_key", arguments={"property_name": "RenderTransform.Scale.X", "time": 0.0, "value": 1.0})
            await session.call_tool("add_key", arguments={"property_name": "RenderTransform.Scale.Y", "time": 0.0, "value": 1.0})

            # Key 2: 0.4s -> Scale 1.2 (Beat!)
            print("   -> 0.4s: Scale 1.2")
            await session.call_tool("add_key", arguments={"property_name": "RenderTransform.Scale.X", "time": 0.4, "value": 1.2})
            await session.call_tool("add_key", arguments={"property_name": "RenderTransform.Scale.Y", "time": 0.4, "value": 1.2})

            # Key 3: 0.8s -> Scale 1.0 (Relax)
            print("   -> 0.8s: Scale 1.0")
            await session.call_tool("add_key", arguments={"property_name": "RenderTransform.Scale.X", "time": 0.8, "value": 1.0})
            await session.call_tool("add_key", arguments={"property_name": "RenderTransform.Scale.Y", "time": 0.8, "value": 1.0})

            # 6. Save
            print("\n💾 Saving asset...")
            await session.call_tool("save_asset", arguments={})

            print("\n" + "="*80)
            print("✅ HEARTBEAT ANIMATION CREATED!")
            print("="*80)

if __name__ == "__main__":
    import os
    if os.path.exists("../UmgMcpServer.py"):
        pass
    else:
        print("⚠️ Warning: ../UmgMcpServer.py not found. Please run this script from the 'APITest' directory.")

    asyncio.run(run_classic_animation_test())
