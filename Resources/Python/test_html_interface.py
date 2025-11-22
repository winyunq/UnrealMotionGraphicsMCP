import asyncio
import json
import logging
from mcp import ClientSession, StdioServerParameters
from mcp.client.stdio import stdio_client

# Configure logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger("test_html_interface")

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

            # List available tools
            tools = await session.list_tools()
            tool_names = [tool.name for tool in tools.tools]
            logger.info(f"Available tools: {tool_names}")
            
            if "apply_html_to_umg" not in tool_names:
                logger.error("apply_html_to_umg tool NOT found!")
                return

            # Test preview_html_conversion
            logger.info("Testing preview_html_conversion...")
            html_content = """
            <CanvasPanel>
                <Button Name="TestBtn" Slot.Position="(X=100,Y=100)" Slot.Size="(X=200,Y=50)">
                    <TextBlock Text="Hello HTML" ColorAndOpacity="(R=1,G=0,B=0,A=1)"/>
                </Button>
            </CanvasPanel>
            """
            
            try:
                result = await session.call_tool("preview_html_conversion", arguments={"html_content": html_content})
                
                # Handle different content types
                if result.content:
                    text_content = ""
                    for content in result.content:
                        if hasattr(content, 'text'):
                            text_content += content.text
                    
                    logger.info(f"Preview Result: {text_content}")
                else:
                    logger.info("Result is empty.")
                    
            except Exception as e:
                logger.error(f"preview_html_conversion failed: {e}")

if __name__ == "__main__":
    asyncio.run(run())
