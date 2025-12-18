
import asyncio
from UmgMcpServer import add_step, prepare_value, connect_data_to_pin, get_function_nodes, get_unreal_connection

async def test_new_api():
    print("Testing New API...")
    
    # 1. Create Data Node (prepare_value with name)
    print("1. Creating Data Node 'MakeLiteralString'...")
    node_res = await prepare_value(name="MakeLiteralString", args=["Hello World"])
    print(f"   Result: {node_res}")
    
    # 2. Add Execution Step (add_step with name)
    print("2. Adding Step 'PrintString'...")
    step_res = await add_step(name="PrintString")
    print(f"   Result: {step_res}")
    
    # 3. Connect (simulate input_wires or manual connect)
    if node_res.get("success") and step_res.get("success"):
        print("3. Connecting Data to Step...")
        # Note: In real usage, input_wires does this automatically. Here we test manual connect.
        data_id = node_res["nodeId"] 
        target_id = step_res["nodeId"]
        # PrintString input pin is usually "InString"
        conn_res = await connect_data_to_pin(f"{data_id}:ReturnValue", f"{target_id}:InString")
        print(f"   Result: {conn_res}")

    # 4. Get Scoped Nodes
    print("4. Getting Scoped Nodes (should be small list)...")
    nodes_res = await get_function_nodes()
    if nodes_res.get("success"):
        count = len(nodes_res["nodes"])
        print(f"   Got {count} nodes (Expected < 50).")
    else:
        print(f"   Error: {nodes_res}")

if __name__ == "__main__":
    loop = asyncio.new_event_loop()
    loop.run_until_complete(test_new_api())
