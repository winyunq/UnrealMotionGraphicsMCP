import asyncio
import logging
from mcp import ClientSession, StdioServerParameters
from mcp.client.stdio import stdio_client

# Configure logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger("test_simple_ui")

async def run():
    """测试一个极简UI，不使用任何Slot属性"""
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

            logger.info("=" * 60)
            logger.info("测试极简UI（无Slot属性）")
            logger.info("=" * 60)
            
            # 非常简单的UI - 只有一个TextBlock在VerticalBox中
            simple_ui = """
            <VerticalBox>
                <TextBlock Text="测试文本" Font.Size="24"/>
            </VerticalBox>
            """

            try:
                logger.info("应用极简布局...")
                result = await session.call_tool("apply_layout", arguments={
                    "layout_content": simple_ui
                })
                
                if result.content and hasattr(result.content[0], 'text'):
                    import json
                    result_data = json.loads(result.content[0].text)
                    logger.info(f"结果: {json.dumps(result_data, indent=2, ensure_ascii=False)}")
                    
                    if result_data.get("status") == "success":
                        logger.info("✅ 极简UI创建成功！请手动打开资产查看是否崩溃。")
                    else:
                        logger.error(f"❌ 失败: {result_data.get('error')}")
                else:
                    logger.info(f"原始结果: {result}")
                    
            except Exception as e:
                logger.error(f"❌ 异常: {e}")

if __name__ == "__main__":
    asyncio.run(run())
