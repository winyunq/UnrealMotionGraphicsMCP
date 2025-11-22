import asyncio
import json
import logging
from mcp import ClientSession, StdioServerParameters
from mcp.client.stdio import stdio_client

# Configure logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger("test_umg_mcp")

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
            logger.info(f"Available tools: {[tool.name for tool in tools.tools]}")

            # Test get_widget_schema
            logger.info("Testing get_widget_schema...")
            try:
                result = await session.call_tool("get_widget_schema", arguments={"widget_type": "/Script/UMG.Button"})
                # MCP returns a list of Content objects (TextContent or ImageContent)
                # We need to extract the text from the first TextContent
                if result.content and hasattr(result.content[0], 'text'):
                     logger.info(f"get_widget_schema result: {result.content[0].text}")
                else:
                     logger.info(f"get_widget_schema result (raw): {result}")
            except Exception as e:
                logger.error(f"get_widget_schema failed: {e}")

            # Test get_creatable_widget_types
            logger.info("Testing get_creatable_widget_types...")
            try:
                result = await session.call_tool("get_creatable_widget_types", arguments={})
                if result.content and hasattr(result.content[0], 'text'):
                     logger.info(f"get_creatable_widget_types result: {result.content[0].text}")
                else:
                     logger.info(f"get_creatable_widget_types result (raw): {result}")
            except Exception as e:
                logger.error(f"get_creatable_widget_types failed: {e}")

if __name__ == "__main__":
    asyncio.run(run())
