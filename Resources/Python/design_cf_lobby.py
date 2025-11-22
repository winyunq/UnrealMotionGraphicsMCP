import asyncio
import logging
from mcp import ClientSession, StdioServerParameters
from mcp.client.stdio import stdio_client

# Configure logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger("design_cf_lobby")

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

            logger.info("Applying CF-Style Lobby Design...")

            # CF Lobby Layout
            # - Dark tactical background
            # - Top Info Bar (Rank, Name, GP)
            # - Left Server List
            # - Center Room List
            # - Bottom Function Bar
            
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

                    <!-- Center: Room List -->
                    <CanvasPanel Slot.Position="(X=310,Y=90)" Slot.Size="(X=1200,Y=800)">
                        <Border Brush.Color="(R=0.0,G=0.0,B=0.0,A=0.5)" Slot.Size="(X=1200,Y=800)">
                            <VerticalBox Slot.Position="(X=10,Y=10)">
                                <!-- Header -->
                                <HorizontalBox>
                                    <TextBlock Text="ID" Slot.Size="(X=50,Y=30)"/>
                                    <Spacer Size="(X=10,Y=1)"/>
                                    <TextBlock Text="Room Name" Slot.Size="(X=400,Y=30)"/>
                                    <Spacer Size="(X=10,Y=1)"/>
                                    <TextBlock Text="Map" Slot.Size="(X=200,Y=30)"/>
                                    <Spacer Size="(X=10,Y=1)"/>
                                    <TextBlock Text="Mode" Slot.Size="(X=150,Y=30)"/>
                                    <Spacer Size="(X=10,Y=1)"/>
                                    <TextBlock Text="Status" Slot.Size="(X=100,Y=30)"/>
                                </HorizontalBox>
                                <Spacer Size="(X=1,Y=5)"/>
                                <!-- Room Items (Simulated) -->
                                <Button Name="Room1">
                                    <HorizontalBox>
                                        <TextBlock Text="001" ColorAndOpacity="(R=0.8,G=0.8,B=0.8,A=1)"/>
                                        <Spacer Size="(X=25,Y=1)"/>
                                        <TextBlock Text="[Sniper Only] Black Widow" ColorAndOpacity="(R=1,G=1,B=1,A=1)"/>
                                        <Spacer Size="(X=200,Y=1)"/>
                                        <TextBlock Text="Black Widow" ColorAndOpacity="(R=0.7,G=0.7,B=0.7,A=1)"/>
                                        <Spacer Size="(X=100,Y=1)"/>
                                        <TextBlock Text="TDM" ColorAndOpacity="(R=0.7,G=0.7,B=0.7,A=1)"/>
                                        <Spacer Size="(X=100,Y=1)"/>
                                        <TextBlock Text="WAITING" ColorAndOpacity="(R=0.2,G=1.0,B=0.2,A=1)"/>
                                    </HorizontalBox>
                                </Button>
                                <Spacer Size="(X=1,Y=2)"/>
                                <Button Name="Room2">
                                    <HorizontalBox>
                                        <TextBlock Text="002" ColorAndOpacity="(R=0.8,G=0.8,B=0.8,A=1)"/>
                                        <Spacer Size="(X=25,Y=1)"/>
                                        <TextBlock Text="Knife Fight Mid" ColorAndOpacity="(R=1,G=1,B=1,A=1)"/>
                                        <Spacer Size="(X=255,Y=1)"/>
                                        <TextBlock Text="Arena" ColorAndOpacity="(R=0.7,G=0.7,B=0.7,A=1)"/>
                                        <Spacer Size="(X=100,Y=1)"/>
                                        <TextBlock Text="Melee" ColorAndOpacity="(R=0.7,G=0.7,B=0.7,A=1)"/>
                                        <Spacer Size="(X=100,Y=1)"/>
                                        <TextBlock Text="PLAYING" ColorAndOpacity="(R=1.0,G=0.2,B=0.2,A=1)"/>
                                    </HorizontalBox>
                                </Button>
                            </VerticalBox>
                        </Border>
                    </CanvasPanel>

                    <!-- Right: Character Preview & Quick Join -->
                    <CanvasPanel Slot.Position="(X=1520,Y=90)" Slot.Size="(X=390,Y=800)">
                         <Border Brush.Color="(R=0.05,G=0.05,B=0.05,A=0.8)" Slot.Size="(X=390,Y=500)">
                            <TextBlock Text="[CHARACTER PREVIEW]" Justification="Center" Slot.Position="(X=100,Y=200)"/>
                         </Border>
                         <Button Name="QuickJoinBtn" Slot.Position="(X=0,Y=520)" Slot.Size="(X=390,Y=80)">
                            <TextBlock Text="QUICK JOIN" Font.Size="32" Justification="Center"/>
                         </Button>
                    </CanvasPanel>

                    <!-- Bottom Bar: Menu -->
                    <CanvasPanel Slot.Position="(X=0,Y=980)" Slot.Size="(X=1920,Y=100)">
                        <Border Brush.Color="(R=0.15,G=0.15,B=0.18,A=1.0)" Slot.Size="(X=1920,Y=100)">
                            <HorizontalBox Slot.Position="(X=50,Y=20)">
                                <Button Name="ShopBtn" Slot.Size="(X=150,Y=60)"><TextBlock Text="SHOP" Font.Size="24"/></Button>
                                <Spacer Size="(X=20,Y=1)"/>
                                <Button Name="StorageBtn" Slot.Size="(X=150,Y=60)"><TextBlock Text="STORAGE" Font.Size="24"/></Button>
                                <Spacer Size="(X=20,Y=1)"/>
                                <Button Name="CapsuleBtn" Slot.Size="(X=150,Y=60)"><TextBlock Text="CAPSULE" Font.Size="24"/></Button>
                                <Spacer Size="(X=20,Y=1)"/>
                                <Button Name="MissionBtn" Slot.Size="(X=150,Y=60)"><TextBlock Text="MISSION" Font.Size="24"/></Button>
                                <Spacer Size="(X=400,Y=1)"/>
                                <Button Name="OptionsBtn" Slot.Size="(X=150,Y=60)"><TextBlock Text="OPTIONS" Font.Size="24"/></Button>
                                <Spacer Size="(X=20,Y=1)"/>
                                <Button Name="ExitBtn" Slot.Size="(X=150,Y=60)"><TextBlock Text="EXIT" Font.Size="24" ColorAndOpacity="(R=1,G=0.2,B=0.2,A=1)"/></Button>
                            </HorizontalBox>
                        </Border>
                    </CanvasPanel>

                </Border>
            </CanvasPanel>
            """

            try:
                # Rely on automatic detection of currently open UMG file
                result = await session.call_tool("apply_layout", arguments={
                    "layout_content": cf_lobby_html
                })
                
                if result.content and hasattr(result.content[0], 'text'):
                    logger.info(f"Apply Result: {result.content[0].text}")
                else:
                    logger.info(f"Apply Result (raw): {result}")
            except Exception as e:
                logger.error(f"Failed to apply CF Lobby: {e}")

if __name__ == "__main__":
    asyncio.run(run())
