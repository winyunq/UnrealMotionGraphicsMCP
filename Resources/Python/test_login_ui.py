import asyncio
import json
import logging
from mcp import ClientSession, StdioServerParameters
from mcp.client.stdio import stdio_client

# Configure logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger("test_login_ui")

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
            # Initialize the connection
            await session.initialize()

            # 1. Find the asset "2"
            logger.info("Searching for asset '2'...")
            found_asset_path = None
            
            # Try getting recently edited assets
            try:
                result = await session.call_tool("get_recently_edited_umg_assets", arguments={})
                if result.content and hasattr(result.content[0], 'text'):
                    assets_json = json.loads(result.content[0].text)
                    if assets_json.get("status") == "success":
                        assets = assets_json.get("data", [])
                        logger.info(f"Recently edited assets: {assets}")
                        for asset in assets:
                            if asset.endswith("/2") or asset.endswith("/2.2"): # Handle potential package names
                                found_asset_path = asset
                                break
            except Exception as e:
                logger.error(f"Failed to get recent assets: {e}")

            # If not found in recent, try getting the currently open asset
            if not found_asset_path:
                try:
                    result = await session.call_tool("get_target_umg_asset", arguments={})
                    # Log raw result for debugging
                    if result.content and hasattr(result.content[0], 'text'):
                         logger.info(f"Raw get_target_umg_asset result: {result.content[0].text}")
                         
                    if result.content and hasattr(result.content[0], 'text'):
                        current_asset_json = json.loads(result.content[0].text)
                        if current_asset_json.get("status") == "success":
                            current_asset = current_asset_json.get("result", {}).get("data", {}).get("asset_path")
                            logger.info(f"Current open asset: {current_asset}")
                            if current_asset and ("2" in current_asset.split("/")[-1]):
                                found_asset_path = current_asset
                except Exception as e:
                    logger.error(f"Failed to get target asset: {e}")

            if not found_asset_path:
                logger.warning("Could not automatically detect asset '2'.")
                logger.info("Attempting to force access to '/Game/2'...")
                found_asset_path = "/Game/2"

            logger.info(f"Targeting asset: {found_asset_path}")

            # 2. Set it as target (just to be sure)
            await session.call_tool("set_target_umg_asset", arguments={"asset_path": found_asset_path})

            # 3. Apply Login UI HTML
            logger.info("Applying Login UI...")
            login_html = """
            <CanvasPanel>
                <Border Name="Background" Slot.Size="(X=1920,Y=1080)" Brush.Color="(R=0.1,G=0.1,B=0.1,A=1)">
                    <VerticalBox Slot.Position="(X=800,Y=400)" Slot.Size="(X=320,Y=300)">
                        <TextBlock Text="Welcome Back" Font.Size="24" ColorAndOpacity="(R=1,G=1,B=1,A=1)" Justification="Center"/>
                        <Spacer Size="(X=1,Y=20)"/>
                        <EditableTextBox Name="UsernameInput" HintText="Username"/>
                        <Spacer Size="(X=1,Y=10)"/>
                        <EditableTextBox Name="PasswordInput" HintText="Password" IsPassword="true"/>
                        <Spacer Size="(X=1,Y=20)"/>
                        <Button Name="LoginBtn">
                            <TextBlock Text="Log In" ColorAndOpacity="(R=0,G=0,B=0,A=1)"/>
                        </Button>
                    </VerticalBox>
                </Border>
            </CanvasPanel>
            """
            
            try:
                result = await session.call_tool("apply_html_to_umg", arguments={
                    "asset_path": found_asset_path,
                    "html_content": login_html
                })
                
                if result.content and hasattr(result.content[0], 'text'):
                    logger.info(f"Apply Result: {result.content[0].text}")
                else:
                    logger.info(f"Apply Result (raw): {result}")
            except Exception as e:
                logger.error(f"Failed to apply login UI: {e}")

if __name__ == "__main__":
    asyncio.run(run())
