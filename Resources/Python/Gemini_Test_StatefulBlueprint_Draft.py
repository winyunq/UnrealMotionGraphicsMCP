import socket
import json
import time

UNREAL_HOST = "127.0.0.1"
UNREAL_PORT = 30010

def send_command(command, params=None):
    if params is None:
        params = {}
    
    message = {
        "command": command,
        "params": params
    }
    
    json_msg = json.dumps(message)
    
    try:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            s.settimeout(5.0)
            s.connect((UNREAL_HOST, UNREAL_PORT))
            
            # Send (Null terminated)
            s.sendall(json_msg.encode('utf-8'))
            s.sendall(b'\x00')
            
            # Receive
            data = b""
            while True:
                chunk = s.recv(4096)
                if not chunk:
                    break
                data += chunk
                if data.endswith(b'\x00'):
                    break
            
            response_str = data[:-1].decode('utf-8') # Remove null terminator
            return json.loads(response_str)
            
    except Exception as e:
        print(f"Error communicating with Unreal: {e}")
        return None

def run_test():
    print("=== Gemini Stateful Blueprint Test ===")
    
    # 1. Setup: Create a Test Blueprint
    BP_Name = "BP_GeminiStatefulTest"
    print(f"\n[1] Creating Blueprint: {BP_Name}")
    res = send_command("create_blueprint", {"name": BP_Name, "parent_class": "AActor"})
    print(f"Result: {json.dumps(res, indent=2)}")
    
    if not res or res.get("status") == "error":
        print("Failed to create blueprint. Continuing anyway if it exists...")

    # 2. Set Attention Target
    AssetPath = f"/Game/Blueprints/{BP_Name}.{BP_Name}"
    print(f"\n[2] Setting Target Asset: {AssetPath}")
    res = send_command("set_target_umg_asset", {"asset_path": AssetPath}) # Assuming generic setter uses asset_path
    # Wait, the command is "set_target_umg_asset". Does it take asset_path or package_name?
    # UmgAttentionSubsystem.cpp handles this. 
    # Usually it takes "path" or "name".
    print(f"Result: {json.dumps(res, indent=2)}")

    # 3. Set Target Graph
    print(f"\n[3] Setting Target Graph: EventGraph")
    res = send_command("set_target_graph", {"graph_name": "EventGraph"})
    print(f"Result: {json.dumps(res, indent=2)}")

    # 4. Create Initial Node (Event Construct? Actor uses BeginPlay)
    # Let's verify FindFunctions first just to be cool
    print(f"\n[Validation] Finding 'BeginPlay'")
    res = send_command("manage_blueprint_graph", {
        "subAction": "find_functions",
        "query": "ReceiveBeginPlay"
    })
    # ReceiveBeginPlay is the function name for BeginPlay event
    print(f"Result: {json.dumps(res, indent=2)}")

    # 5. Create Event Node (Begin Play)
    print(f"\n[5] Creating BeginPlay Event Node")
    # Reset Cursor first?
    send_command("set_cursor_node", {"node_id": ""})
    
    res = send_command("manage_blueprint_graph", {
        "subAction": "create_node",
        "nodeType": "Event",
        "eventName": "ReceiveBeginPlay",
        "x": 0,
        "y": 0
    })
    print(f"Result: {json.dumps(res, indent=2)}")
    
    # 6. Create Dependent Node (Print String) - Should Auto Connect!
    print(f"\n[6] Creating PrintString (Auto-Connect Expected)")
    res = send_command("manage_blueprint_graph", {
        "subAction": "create_node",
        "nodeType": "PrintString",
        # No X/Y provided, should auto-layout
        # "autoConnectToNodeId": ... Should be injected automatically by Bridge
    })
    print(f"Result: {json.dumps(res, indent=2)}")
    
    # 7. Compile
    print(f"\n[7] Compiling")
    res = send_command("compile_blueprint", {"blueprint_name": BP_Name})
    print(f"Result: {json.dumps(res, indent=2)}")

    print("\n=== Test Complete ===")

if __name__ == "__main__":
    run_test()
