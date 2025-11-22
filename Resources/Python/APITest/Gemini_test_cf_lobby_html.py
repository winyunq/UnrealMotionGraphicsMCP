import asyncio
import logging
import json
from mcp import ClientSession, StdioServerParameters
from mcp.client.stdio import stdio_client

# Configure logging
logging.basicConfig(level=logging.INFO, format='%(name)s - %(levelname)s - %(message)s')
logger = logging.getLogger("test_cf_lobby_html")

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
            print("🌐 CF Lobby HTML Application Test")
            print("="*60)
            print("⚠️  Please ensure you have a UMG asset open in Unreal Editor.")
            input("👉 Press Enter to start...")

            # Define the CF Lobby Layout HTML
            # Note: We use custom tags that map to UMG types or standard HTML tags mapped by the parser.
            # Assuming the parser maps <div> to CanvasPanel/VerticalBox etc based on context or attributes,
            # or we use explicit UMG class names if supported.
            # Let's try a semantic HTML approach which the parser likely translates.
            
            lobby_html = """
            <CanvasPanel name="RootCanvas">
                <VerticalBox name="MainVerticalBox">
                    <Slot>
                        <LayoutData>
                            <Offsets Left="0" Top="0" Right="0" Bottom="0" />
                            <Anchors Minimum="0,0" Maximum="1,1" />
                        </LayoutData>
                    </Slot>
                    
                    <!-- Top Bar -->
                    <HorizontalBox name="TopBar">
                        <TextBlock name="GameTitle" Text="CROSSFIRE LOBBY (HTML)">
                            <Font Size="32" />
                            <ColorAndOpacity R="0" G="1" B="1" A="1" />
                        </TextBlock>
                    </HorizontalBox>
                    
                    <!-- Character Area -->
                    <Border name="CharacterView" BrushColor="(R=0.1,G=0.1,B=0.1,A=1)">
                        <Slot SizeRule="Fill" />
                        <TextBlock name="CharPreviewText" Text="HTML CHARACTER PREVIEW" Justification="Center">
                            <Font Size="48" />
                        </TextBlock>
                    </Border>
                    
                    <!-- Bottom Menu -->
                    <HorizontalBox name="BottomMenu">
                        <Button name="PlayBtn">
                            <TextBlock name="PlayTxt" Text="START GAME" />
                        </Button>
                        <Button name="ShopBtn">
                            <TextBlock name="ShopTxt" Text="WEAPON SHOP" />
                        </Button>
                    </HorizontalBox>
                </VerticalBox>
            </CanvasPanel>
            """

            # --- Apply HTML ---
            logger.info("Applying HTML layout...")
            
            # Get target asset
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

            # Use apply_layout which handles HTML detection
            res = await session.call_tool("apply_layout", arguments={
                "asset_path": asset_path,
                "layout_content": lobby_html
            })
            
            if res.content:
                print(f"Result: {res.content[0].text}")
            
            # --- Save Asset ---
            print(f"\nSaving Asset...")
            await session.call_tool("save_asset", arguments={"asset_path": asset_path})

            print("\n" + "="*60)
            print("✅ CF Lobby HTML Application Complete!")
            print("="*60)

if __name__ == "__main__":
    asyncio.run(run_test())
