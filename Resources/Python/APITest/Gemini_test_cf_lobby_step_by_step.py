import asyncio
import logging
import json
from mcp import ClientSession, StdioServerParameters
from mcp.client.stdio import stdio_client

# Configure logging
logging.basicConfig(level=logging.INFO, format='%(name)s - %(levelname)s - %(message)s')
logger = logging.getLogger("test_cf_lobby_steps")

async def run_test():
    server_params = StdioServerParameters(
        command="python",
        args=["UmgMcpServer.py"],
        env=None
    )

    async with stdio_client(server_params) as (read, write):
        async with ClientSession(read, write) as session:
            await session.initialize()
            
            print("\n" + "="*60)
            print("🎮 CF Lobby Step-by-Step Construction Test")
            print("="*60)
            print("⚠️  Please ensure you have a UMG asset open in Unreal Editor.")
            input("👉 Press Enter to start...")

            # --- Step 0: Find Root ---
            logger.info("🔍 Finding root widget...")
            tree_result = await session.call_tool("get_widget_tree", arguments={})
            root_name = None
            if tree_result.content:
                try:
                    data = json.loads(tree_result.content[0].text)
                    if data.get("status") == "success":
                        root = data.get("result", {}).get("data", {})
                        root_name = root.get("widget_name")
                        logger.info(f"✅ Found root: {root_name}")
                except:
                    pass
            
            if not root_name:
                logger.error("❌ Could not find root widget.")
                return

            # --- Step 1: Create Main Layout (VerticalBox) ---
            print(f"\n[Step 1] Creating Main Layout (VerticalBox)...")
            await session.call_tool("create_widget", arguments={
                "parent_name": root_name, "widget_type": "VerticalBox", "new_widget_name": "MainLayout"
            })
            
            # --- Step 2: Create Top Bar (HorizontalBox) ---
            print(f"\n[Step 2] Creating Top Bar...")
            await session.call_tool("create_widget", arguments={
                "parent_name": "MainLayout", "widget_type": "HorizontalBox", "new_widget_name": "TopBar"
            })
            # Add Title
            await session.call_tool("create_widget", arguments={
                "parent_name": "TopBar", "widget_type": "TextBlock", "new_widget_name": "GameTitle"
            })
            await session.call_tool("set_widget_properties", arguments={
                "widget_name": "GameTitle", 
                "properties": {
                    "Text": "CROSSFIRE LOBBY",
                    "Font": {"Size": 24}
                }
            })

            # --- Step 3: Create Character Area (Border/Image) ---
            print(f"\n[Step 3] Creating Character Area...")
            await session.call_tool("create_widget", arguments={
                "parent_name": "MainLayout", "widget_type": "Border", "new_widget_name": "CharacterArea"
            })
            await session.call_tool("set_widget_properties", arguments={
                "widget_name": "CharacterArea", 
                "properties": {
                    "BrushColor": {"R": 0.1, "G": 0.1, "B": 0.1, "A": 1.0},
                    "Padding": {"Left": 20, "Top": 20, "Right": 20, "Bottom": 20}
                }
            })
            # Add Character Placeholder Text
            await session.call_tool("create_widget", arguments={
                "parent_name": "CharacterArea", "widget_type": "TextBlock", "new_widget_name": "CharacterPlaceholder"
            })
            await session.call_tool("set_widget_properties", arguments={
                "widget_name": "CharacterPlaceholder", 
                "properties": {"Text": "[CHARACTER MODEL PREVIEW]", "Justification": "Center"}
            })

            # --- Step 4: Create Bottom Menu (HorizontalBox) ---
            print(f"\n[Step 4] Creating Bottom Menu...")
            await session.call_tool("create_widget", arguments={
                "parent_name": "MainLayout", "widget_type": "HorizontalBox", "new_widget_name": "BottomMenu"
            })
            
            # Add Buttons
            buttons = ["Play", "Inventory", "Shop"]
            for btn_text in buttons:
                btn_name = f"Btn_{btn_text}"
                await session.call_tool("create_widget", arguments={
                    "parent_name": "BottomMenu", "widget_type": "Button", "new_widget_name": btn_name
                })
                # Add Text to Button
                txt_name = f"Txt_{btn_text}"
                await session.call_tool("create_widget", arguments={
                    "parent_name": btn_name, "widget_type": "TextBlock", "new_widget_name": txt_name
                })
                await session.call_tool("set_widget_properties", arguments={
                    "widget_name": txt_name, "properties": {"Text": btn_text}
                })

            # --- Step 5: Save Asset ---
            print(f"\n[Step 5] Saving Asset...")
            res = await session.call_tool("save_asset", arguments={})
            print(f"Save Result: {res.content[0].text}")

            print("\n" + "="*60)
            print("✅ CF Lobby Construction Complete!")
            print("="*60)

if __name__ == "__main__":
    asyncio.run(run_test())
