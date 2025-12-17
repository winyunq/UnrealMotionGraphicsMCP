import socket
import json
import time

HOST = "127.0.0.1"
PORT = 55557

def send_command(command, params=None):
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.connect((HOST, PORT))
        
        cmd = {
            "command": command,
            "params": params or {}
        }
        
        # Send with null delimiter
        msg = json.dumps(cmd).encode('utf-8') + b'\0'
        s.sendall(msg)
        
        # Receive response
        response_data = b""
        while True:
            chunk = s.recv(4096)
            if not chunk:
                break
            response_data += chunk
            
        s.close()
        
        decoded = response_data.decode('utf-8').strip('\0')
        if not decoded:
            return None
        return json.loads(decoded)
        
    except Exception as e:
        print(f"Error sending command {command}: {e}")
        return None

# 1. Set Target Asset
print("Setting target asset...")
resp = send_command("set_target_umg_asset", {"asset_path": "/Game/Gemini/CardGame/Gemini_WBP_CardGame_Full.Gemini_WBP_CardGame_Full"})
print(f"Target Asset Response: {resp}")

# 2. Create Animation
anim_name = "CardRotateAnim"
print(f"Creating animation '{anim_name}'...")
resp = send_command("create_animation", {"animation_name": anim_name})
print(f"Create Animation Response: {resp}")

# 3. Set Animation Scope
print(f"Setting animation scope to '{anim_name}'...")
resp = send_command("set_animation_scope", {"animation_name": anim_name})
print(f"Set Animation Scope Response: {resp}")

# 4. Set Widget Scope
widget_name = "NewCardInHand"
print(f"Setting widget scope to '{widget_name}'...")
resp = send_command("set_widget_scope", {"widget_name": widget_name})
print(f"Set Widget Scope Response: {resp}")

# 5. Add Keyframes for Rotation
# Property: RenderTransform.Angle (This rotates the widget)
# Keys: 0.0s -> 0 deg, 1.0s -> 360 deg
property_name = "RenderTransform.Angle"
keys = [
    {"Time": 0.0, "Value": 0.0},
    {"Time": 1.0, "Value": 360.0}
]

print(f"Adding keyframes for '{property_name}'...")
resp = send_command("set_property_keys", {
    "property_name": property_name,
    "keys": keys
})
print(f"Set Property Keys Response: {resp}")

# 6. Save Asset
print("Saving asset...")
resp = send_command("save_asset", {})
print(f"Save Asset Response: {resp}")
