import asyncio
import logging
import json
from mcp import ClientSession, StdioServerParameters
from mcp.client.stdio import stdio_client

# Configure logging
logging.basicConfig(level=logging.INFO, format='%(name)s - %(levelname)s - %(message)s')
logger = logging.getLogger("test_cf_lobby_json")

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
            print("📄 CF Lobby JSON Application Test")
            print("="*60)
            print("⚠️  Please ensure you have a UMG asset open in Unreal Editor.")
            input("👉 Press Enter to start...")

            # Define the CF Lobby Layout JSON
            lobby_layout = {
                "widget_name": "RootCanvas",
                "widget_class": "/Script/UMG.CanvasPanel",
                "children": [
                    {
                        "widget_name": "MainVerticalBox",
                        "widget_class": "/Script/UMG.VerticalBox",
                        "properties": {
                            "Slot": {
                                "LayoutData": {
                                    "Offsets": {"Left": 0, "Top": 0, "Right": 0, "Bottom": 0},
                                    "Anchors": {"Minimum": {"X": 0, "Y": 0}, "Maximum": {"X": 1, "Y": 1}}
                                }
                            }
                        },
                        "children": [
                            # Top Bar
                            {
                                "widget_name": "TopBar",
                                "widget_class": "/Script/UMG.HorizontalBox",
                                "children": [
                                    {
                                        "widget_name": "GameTitle",
                                        "widget_class": "/Script/UMG.TextBlock",
                                        "properties": {
                                            "Text": "CROSSFIRE LOBBY (JSON)",
                                            "Font": {"Size": 32},
                                            "ColorAndOpacity": {"SpecifiedColor": {"R": 1, "G": 0.8, "B": 0, "A": 1}}
                                        }
                                    }
                                ]
                            },
                            # Character Area
                            {
                                "widget_name": "CharacterView",
                                "widget_class": "/Script/UMG.Border",
                                "properties": {
                                    "BrushColor": {"R": 0.2, "G": 0.2, "B": 0.25, "A": 1},
                                    "Padding": {"Left": 50, "Top": 50, "Right": 50, "Bottom": 50},
                                    "Slot": {
                                        "Size": {"SizeRule": "Fill"}
                                    }
                                },
                                "children": [
                                    {
                                        "widget_name": "CharPreviewText",
                                        "widget_class": "/Script/UMG.TextBlock",
                                        "properties": {
                                            "Text": "CHARACTER MODEL HERE",
                                            "Justification": "Center",
                                            "Font": {"Size": 48}
                                        }
                                    }
                                ]
                            },
                            # Bottom Menu
                            {
                                "widget_name": "BottomMenu",
                                "widget_class": "/Script/UMG.HorizontalBox",
                                "children": [
                                    {
                                        "widget_name": "PlayBtn",
                                        "widget_class": "/Script/UMG.Button",
                                        "children": [
                                            {
                                                "widget_name": "PlayTxt",
                                                "widget_class": "/Script/UMG.TextBlock",
                                                "properties": {"Text": "START GAME"}
                                            }
                                        ]
                                    },
                                    {
                                        "widget_name": "ShopBtn",
                                        "widget_class": "/Script/UMG.Button",
                                        "children": [
                                            {
                                                "widget_name": "ShopTxt",
                                                "widget_class": "/Script/UMG.TextBlock",
                                                "properties": {"Text": "WEAPON SHOP"}
                                            }
                                        ]
                                    }
                                ]
                            }
                        ]
                    }
                ]
            }

            # --- Apply JSON ---
            logger.info("Applying JSON layout...")
            # We need to get the current asset path first to pass it explicitly if needed, 
            # or just rely on the default context. apply_json_to_umg requires asset_path.
            
            target_resp = await session.call_tool("get_target_umg_asset", arguments={})
            asset_path = ""
            if target_resp.content:
                try:
                    data = json.loads(target_resp.content[0].text)
                    if data.get("status") == "success":
                        asset_path = data.get("result", {}).get("data", {}).get("asset_path")
                        logger.info(f"Target Asset: {asset_path}")
                except:
                    pass
            
            if not asset_path:
                logger.error("❌ No target asset found. Please open a UMG asset.")
                return

            res = await session.call_tool("apply_json_to_umg", arguments={
                "asset_path": asset_path,
                "json_data": lobby_layout
            })
            
            if res.content:
                print(f"Result: {res.content[0].text}")
            
            # --- Save Asset ---
            print(f"\nSaving Asset...")
            await session.call_tool("save_asset", arguments={"asset_path": asset_path})

            print("\n" + "="*60)
            print("✅ CF Lobby JSON Application Complete!")
            print("="*60)

if __name__ == "__main__":
    asyncio.run(run_test())
