
import logging
import socket
import json
import time
import sys
import os
from typing import Dict, Any, Optional, List

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger("GeminiTest")

# Configuration
UNREAL_HOST = "127.0.0.1"
UNREAL_PORT = 55557

class UnrealConnection:
    """Manages the socket connection to the UmgMcp plugin running inside Unreal Engine."""
    def __init__(self):
        self.socket = None
        self.connected = False
    
    def connect(self) -> bool:
        """Connect to the Unreal Engine instance."""
        try:
            if self.socket:
                try:
                    self.socket.close()
                except:
                    pass
                self.socket = None
            
            logger.info(f"Connecting to Unreal at {UNREAL_HOST}:{UNREAL_PORT}...")
            self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.socket.settimeout(10)
            self.socket.connect((UNREAL_HOST, UNREAL_PORT))
            self.connected = True
            logger.info("Connected to Unreal Engine")
            return True
            
        except Exception as e:
            logger.error(f"Failed to connect to Unreal: {e}")
            self.connected = False
            return False
    
    def receive_full_response(self, sock, buffer_size=4096) -> bytes:
        """Receive a complete response from Unreal, handling chunked data."""
        chunks = []
        sock.settimeout(30)
        try:
            while True:
                chunk = sock.recv(buffer_size)
                if not chunk:
                    if not chunks:
                        raise Exception("Connection closed before receiving data")
                    break
                chunks.append(chunk)
                
                data = b''.join(chunks)
                try:
                    json.loads(data.decode('utf-8'))
                    return data
                except json.JSONDecodeError:
                    continue
        except Exception as e:
            logger.error(f"Error during receive: {str(e)}")
            raise
    
    def send_command(self, command: str, params: Dict[str, Any] = None) -> Optional[Dict[str, Any]]:
        """Send a command to Unreal Engine and get the response."""
        if self.socket:
            try:
                self.socket.close()
            except:
                pass
            self.socket = None
            self.connected = False
        
        if not self.connect():
            return None
        
        try:
            command_obj = {
                "command": command,
                "params": params or {}
            }
            
            command_json = json.dumps(command_obj) + "__MCP_END__"
            self.socket.sendall(command_json.encode('utf-8'))
            
            response_data = self.receive_full_response(self.socket)
            response = json.loads(response_data.decode('utf-8'))
            
            try:
                self.socket.close()
            except:
                pass
            self.socket = None
            self.connected = False
            
            return response
            
        except Exception as e:
            logger.error(f"Error sending command: {e}")
            self.connected = False
            try:
                self.socket.close()
            except:
                pass
            self.socket = None
            return {
                "status": "error",
                "error": str(e)
            }

# Ensure current directory is in sys.path
current_dir = os.path.dirname(os.path.abspath(__file__))
if current_dir not in sys.path:
    sys.path.append(current_dir)

import UMGAttention
import UMGGet
import UMGSequencer

def main():
    conn = UnrealConnection()
    
    # 1. Get Target Asset
    attention = UMGAttention.UMGAttention(conn)
    resp = attention.get_target_umg_asset()
    
    asset_path = None
    if resp and resp.get("status") == "success":
        data = resp.get("result", {}).get("data", {})
        asset_path = data.get("asset_path")
    
    if not asset_path:
        logger.warning("No target UMG asset found. Trying to get the last edited asset...")
        resp = attention.get_last_edited_umg_asset()
        if resp and resp.get("status") == "success":
            data = resp.get("result", {}).get("data", {})
            asset_path = data.get("asset_path")

    if not asset_path:
        logger.error("Could not determine a target UMG asset. Please open a Widget Blueprint in Unreal Engine.")
        return

    logger.info(f"Targeting Asset: {asset_path}")
    
    # 2. Get Widget Tree to find a candidate widget
    getter = UMGGet.UMGGet(conn)
    resp = getter.get_widget_tree(asset_path)
    
    target_widget_name = None
    
    if resp and resp.get("status") == "success":
        tree_data = resp.get("result", {}).get("data", {})
        # Simple traversal to find a valid widget (e.g., a CanvasPanel or Image or Button)
        # We'll just take the first child of the root for simplicity, or search for a specific one
        
        def find_widget(node):
            if not node: return None
            # Prefer something that isn't the root if possible, but root is fine too
            if node.get("type") != "WidgetTree": 
                return node.get("name")
            
            children = node.get("children", [])
            for child in children:
                res = find_widget(child)
                if res: return res
            return None

        target_widget_name = find_widget(tree_data)
        
    if not target_widget_name:
        logger.error("Could not find any widgets in the asset to animate.")
        return

    logger.info(f"Selected Widget for Animation: {target_widget_name}")

    # 3. Create Animation
    sequencer = UMGSequencer.UMGSequencer(conn)
    anim_name = "GeminiTestAnim"
    
    logger.info(f"Creating animation: {anim_name}")
    resp = sequencer.create_animation(asset_path, anim_name)
    if resp.get("status") != "success":
        logger.error(f"Failed to create animation: {resp}")
        # It might already exist, which is fine, we can reuse it
    
    # 4. Focus Animation
    logger.info(f"Focusing animation: {anim_name}")
    sequencer.focus_animation(anim_name)
    
    # 5. Add Track (Render Opacity)
    logger.info(f"Adding track for {target_widget_name}.RenderOpacity")
    # Note: Property names in UMG Sequencer usually map to properties. 
    # 'RenderOpacity' is a common one.
    resp = sequencer.add_track(asset_path, anim_name, target_widget_name, "RenderOpacity")
    if resp.get("status") != "success":
        logger.error(f"Failed to add track: {resp}")
    
    # 6. Add Keys
    logger.info("Adding keys...")
    # Key at 0.0s, Value 1.0
    sequencer.add_key(asset_path, anim_name, target_widget_name, "RenderOpacity", 0.0, 1.0)
    # Key at 1.0s, Value 0.0
    sequencer.add_key(asset_path, anim_name, target_widget_name, "RenderOpacity", 1.0, 0.0)
    # Key at 2.0s, Value 1.0
    sequencer.add_key(asset_path, anim_name, target_widget_name, "RenderOpacity", 2.0, 1.0)
    
    logger.info("Animation test sequence completed.")

if __name__ == "__main__":
    main()
