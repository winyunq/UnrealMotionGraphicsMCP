import asyncio
import os
import sys
import json
from mcp import ClientSession, StdioServerParameters
from mcp.client.stdio import stdio_client

async def run_get_raw_data():
    print("🚀 Connecting to UmgMcpServer...")
    
    current_dir = os.path.dirname(os.path.abspath(__file__))
    server_script = os.path.abspath(os.path.join(current_dir, "..", "UmgMcpServer.py"))
    
    server_params = StdioServerParameters(
        command="python",
        args=[server_script],
        env=None
    )

    try:
        async with stdio_client(server_params) as (read, write):
            async with ClientSession(read, write) as session:
                await session.initialize()
                
                # 1. 获取路径 (这一步必须先做以确定目标)
                print("🔍 Fetching target path...")
                target_result = await session.call_tool("get_target_umg_asset", arguments={})
                # 简单解析出路径供文件名使用
                target_json = json.loads(target_result.content[0].text)
                asset_path = target_json.get("asset_path", "UnknownAsset")
                file_prefix = os.path.basename(asset_path).split('.')[0]

                # 2. 获取原始 Widget Tree 字符串
                print(f"🌳 Requesting raw 'get_widget_tree' for {file_prefix}...")
                tree_result = await session.call_tool("get_widget_tree", arguments={})
                
                # 直接提取原始文本，不进行 json.loads
                raw_tree_string = tree_result.content[0].text
                
                # 保存原始结果
                output_path = os.path.join(current_dir, f"RAW_{file_prefix}_Tree.json")
                with open(output_path, 'w', encoding='utf-8') as f:
                    f.write(raw_tree_string)
                
                print(f"💾 RAW DATA SAVED TO: {output_path}")
                print("-" * 50)
                print("📝 Preview of raw string (first 200 chars):")
                print(raw_tree_string[:200] + "...")
                print("-" * 50)

    except Exception as e:
        print(f"❌ Error: {e}")

if __name__ == "__main__":
    if sys.platform == 'win32':
        asyncio.set_event_loop_policy(asyncio.WindowsSelectorEventLoopPolicy())
    asyncio.run(run_get_raw_data())