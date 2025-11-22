import asyncio
import json
import logging
from mcp import ClientSession, StdioServerParameters
from mcp.client.stdio import stdio_client

# Configure logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger("test_connection")

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

            # Test get_target_umg_asset
            logger.info("Testing get_target_umg_asset...")
            try:
                result = await session.call_tool("get_target_umg_asset", arguments={})
                
                # Handle different content types
                if result.content:
                    text_content = ""
                    for content in result.content:
                        if hasattr(content, 'text'):
                            text_content += content.text
                    
                    logger.info(f"Current Asset: {text_content}")
                else:
                    logger.info("Result is empty.")
                    
            except Exception as e:
                logger.error(f"get_target_umg_asset failed: {e}")

if __name__ == "__main__":
    asyncio.run(run())
