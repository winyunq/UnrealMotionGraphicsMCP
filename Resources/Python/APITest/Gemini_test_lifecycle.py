import asyncio
import logging
from mcp import ClientSession, StdioServerParameters
from mcp.client.stdio import stdio_client

# Configure logging
logging.basicConfig(level=logging.INFO, format='%(name)s - %(levelname)s - %(message)s')
logger = logging.getLogger("test_lifecycle")

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
            print("🚀 UMG Lifecycle Test: Create -> Modify -> Delete")
            print("="*60)
            print("⚠️  Please ensure you have a UMG asset open in Unreal Editor.")
            print("⚠️  Ideally, it should have a 'CanvasPanel' or some container we can add to.")
            input("👉 Press Enter to start...")

            # --- Step 0: Find a valid parent ---
            logger.info("🔍 Finding a valid parent widget...")
            tree_result = await session.call_tool("get_widget_tree", arguments={})
            
            # Log raw result for debugging
            if tree_result.content:
                logger.info(f"Raw tree result length: {len(tree_result.content)}")
                if len(tree_result.content) > 0:
                    logger.info(f"First content item type: {type(tree_result.content[0])}")
                    if hasattr(tree_result.content[0], 'text'):
                        logger.info(f"Raw JSON (first 500 chars): {tree_result.content[0].text[:500]}...")
            else:
                logger.error("❌ Tree result content is empty!")

            parent_name = None
            if tree_result.content:
                import json
                try:
                    data = json.loads(tree_result.content[0].text)
                    if data.get("status") == "success":
                        root = data.get("result", {}).get("data", {})
                        # Try to find a CanvasPanel or just use the root if it's a container
                        # Simple traversal to find a CanvasPanel
                        def find_canvas(node):
                            if "CanvasPanel" in node.get("widget_class", ""):
                                return node.get("widget_name")
                            for child in node.get("children", []):
                                res = find_canvas(child)
                                if res: return res
                            return None
                        
                        parent_name = find_canvas(root)
                        if not parent_name:
                            # Fallback to root name
                            parent_name = root.get("widget_name")
                            logger.warning(f"Could not find a CanvasPanel, trying to use root '{parent_name}' as parent.")
                        else:
                            logger.info(f"✅ Found target parent: {parent_name}")
                    else:
                        logger.error(f"❌ Server returned error: {data.get('error')}")
                except Exception as e:
                    logger.error(f"Failed to parse tree: {e}")
                    return

            if not parent_name:
                logger.error("❌ Could not determine a parent widget. Aborting.")
                return

            # --- Step 1: Create Button ---
            print(f"\n[Step 1] Creating 'TestButton' inside '{parent_name}'...")
            res = await session.call_tool("create_widget", arguments={
                "parent_name": parent_name,
                "widget_type": "Button",
                "new_widget_name": "TestButton"
            })
            print(f"Result: {res.content[0].text}")
            input("👉 Check Unreal: Is there a 'TestButton'? Press Enter to continue...")

            # --- Step 2: Add Text to Button ---
            print(f"\n[Step 2] Creating 'TestButtonText' inside 'TestButton'...")
            res = await session.call_tool("create_widget", arguments={
                "parent_name": "TestButton",
                "widget_type": "TextBlock",
                "new_widget_name": "TestButtonText"
            })
            print(f"Result: {res.content[0].text}")
            input("👉 Check Unreal: Does the button have text? Press Enter to continue...")

            # --- Step 3: Set Initial Text ---
            print(f"\n[Step 3] Setting text to 'Hello World'...")
            res = await session.call_tool("set_widget_properties", arguments={
                "widget_name": "TestButtonText",
                "properties": {"Text": "Hello World"}
            })
            print(f"Result: {res.content[0].text}")
            input("👉 Check Unreal: Does it say 'Hello World'? Press Enter to continue...")

            # --- Step 4: Modify Text ---
            print(f"\n[Step 4] Changing text to 'Modified Text'...")
            res = await session.call_tool("set_widget_properties", arguments={
                "widget_name": "TestButtonText",
                "properties": {"Text": "Modified Text", "ColorAndOpacity": {"SpecifiedColor": {"R": 1, "G": 0, "B": 0, "A": 1}}}
            })
            print(f"Result: {res.content[0].text}")
            input("👉 Check Unreal: Did text change to 'Modified Text' (and maybe red)? Press Enter to continue...")

            # --- Step 5: Delete Button ---
            print(f"\n[Step 5] Deleting 'TestButton' (should also delete text)...")
            res = await session.call_tool("delete_widget", arguments={
                "widget_name": "TestButton"
            })
            print(f"Result: {res.content[0].text}")
            input("👉 Check Unreal: Is the button gone? Press Enter to continue...")

            # --- Step 6: Create Image ---
            print(f"\n[Step 6] Creating 'TestImage' inside '{parent_name}'...")
            res = await session.call_tool("create_widget", arguments={
                "parent_name": parent_name,
                "widget_type": "Image",
                "new_widget_name": "TestImage"
            })
            print(f"Result: {res.content[0].text}")
            input("👉 Check Unreal: Is there a 'TestImage'? Press Enter to continue...")

            # --- Step 7: Delete Image ---
            print(f"\n[Step 7] Deleting 'TestImage'...")
            res = await session.call_tool("delete_widget", arguments={
                "widget_name": "TestImage"
            })
            print(f"Result: {res.content[0].text}")

            # --- Step 8: Reparenting Test Setup ---
            print(f"\n[Step 8] Setting up Reparent Test (SourceBox & DestBox)...")
            await session.call_tool("create_widget", arguments={
                "parent_name": parent_name, "widget_type": "VerticalBox", "new_widget_name": "SourceBox"
            })
            await session.call_tool("create_widget", arguments={
                "parent_name": parent_name, "widget_type": "VerticalBox", "new_widget_name": "DestBox"
            })
            await session.call_tool("create_widget", arguments={
                "parent_name": "SourceBox", "widget_type": "TextBlock", "new_widget_name": "TravelerWidget"
            })
            await session.call_tool("set_widget_properties", arguments={
                "widget_name": "TravelerWidget", "properties": {"Text": "I am moving!"}
            })
            input("👉 Check Unreal: 'TravelerWidget' should be in 'SourceBox'. Press Enter to Reparent...")

            # --- Step 9: Reparent Widget ---
            print(f"\n[Step 9] Reparenting 'TravelerWidget' from 'SourceBox' to 'DestBox'...")
            res = await session.call_tool("reparent_widget", arguments={
                "widget_name": "TravelerWidget",
                "new_parent_name": "DestBox"
            })
            print(f"Result: {res.content[0].text}")
            input("👉 Check Unreal: 'TravelerWidget' should now be in 'DestBox'. Press Enter to Cleanup...")

            # --- Step 10: Cleanup Reparent Test ---
            print(f"\n[Step 10] Cleaning up boxes...")
            await session.call_tool("delete_widget", arguments={"widget_name": "SourceBox"})
            await session.call_tool("delete_widget", arguments={"widget_name": "DestBox"})
            
            print("\n" + "="*60)
            print("✅ Test Complete!")
            print("="*60)

if __name__ == "__main__":
    asyncio.run(run_test())
