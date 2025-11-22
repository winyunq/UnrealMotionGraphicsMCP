import asyncio
import json
import logging
from mcp import ClientSession, StdioServerParameters
from mcp.client.stdio import stdio_client

# Configure logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger("verify_actions")

async def run():
    # Define server parameters
    server_params = StdioServerParameters(
        command="uv",
        args=[
            "run",
            "--directory",
            "d:/ModelContextProtocol/unreal-engine-mcp/FlopperamUnrealMCP/Plugins/UmgMcp/Resources/Python",
            "UmgMcpServer.py"
        ]
    )

    async with stdio_client(server_params) as (read, write):
        async with ClientSession(read, write) as session:
            await session.initialize()

            # 1. Find Target Asset
            logger.info("--- Step 1: Finding Target Asset ---")
            target_asset = None
            
            # Strategy A: Recent Assets
            try:
                result = await session.call_tool("get_recently_edited_umg_assets", arguments={})
                if result.content and hasattr(result.content[0], 'text'):
                    data = json.loads(result.content[0].text)
                    assets = data.get("data", [])
                    logger.info(f"Recent assets: {assets}")
                    for asset in assets:
                        if "2" in asset:
                            target_asset = asset
                            break
                    if not target_asset and assets:
                        target_asset = assets[0]
            except Exception as e:
                logger.warning(f"Failed to get recent assets: {e}")

            # Strategy B: Current Open Asset
            if not target_asset:
                try:
                    logger.info("Checking currently open asset...")
                    result = await session.call_tool("get_target_umg_asset", arguments={})
                    if result.content and hasattr(result.content[0], 'text'):
                        data = json.loads(result.content[0].text)
                        # Structure is usually {status: success, result: {data: {asset_path: ...}}}
                        # Or sometimes direct data depending on implementation. 
                        # Let's parse carefully.
                        asset_path = data.get("result", {}).get("data", {}).get("asset_path")
                        if asset_path:
                            target_asset = asset_path
                            logger.info(f"Found open asset: {asset_path}")
                except Exception as e:
                    logger.warning(f"Failed to get target asset: {e}")

            # Strategy C: Hardcoded Fallback
            if not target_asset:
                logger.warning("No assets found via API. Attempting fallback to '/Game/2'...")
                target_asset = "/Game/2"

            logger.info(f"Targeting Asset: {target_asset}")

            # 2. Set Target
            logger.info("--- Step 2: Setting Target ---")
            await session.call_tool("set_target_umg_asset", arguments={"asset_path": target_asset})

            # 3. Create Widget
            logger.info("--- Step 3: Create Widget (Button) ---")
            new_widget_name = "TestButton_Gemini"
            try:
                # Assuming root widget exists, let's try to add to the root or a known container.
                # If we don't know the parent, we might fail. 
                # Let's first get the tree to find a valid parent (like a CanvasPanel).
                tree_result = await session.call_tool("get_widget_tree", arguments={})
                
                if tree_result.content and hasattr(tree_result.content[0], 'text'):
                     logger.info(f"Raw Tree Result: {tree_result.content[0].text}")
                
                tree_data = json.loads(tree_result.content[0].text)
                # The structure is result -> data -> {widget_name: ...}
                root_widget = tree_data.get("result", {}).get("data", {})
                parent_name = root_widget.get("widget_name")
                
                if not parent_name:
                    # If no root widget, maybe we can create one? 
                    # Or maybe the asset path is invalid.
                    logger.error("Could not find root widget name. Is the asset valid?")
                    return

                logger.info(f"Creating button under parent: {parent_name}")
                create_result = await session.call_tool("create_widget", arguments={
                    "parent_name": parent_name,
                    "widget_type": "Button",
                    "new_widget_name": new_widget_name
                })
                logger.info(f"Create Result: {create_result.content[0].text}")
            except Exception as e:
                logger.error(f"Create Widget Failed: {e}")

            # 4. Set Properties
            logger.info("--- Step 4: Set Properties (Color) ---")
            try:
                # Try setting background color
                prop_result = await session.call_tool("set_widget_properties", arguments={
                    "widget_name": new_widget_name,
                    "properties": {
                        "BackgroundColor": "(R=1.0,G=0.0,B=0.0,A=1.0)"
                    }
                })
                logger.info(f"Set Properties Result: {prop_result.content[0].text}")
            except Exception as e:
                logger.error(f"Set Properties Failed: {e}")

            # 5. Reparent (Skip for now as it requires a second container)
            
            # 6. Delete Widget
            logger.info("--- Step 6: Delete Widget ---")
            try:
                delete_result = await session.call_tool("delete_widget", arguments={
                    "widget_name": new_widget_name
                })
                logger.info(f"Delete Result: {delete_result.content[0].text}")
            except Exception as e:
                logger.error(f"Delete Widget Failed: {e}")

if __name__ == "__main__":
    asyncio.run(run())
