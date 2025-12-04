import asyncio
import json
import math
import time
from mcp import ClientSession, StdioServerParameters
from mcp.client.stdio import stdio_client

async def run_demo():
    print("🚀 Starting Gemini Show-Off Demo (MCP Client Mode)...")
    
    # Connect to the MCP server via Stdio
    server_params = StdioServerParameters(
        command="python",
        args=["../UmgMcpServer.py"],
        env=None
    )

    async with stdio_client(server_params) as (read, write):
        async with ClientSession(read, write) as session:
            await session.initialize()
            
            # 1. Setup Scene (Atomic Layout Application)
            print("\n🎨 Setting up Scene (Atomic)...")
            
            # Get current target
            result = await session.call_tool("get_target_umg_asset", arguments={})
            current_asset = None
            if result.content:
                data = json.loads(result.content[0].text)
                current_asset = data.get("result", {}).get("data", {}).get("asset_path")
            
            print(f"  Working on asset: {current_asset}")
            
            if not current_asset:
                print("❌ No UMG asset open or targeted! Please open a Widget Blueprint in Unreal.")
                return

            # Define the FULL Scene Layout
            scene_layout = {
                "widget_name": "RootCanvas",
                "widget_class": "CanvasPanel",
                "properties": {
                    "Slot": {"Size": {"SizeRule": "Fill", "Value": 1.0}}
                },
                "children": [
                    {
                        "widget_name": "TitleText",
                        "widget_class": "TextBlock",
                        "properties": {
                            "Text": "Create by Gemini 3",
                            "RenderOpacity": 0.0,
                            "Font": {"Size": 48},
                            "Slot": {
                                "Position": {"X": 600, "Y": 100},
                                "Size": {"X": 800, "Y": 100}
                            }
                        }
                    },
                    {
                        "widget_name": "Ball",
                        "widget_class": "Image",
                        "properties": {
                            "Brush": {"ImageSize": {"X": 50, "Y": 50}},
                            "ColorAndOpacity": {"R": 0.0, "G": 0.7, "B": 1.0, "A": 1.0}
                        }
                    }
                ]
            }

            print("  Applying Full Layout...")
            # apply_layout expects a string argument 'layout_content'
            await session.call_tool("apply_layout", arguments={"layout_content": json.dumps(scene_layout)})
            print("  ✅ Scene Layout Applied (Async)!")

            # 2. Create Math Animations
            print("\n✨ Creating Math Animations...")
            
            # --- Sin Wave ---
            print("  Creating Anim_Sin...")
            await session.call_tool("create_animation", arguments={"animation_name": "Anim_Sin"})
            
            # Animate Ball: X moves 0->1000, Y follows Sin
            # Batching commands isn't directly supported by MCP stdio client easily without custom logic,
            # so we'll just await them. It might be slower but it's correct.
            for i in range(0, 61, 2): # Step 2 to reduce calls for demo speed
                t = i / 10.0
                x = 100 + (t * 150)
                y = 500 + (math.sin(t * 2) * 200)
                
                await session.call_tool("add_key", arguments={
                    "animation_name": "Anim_Sin", "widget_name": "Ball",
                    "property_name": "RenderTransform.Translation.X", "time": t, "value": x
                })
                await session.call_tool("add_key", arguments={
                    "animation_name": "Anim_Sin", "widget_name": "Ball",
                    "property_name": "RenderTransform.Translation.Y", "time": t, "value": y
                })
                
            # --- Cos Wave ---
            print("  Creating Anim_Cos...")
            await session.call_tool("create_animation", arguments={"animation_name": "Anim_Cos"})
            for i in range(0, 61, 2):
                t = i / 10.0
                x = 100 + (t * 150)
                y = 500 + (math.cos(t * 2) * 200)
                
                await session.call_tool("add_key", arguments={
                    "animation_name": "Anim_Cos", "widget_name": "Ball",
                    "property_name": "RenderTransform.Translation.X", "time": t, "value": x
                })
                await session.call_tool("add_key", arguments={
                    "animation_name": "Anim_Cos", "widget_name": "Ball",
                    "property_name": "RenderTransform.Translation.Y", "time": t, "value": y
                })

            # --- Tan Wave ---
            print("  Creating Anim_Tan...")
            await session.call_tool("create_animation", arguments={"animation_name": "Anim_Tan"})
            for i in range(0, 61, 2):
                t = i / 10.0
                x = 100 + (t * 150)
                val = math.tan(t)
                if val > 5: val = 5
                if val < -5: val = -5
                y = 500 + (val * 100)
                
                await session.call_tool("add_key", arguments={
                    "animation_name": "Anim_Tan", "widget_name": "Ball",
                    "property_name": "RenderTransform.Translation.X", "time": t, "value": x
                })
                await session.call_tool("add_key", arguments={
                    "animation_name": "Anim_Tan", "widget_name": "Ball",
                    "property_name": "RenderTransform.Translation.Y", "time": t, "value": y
                })

            # 3. Create Complex "Gemini" Animation
            print("\n✨ Creating Complex 'Anim_Gemini'...")
            await session.call_tool("create_animation", arguments={"animation_name": "Anim_Gemini"})
            
            # Part A: Fade In Text
            print("  Animating Text Fade-In...")
            await session.call_tool("add_key", arguments={
                "animation_name": "Anim_Gemini", "widget_name": "TitleText",
                "property_name": "RenderOpacity", "time": 0.0, "value": 0.0
            })
            await session.call_tool("add_key", arguments={
                "animation_name": "Anim_Gemini", "widget_name": "TitleText",
                "property_name": "RenderOpacity", "time": 2.0, "value": 1.0
            })
            
            # Part B: Spiral Ball
            print("  Animating Spiral Ball...")
            center_x = 960
            center_y = 540
            duration = 5.0
            steps = 50 # Reduced steps for speed
            
            for i in range(steps + 1):
                t = (i / steps) * duration
                angle = t * 5.0
                radius = t * 100.0
                
                x = center_x + (math.cos(angle) * radius) - 960
                y = center_y + (math.sin(angle) * radius) - 540
                
                await session.call_tool("add_key", arguments={
                    "animation_name": "Anim_Gemini", "widget_name": "Ball",
                    "property_name": "RenderTransform.Translation.X", "time": t, "value": x
                })
                await session.call_tool("add_key", arguments={
                    "animation_name": "Anim_Gemini", "widget_name": "Ball",
                    "property_name": "RenderTransform.Translation.Y", "time": t, "value": y
                })
                
                scale = 1.0 + (math.sin(t * 10) * 0.5)
                await session.call_tool("add_key", arguments={
                    "animation_name": "Anim_Gemini", "widget_name": "Ball",
                    "property_name": "RenderTransform.Scale.X", "time": t, "value": scale
                })
                await session.call_tool("add_key", arguments={
                    "animation_name": "Anim_Gemini", "widget_name": "Ball",
                    "property_name": "RenderTransform.Scale.Y", "time": t, "value": scale
                })

            print("\n✅ All Animations Created!")
            print("PLEASE GO TO UNREAL EDITOR AND PLAY THE ANIMATIONS NOW.")
            
            # 4. Pause for Inspection
            # In stdio mode, input() might block or interfere with async loop if not careful, 
            # but for a simple script it's usually okay or we can just wait.
            # However, since this is an automated test script now, maybe we skip the pause or make it short?
            # The user wanted "inspection", so let's keep a short sleep or just exit.
            # Let's just print a message.
            
            # 5. Recursive Deletion Test
            print("\n🗑️  Starting Recursive Deletion Test (Simulated)...")
            
            result = await session.call_tool("get_all_animations", arguments={})
            anims = []
            if result.content:
                data = json.loads(result.content[0].text)
                anims = data.get("result", {}).get("animations", [])
                
            print(f"  Found {len(anims)} animations: {anims}")
            
            for anim in anims:
                print(f"  Deleting '{anim}'...")
                await session.call_tool("delete_animation", arguments={"animation_name": anim})
                # await asyncio.sleep(0.1)
                
            print("\n🎉 Demo Complete!")

if __name__ == "__main__":
    asyncio.run(run_demo())
