import asyncio
import logging
from mcp import ClientSession, StdioServerParameters
from mcp.client.stdio import stdio_client

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger("test_protocol")

async def run():
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
            
            logger.info("Testing protocol with get_target_umg_asset...")
            try:
                result = await session.call_tool("get_target_umg_asset", arguments={})
                if result.content:
                    logger.info(f"Result: {result.content[0].text}")
            except Exception as e:
                logger.error(f"Protocol Test Failed: {e}")

if __name__ == "__main__":
    asyncio.run(run())
