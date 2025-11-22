import asyncio
import logging
import json
from mcp import ClientSession, StdioServerParameters
from mcp.client.stdio import stdio_client

# Configure logging
logging.basicConfig(level=logging.INFO, format='%(name)s - %(levelname)s - %(message)s')
logger = logging.getLogger("test_sensing")

def print_tree(node, prefix="", is_last=True):
    """Pretty prints the widget tree structure."""
    connector = "└── " if is_last else "├── "
    name = node.get("widget_name", "Unknown")
    w_class = node.get("widget_class", "Unknown")
    
    print(f"{prefix}{connector}{name} ({w_class})")
    
    children = node.get("children", [])
    child_prefix = prefix + ("    " if is_last else "│   ")
    
    for i, child in enumerate(children):
        print_tree(child, child_prefix, i == len(children) - 1)

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
            print("👁️ UMG Sensing Test: get_widget_tree")
            print("="*60)
            
            logger.info("Requesting widget tree...")
            result = await session.call_tool("get_widget_tree", arguments={})
            
            if result.content:
                try:
                    # Handle both string and object content if necessary, though usually it's text
                    content_text = result.content[0].text if hasattr(result.content[0], 'text') else str(result.content[0])
                    data = json.loads(content_text)
                    
                    if data.get("status") == "success":
                        root = data.get("result", {}).get("data", {})
                        print("\n🌳 Widget Tree Structure:\n")
                        print_tree(root)
                        print("\n✅ Sensing Test Complete!")
                    else:
                        logger.error(f"❌ Server returned error: {data.get('error')}")
                        logger.info(f"Raw response: {content_text}")
                except json.JSONDecodeError as e:
                    logger.error(f"❌ Failed to parse JSON: {e}")
                    logger.info(f"Raw content: {result.content[0].text[:500]}...")
            else:
                logger.error("❌ No content received from server.")

if __name__ == "__main__":
    asyncio.run(run_test())
