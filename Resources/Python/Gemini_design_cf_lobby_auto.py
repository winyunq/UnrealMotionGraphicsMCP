import asyncio
import logging
from mcp import ClientSession, StdioServerParameters
from mcp.client.stdio import stdio_client

# Configure logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger("design_cf_lobby_auto")

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

            logger.info("Checking for target UMG asset...")
            
            # 首先尝试获取当前打开的UMG资产
            target_result = await session.call_tool("get_target_umg_asset", arguments={})
            
            target_path = None
            if target_result.content and hasattr(target_result.content[0], 'text'):
                import json
                result_data = json.loads(target_result.content[0].text)
                logger.info(f"get_target_umg_asset result: {result_data}")
                
                # 检查是否成功获取到目标资产
                if result_data.get("status") == "success":
                    data = result_data.get("result", {}).get("data", {})
                    if data:
                        target_path = data.get("asset_path")
                        logger.info(f"Found open UMG asset: {target_path}")
                    else:
                        logger.info("No UMG asset currently open.")
                else:
                    logger.info(f"Error getting target: {result_data.get('error')}")
            
            # 如果没有打开的UMG资产，设置一个默认路径
            if not target_path:
                default_path = "/Game/UnrealMotionGraphicsMCP.UnrealMotionGraphicsMCP"
                logger.info(f"No UMG asset open. Setting default target: {default_path}")
                
                set_result = await session.call_tool("set_target_umg_asset", arguments={
                    "asset_path": default_path
                })
                
                if set_result.content and hasattr(set_result.content[0], 'text'):
                    set_data = json.loads(set_result.content[0].text)
                    logger.info(f"set_target_umg_asset result: {set_data}")
                    
                    if set_data.get("status") == "success":
                        target_path = default_path
                        logger.info(f"Successfully set target to: {target_path}")
                    else:
                        logger.error(f"Failed to set target: {set_data.get('error')}")
                        return

            logger.info(f"Final target UMG asset: {target_path}")
            logger.info("Applying CF-Style Lobby Design...")

            # CF Lobby Layout (same as before)
            cf_lobby_html = """
            <CanvasPanel>
                <!-- Main Background -->
                <Border Name="MainBackground" Slot.Size="(X=1920,Y=1080)" Brush.Color="(R=0.05,G=0.05,B=0.07,A=1.0)">
                    
                    <!-- Top Bar: Player Info -->
                    <CanvasPanel Slot.Position="(X=0,Y=0)" Slot.Size="(X=1920,Y=80)">
                        <Border Brush.Color="(R=0.1,G=0.1,B=0.12,A=1.0)" Slot.Size="(X=1920,Y=80)">
                            <HorizontalBox Slot.Position="(X=20,Y=10)">
                                <!-- Rank Icon Placeholder -->
                                <Border Brush.Color="(R=0.8,G=0.6,B=0.0,A=1.0)" Slot.Size="(X=60,Y=60)">
                                    <TextBlock Text="GEN" ColorAndOpacity="(R=0,G=0,B=0,A=1)" Font.Size="20" Justification="Center"/>
                                </Border>
                                <Spacer Size="(X=20,Y=1)"/>
                                
                                <!-- Player Name & Stats -->
                                <VerticalBox>
                                    <TextBlock Text="PlayerName [VIP]" ColorAndOpacity="(R=1,G=0.8,B=0.2,A=1)" Font.Size="24"/>
                                    <HorizontalBox>
                                        <TextBlock Text="EXP: 99%" ColorAndOpacity="(R=0.6,G=0.6,B=0.6,A=1)" Font.Size="14"/>
                                        <Spacer Size="(X=20,Y=1)"/>
                                        <TextBlock Text="GP: 500,000" ColorAndOpacity="(R=0.4,G=0.8,B=0.4,A=1)" Font.Size="14"/>
                                        <Spacer Size="(X=20,Y=1)"/>
                                        <TextBlock Text="CF: 10,000" ColorAndOpacity="(R=1.0,G=0.2,B=0.2,A=1)" Font.Size="14"/>
                                    </HorizontalBox>
                                </VerticalBox>
                            </HorizontalBox>
                        </Border>
                    </CanvasPanel>

                    <!-- Left Sidebar: Server/Channel List -->
                    <CanvasPanel Slot.Position="(X=0,Y=80)" Slot.Size="(X=300,Y=900)">
                        <Border Brush.Color="(R=0.08,G=0.08,B=0.1,A=0.9)" Slot.Size="(X=300,Y=900)">
                            <VerticalBox Slot.Position="(X=10,Y=10)" Slot.Size="(X=280,Y=880)">
                                <TextBlock Text="SERVER LIST" ColorAndOpacity="(R=0.5,G=0.5,B=0.6,A=1)" Font.Size="18"/>
                                <Spacer Size="(X=1,Y=10)"/>
                                <Button Name="Server1Btn"><TextBlock Text="Alpha Server"/></Button>
                                <Spacer Size="(X=1,Y=5)"/>
                                <Button Name="Server2Btn"><TextBlock Text="Bravo Server"/></Button>
                                <Spacer Size="(X=1,Y=5)"/>
                                <Button Name="Server3Btn"><TextBlock Text="Clan Server"/></Button>
                                <Spacer Size="(X=1,Y=20)"/>
                                <TextBlock Text="CHANNEL" ColorAndOpacity="(R=0.5,G=0.5,B=0.6,A=1)" Font.Size="18"/>
                                <Spacer Size="(X=1,Y=10)"/>
                                <Button Name="Ch1Btn"><TextBlock Text="Channel 1 (Full)"/></Button>
                                <Spacer Size="(X=1,Y=5)"/>
                                <Button Name="Ch2Btn"><TextBlock Text="Channel 2 (High)"/></Button>
                                <Spacer Size="(X=1,Y=5)"/>
                                <Button Name="Ch3Btn"><TextBlock Text="Channel 3 (Low)"/></Button>
                            </VerticalBox>
                        </Border>
                    </CanvasPanel>

                    <!-- Bottom Bar: Menu -->
                    <CanvasPanel Slot.Position="(X=0,Y=980)" Slot.Size="(X=1920,Y=100)">
                        <Border Brush.Color="(R=0.15,G=0.15,B=0.18,A=1.0)" Slot.Size="(X=1920,Y=100)">
                            <HorizontalBox Slot.Position="(X=50,Y=20)">
                                <Button Name="ShopBtn" Slot.Size="(X=150,Y=60)"><TextBlock Text="SHOP" Font.Size="24"/></Button>
                                <Spacer Size="(X=20,Y=1)"/>
                                <Button Name="StorageBtn" Slot.Size="(X=150,Y=60)"><TextBlock Text="STORAGE" Font.Size="24"/></Button>
                                <Spacer Size="(X=20,Y=1)"/>
                                <Button Name="ExitBtn" Slot.Size="(X=150,Y=60)"><TextBlock Text="EXIT" Font.Size="24" ColorAndOpacity="(R=1,G=0.2,B=0.2,A=1)"/></Button>
                            </HorizontalBox>
                        </Border>
                    </CanvasPanel>

                </Border>
            </CanvasPanel>
            """

            try:
                # 使用自动检测或设置的目标
                result = await session.call_tool("apply_layout", arguments={
                    "layout_content": cf_lobby_html,
                    "asset_path": target_path  # 显式传递路径
                })
                
                if result.content and hasattr(result.content[0], 'text'):
                    logger.info(f"Apply Result: {result.content[0].text}")
                else:
                    logger.info(f"Apply Result (raw): {result}")
            except Exception as e:
                logger.error(f"Failed to apply CF Lobby: {e}")

if __name__ == "__main__":
    asyncio.run(run())
