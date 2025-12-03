import asyncio
import json
from mcp import ClientSession, StdioServerParameters
from mcp.client.stdio import stdio_client

async def run_sequencer_test():
    # Connect to the MCP server
    server_params = StdioServerParameters(
        command="python",
        args=["../UmgMcpServer.py"], # Adjust path relative to APITest folder
        env=None
    )

    print("\n" + "="*80)
    print("🎬 GEMINI SEQUENCER API TEST")
    print("="*80)

    async with stdio_client(server_params) as (read, write):
        async with ClientSession(read, write) as session:
            await session.initialize()
            
            # 1. Set Target Asset (Use the currently open one or a specific one)
            # For this test, we'll try to get the currently open asset first.
            print("🔍 Finding target asset...")
            result = await session.call_tool("get_target_umg_asset", arguments={})
            asset_path = None
            if result.content and len(result.content) > 0:
                data = json.loads(result.content[0].text)
                if data.get("status") == "success":
                    asset_path = data.get("data", {}).get("asset_path")
                    print(f"✅ Found target asset: {asset_path}")
                else:
                    print(f"⚠️ Could not find open asset: {data.get('error')}")
            
            if not asset_path:
                print("❌ No asset open. Please open a Widget Blueprint in UE5 and try again.")
                return

            # 2. Create a Test Widget to Animate
            print("\n🔨 Creating test widget 'AnimTestWidget'...")
            await session.call_tool("create_widget", arguments={
                "parent_name": "Root", # Assuming Root exists, if not this might fail but that's part of the test
                "widget_type": "CanvasPanel",
                "new_widget_name": "AnimTestWidget"
            })
            
            # 3. Create Animation
            anim_name = "TestFadeAnim"
            print(f"\n🎬 Creating animation '{anim_name}'...")
            await session.call_tool("create_animation", arguments={
                "animation_name": anim_name
            })

            # 4. Focus Context
            print(f"🎯 Focusing animation '{anim_name}' and widget 'AnimTestWidget'...")
            await session.call_tool("focus_animation", arguments={"animation_name": anim_name})
            await session.call_tool("focus_widget", arguments={"widget_name": "AnimTestWidget"})

            # 5. Add Track & Keys (Fade In)
            print("🔑 Adding keys for RenderOpacity...")
            
            # Key 1: Time 0.0, Value 0.0
            print("   -> Key at 0.0s: 0.0")
            await session.call_tool("add_key", arguments={
                "property_name": "RenderOpacity",
                "time": 0.0,
                "value": 0.0
            })

            # Key 2: Time 1.0, Value 1.0
            print("   -> Key at 1.0s: 1.0")
            await session.call_tool("add_key", arguments={
                "property_name": "RenderOpacity",
                "time": 1.0,
                "value": 1.0
            })

            # 6. Save
            print("\n💾 Saving asset...")
            await session.call_tool("save_asset", arguments={})

            print("\n" + "="*80)
            print("✅ SEQUENCER TEST COMPLETE!")
            print("="*80)

if __name__ == "__main__":
    # Adjust working directory if needed or run from the correct folder
    import os
    # Ensure we can find UmgMcpServer.py
    # If running from APITest, parent is Resources/Python
    if os.path.exists("../UmgMcpServer.py"):
        pass
    else:
        print("⚠️ Warning: ../UmgMcpServer.py not found. Please run this script from the 'APITest' directory.")

    asyncio.run(run_sequencer_test())
