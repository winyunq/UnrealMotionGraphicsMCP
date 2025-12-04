import sys
import os
import time
import math
import logging

# Add parent directory to path to import modules
sys.path.append(os.path.abspath(os.path.join(os.path.dirname(__file__), '..')))

from mcp_config import UNREAL_HOST, UNREAL_PORT
from Widget import UMGSet, UMGGet
from Animation import UMGSequencer
from FileManage import UMGAttention

# Setup Logging
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')
logger = logging.getLogger("SequencerShowcase")

class UnrealConnection:
    """Simple connection manager for the test script."""
    def __init__(self):
        import socket
        self.host = UNREAL_HOST
        self.port = UNREAL_PORT
        self.socket = None

    def send_command(self, command, params=None):
        import socket
        import json
        
        try:
            s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            s.settimeout(10)
            s.connect((self.host, self.port))
            
            cmd_obj = {"command": command, "params": params or {}}
            # Use the delimiter defined in the server
            msg = json.dumps(cmd_obj)
            s.sendall(msg.encode('utf-8') + b'\0')
            
            # Receive response
            response_data = b""
            while True:
                chunk = s.recv(4096)
                if not chunk: break
                response_data += chunk
                if b'\0' in response_data:
                    break
            
            s.close()
            
            try:
                # Split by delimiter and take the first part
                valid_json_bytes = response_data.split(b'\0')[0]
                return json.loads(valid_json_bytes.decode('utf-8'))
            except:
                return {"status": "error", "message": "Invalid JSON response", "raw": response_data.decode('utf-8', errors='ignore')}
                
        except Exception as e:
            return {"status": "error", "message": str(e)}

def main():
    conn = UnrealConnection()
    
    # Initialize Clients
    umg_set = UMGSet.UMGSet(conn)
    umg_get = UMGGet.UMGGet(conn)
    umg_seq = UMGSequencer.UMGSequencer(conn)
    umg_att = UMGAttention.UMGAttention(conn)

    logger.info("=== Starting Gemini Sequencer Showcase ===")

    # 1. Setup Environment
    timestamp = int(time.time())
    asset_path = f"/Game/GeminiTests/Showcase_{timestamp}"
    logger.info(f"1. Setting up Environment: {asset_path}")
    
    # This will create the asset if it doesn't exist (thanks to our C++ fix)
    res = umg_att.set_target_umg_asset(asset_path)
    if res.get("status") != "success":
        logger.error(f"Failed to set/create target asset: {res}")
        return

    # 2. Setup Widget Hierarchy
    logger.info("2. Creating Widgets...")
    
    # Always create a new root for this fresh asset
    root_name = "Canvas_Root"
    res = umg_set.create_widget(widget_type="CanvasPanel", widget_name=root_name)
    if res.get("status") != "success":
        logger.error(f"Failed to create root widget: {res}")
        return
    
    # Background Border (for pulsing)
    # We use 'root_name' as parent
    umg_set.create_widget(widget_type="Border", widget_name="Border_Background", parent_name=root_name)
    umg_set.set_widget_properties(widget_name="Border_Background", properties={
        "Slot.CanvasSlot.SizeX": 600.0,
        "Slot.CanvasSlot.SizeY": 200.0,
        "Slot.CanvasSlot.Position": {"X": 500.0, "Y": 300.0},
        "Slot.CanvasSlot.Alignment": {"X": 0.5, "Y": 0.5},
        "Background.Color": {"R": 0.1, "G": 0.1, "B": 0.3, "A": 1.0},
        "RenderTransform.Pivot": {"X": 0.5, "Y": 0.5} # Center pivot for scaling
    })

    # Horizontal Box for Text
    umg_set.create_widget(widget_type="HorizontalBox", widget_name="HBox_Text", parent_name=root_name)
    umg_set.set_widget_properties(widget_name="HBox_Text", properties={
        "Slot.CanvasSlot.Position": {"X": 500.0, "Y": 300.0},
        "Slot.CanvasSlot.Alignment": {"X": 0.5, "Y": 0.5},
        "Slot.CanvasSlot.SizeToContent": True
    })

    # Create Text Blocks (Typewriter)
    target_text = "Create by gemini 3"
    text_widgets = []
    
    # Clear old children if re-running (simplistic approach)
    # In a real script we might delete them first, but let's assume clean slate or overwrite
    
    for i, char in enumerate(target_text):
        if char == " ":
            # Spacer for space
            spacer_name = f"Spacer_{i}"
            umg_set.create_widget(widget_type="Spacer", widget_name=spacer_name, parent_name="HBox_Text")
            umg_set.set_widget_properties(widget_name=spacer_name, properties={"Size": {"X": 20.0, "Y": 1.0}})
            continue
            
        w_name = f"Char_{i}_{char}"
        text_widgets.append(w_name)
        umg_set.create_widget(widget_type="TextBlock", widget_name=w_name, parent_name="HBox_Text")
        umg_set.set_widget_properties(widget_name=w_name, properties={
            "Text": char,
            "Font.Size": 48,
            "RenderOpacity": 0.0 # Start invisible
        })
    
    start_delay = 0.5
    char_duration = 0.1
    
    # 2. Create Animation
    logger.info("2. Creating Animation 'ShowcaseAnim'...")
    res = umg_seq.create_animation(animation_name="ShowcaseAnim")
    if res.get("status") != "success":
        logger.error(f"Failed to create animation: {res}")
        return

    # 3. Animate Text (Typewriter Effect)
    logger.info("3. Animating Text...")
    umg_seq.set_animation_scope("ShowcaseAnim")

    for i, w_name in enumerate(text_widgets):
        umg_seq.set_widget_scope(w_name)
        
        start_time = start_delay + (i * char_duration)
        end_time = start_time + 0.5 # Fade in duration
        
        keys = [
            {"time": 0.0, "value": 0.0},
            {"time": start_time, "value": 0.0},
            {"time": end_time, "value": 1.0}
        ]
        
        umg_seq.set_property_keys(property_name="RenderOpacity", keys=keys)
        
        # Pop effect (Scale)
        scale_keys = [
            {"time": start_time, "value": 0.0},
            {"time": start_time + 0.2, "value": 1.5}, # Overshoot
            {"time": start_time + 0.4, "value": 1.0}  # Settle
        ]
        # Note: We need to set X and Y separately
        umg_seq.set_property_keys(property_name="RenderTransform.Scale.X", keys=scale_keys)
        umg_seq.set_property_keys(property_name="RenderTransform.Scale.Y", keys=scale_keys)

    # 4. Animate Border (Pulsing)
    logger.info("4. Generating Border Pulse...")
    umg_seq.set_widget_scope("Border_Background")
    
    pulse_keys = []
    for t in range(0, 50): # 5 seconds
        time_sec = t * 0.1
        val = 1.0 + (0.1 * math.sin(time_sec * 5.0)) # Sine wave +/- 10%
        pulse_keys.append({"time": time_sec, "value": val})
        
    umg_seq.set_property_keys(property_name="RenderTransform.Scale.X", keys=pulse_keys)
    umg_seq.set_property_keys(property_name="RenderTransform.Scale.Y", keys=pulse_keys)

    # 5. Verify
    logger.info("5. Verifying Animation Data...")
    data = umg_seq.get_all_animations()
    logger.info(f"Animations: {data}")
    
    logger.info("=== Showcase Setup Complete ===")
    logger.info("Please open the 'ShowcaseAnim' in Unreal Engine to view the result.")

if __name__ == "__main__":
    main()
