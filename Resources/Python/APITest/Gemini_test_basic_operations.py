import asyncio
import logging
from mcp import ClientSession, StdioServerParameters
from mcp.client.stdio import stdio_client

# Configure logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger("test_basic_ops")

async def run():
    """测试基础的get/set操作，不涉及apply_json"""
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
            logger.info("测试基础操作：get_all_widgets 和 set_text")
            logger.info("=" * 60)
            
            # 请先手动在Unreal中打开一个简单的UMG资产（例如之前用Gemini_test_simple_ui.py创建的）
            
            try:
                # 测试1: 获取Widget树
                logger.info("\n[测试1] 获取当前UMG的Widget树...")
                result = await session.call_tool("get_widget_tree", arguments={})
                
                # 打印原始结果用于调试
                logger.info(f"result.content类型: {type(result.content) if hasattr(result, 'content') else 'No content attr'}")
                
                if result.content and len(result.content) > 0:
                    raw_text = result.content[0].text if hasattr(result.content[0], 'text') else str(result.content[0])
                    logger.info(f"原始文本长度: {len(raw_text)}")
                    
                    try:
                        import json
                        result_data = json.loads(raw_text)
                        logger.info(f"✅ 成功解析JSON")
                        
                        if result_data.get("status") == "success":
                            data = result_data.get("result", {}).get("data", {})
                            # data本身就是widget树的根节点
                            widget_tree = data
                            logger.info(f"Widget树: {json.dumps(widget_tree, indent=2, ensure_ascii=False)[:500]}...")
                            
                            # 递归查找TextBlock
                            def find_textblocks(widget, path=""):
                                """递归查找所有TextBlock"""
                                textblocks = []
                                widget_name = widget.get("widget_name", "")
                                widget_class = widget.get("widget_class", "")
                                current_path = f"{path}/{widget_name}" if path else widget_name
                                
                                if "TextBlock" in widget_class:
                                    textblocks.append({
                                        "name": widget_name,
                                        "class": widget_class,
                                        "path": current_path
                                    })
                                
                                # 递归查找children
                                children = widget.get("children", [])
                                for child in children:
                                    textblocks.extend(find_textblocks(child, current_path))
                                
                                return textblocks
                            
                            textblocks = find_textblocks(widget_tree)
                            
                            if textblocks:
                                first_textblock = textblocks[0]
                                logger.info(f"✅ 找到 {len(textblocks)} 个TextBlock")
                                logger.info(f"第一个TextBlock: {first_textblock}")
                                
                                #测试2: 修改TextBlock的Text属性
                                logger.info(f"\n[测试2] 修改TextBlock '{first_textblock['name']}' 的Text属性...")
                                set_result = await session.call_tool("set_widget_properties", arguments={
                                    "widget_name": first_textblock['name'],
                                    "properties": {
                                        "Text": "基础操作测试成功！ 🎉"
                                    }
                                })
                                
                                if set_result.content and len(set_result.content) > 0:
                                    set_text = set_result.content[0].text if hasattr(set_result.content[0], 'text') else str(set_result.content[0])
                                    set_data = json.loads(set_text)
                                    logger.info(f"设置属性结果: {json.dumps(set_data, indent=2, ensure_ascii=False)}")
                                    
                                    # 检查内层的result.status
                                    inner_result = set_data.get("result", {})
                                    inner_status = inner_result.get("status", "unknown")
                                    
                                    if inner_status == "success":
                                        logger.info("✅ set_widget_properties 成功！")
                                        logger.info("👁️ 请在Unreal编辑器中查看变化（可能需要刷新）")
                                    else:
                                        error_msg = inner_result.get("error", "未知错误")
                                        logger.error(f"❌ set_widget_properties 失败: {error_msg}")
                                        logger.error("⚠️ 请检查Unreal编辑器的日志以获取详细错误信息")
                            else:
                                logger.warning("⚠️ 没有找到TextBlock")
                                logger.info("提示：请在Unreal中打开一个包含TextBlock的UMG资产")
                        else:
                            error_msg = result_data.get("error", result_data.get("result", {}).get("message", "未知错误"))
                            logger.error(f"❌ get_widget_tree 失败: {error_msg}")
                            
                    except json.JSONDecodeError as je:
                        logger.error(f"❌ JSON解析失败: {je}")
                        logger.error(f"无法解析的文本: '{raw_text[:200]}'...")
                else:
                    logger.error("result没有content或content为空")
                    logger.warning("⚠️ 请确保：1) Unreal编辑器正在运行 2) UmgMcp插件已启动 3) 已打开一个UMG资产编辑器")
                    
            except Exception as e:
                logger.error(f"❌ 异常: {e}", exc_info=True)

if __name__ == "__main__":
    asyncio.run(run())
