import asyncio
import logging
from mcp import ClientSession, StdioServerParameters
from mcp.client.stdio import stdio_client

# Configure logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger("test_default_workspace")

async def run():
    """测试默认工作区功能"""
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
            logger.info("测试默认工作区功能")
            logger.info("=" * 60)
            
            # 测试一个简单的UI布局
            simple_ui = """
            <CanvasPanel>
                <Border Name="TestBorder" Slot.Size="(X=800,Y=600)" Brush.Color="(R=0.2,G=0.3,B=0.5,A=1.0)">
                    <VerticalBox>
                        <TextBlock Text="默认工作区测试" Font.Size="32" ColorAndOpacity="(R=1,G=1,B=1,A=1)"/>
                        <Spacer Size="(X=1,Y=20)"/>
                        <Button Name="TestButton">
                            <TextBlock Text="测试按钮" Font.Size="24"/>
                        </Button>
                    </VerticalBox>
                </Border>
            </CanvasPanel>
            """

            try:
                logger.info("应用布局（不指定资产路径，应该自动使用默认工作区）...")
                result = await session.call_tool("apply_layout", arguments={
                    "layout_content": simple_ui
                    # 故意不传 asset_path
                })
                
                if result.content and hasattr(result.content[0], 'text'):
                    import json
                    result_data = json.loads(result.content[0].text)
                    logger.info(f"结果: {json.dumps(result_data, indent=2, ensure_ascii=False)}")
                    
                    if result_data.get("status") == "success":
                        logger.info("✅ 默认工作区功能正常！")
                    else:
                        logger.error(f"❌ 失败: {result_data.get('error')}")
                else:
                    logger.info(f"原始结果: {result}")
                    
            except Exception as e:
                logger.error(f"❌ 异常: {e}")

if __name__ == "__main__":
    asyncio.run(run())
