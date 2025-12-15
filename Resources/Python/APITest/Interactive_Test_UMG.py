import asyncio
import json
import sys
from mcp import ClientSession, StdioServerParameters
from mcp.client.stdio import stdio_client

async def run_interactive_test():
    print("🚀 Launching UmgMcpServer via Stdio (Interactive Mode)...")
    
    # Connect to the exact same server file that CLI uses
    server_params = StdioServerParameters(
        command="python",
        args=["../UmgMcpServer.py"],
        env=None
    )

    async with stdio_client(server_params) as (read, write):
        async with ClientSession(read, write) as session:
            await session.initialize()
            
            print("\n" + "="*80)
            print("🕹️ INTERACTIVE UMG BLUEPRINT CREATOR")
            print("Type a name and press ENTER to create a new UserWidget Blueprint.")
            print("Type 'q' or 'exit' to quit.")
            print("="*80)
            
            counter = 1
            
            while True:
                # Use standard input correctly in async context is tricky, 
                # but for a simple test blocking input is fine as it simulates user pause.
                try:
                    user_input = await asyncio.get_event_loop().run_in_executor(None, sys.stdin.readline)
                    if not user_input:
                        break
                    user_input = user_input.strip()
                except EOFError:
                    break
                
                if user_input.lower() in ['q', 'exit']:
                    break
                
                # Default name if empty
                asset_name = user_input if user_input else f"TestWidget_{counter}"
                counter += 1
                
                # Construct a proper package path
                asset_path = f"/Game/TestGen/{asset_name}"
                
                print(f"📡 Sending Request: Set Target (Create) '{asset_path}'...")
                
                try:
                    # Call set_target_umg_asset which creates the asset if it doesn't exist
                    result = await session.call_tool("set_target_umg_asset", arguments={
                        "asset_path": asset_path
                    })
                    
                    print(f"✅ Success! Response received.")
                    print(f"Result: {result}")
                    
                except Exception as e:
                    print(f"❌ Error/Timeout: {e}")
                    
                print("-" * 40)

            print("👋 Test End")

if __name__ == "__main__":
    try:
        asyncio.run(run_interactive_test())
    except KeyboardInterrupt:
        pass
