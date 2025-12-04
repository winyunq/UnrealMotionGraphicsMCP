import json
import socket
import sys
import os

# Add parent directory to path to find mcp_config
sys.path.append(os.path.abspath(os.path.join(os.path.dirname(__file__), '..')))
from mcp_config import UNREAL_HOST, UNREAL_PORT

HOST = UNREAL_HOST
PORT = UNREAL_PORT
BUFFER_SIZE = 4096

def send_command(sock, command, params=None):
    request = {"command": command, "params": params or {}}
    msg = json.dumps(request) + "__MCP_END__"
    sock.sendall(msg.encode('utf-8'))
    data = b""
    while True:
        chunk = sock.recv(BUFFER_SIZE)
        if not chunk: break
        data += chunk
        try:
            return json.loads(data.decode('utf-8'))
        except: continue
    return None

def run():
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.connect((HOST, PORT))
        
        # Get Target
        print("Getting Target...")
        resp = send_command(sock, "get_target_umg_asset")
        print(f"Target: {json.dumps(resp, indent=2)}")
        
        # Get Tree
        print("\nGetting Tree...")
        resp = send_command(sock, "get_widget_tree")
        print(f"Tree: {json.dumps(resp, indent=2)}")
        
    except Exception as e:
        print(f"Error: {e}")
    finally:
        sock.close()

if __name__ == "__main__":
    run()
