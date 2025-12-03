import logging
from typing import Dict, Any, List, Optional

logger = logging.getLogger("UmgMcpServerV3")

class UMGSequencer:
    def __init__(self, connection):
        self.conn = connection

    def get_all_animations(self, asset_path: str) -> Dict[str, Any]:
        return self.conn.send_command("get_all_animations", {"asset_path": asset_path})

    def create_animation(self, asset_path: str, animation_name: str) -> Dict[str, Any]:
        return self.conn.send_command("create_animation", {
            "asset_path": asset_path,
            "animation_name": animation_name
        })

    def delete_animation(self, asset_path: str, animation_name: str) -> Dict[str, Any]:
        return self.conn.send_command("delete_animation", {
            "asset_path": asset_path,
            "animation_name": animation_name
        })

    def focus_animation(self, animation_name: str) -> Dict[str, Any]:
        return self.conn.send_command("focus_animation", {"animation_name": animation_name})

    def focus_widget(self, widget_name: str) -> Dict[str, Any]:
        return self.conn.send_command("focus_widget", {"widget_name": widget_name})

    def add_track(self, asset_path: Optional[str] = None, animation_name: Optional[str] = None, widget_name: Optional[str] = None, property_name: str = "") -> Dict[str, Any]:
        params = {"property_name": property_name}
        if asset_path: params["asset_path"] = asset_path
        if animation_name: params["animation_name"] = animation_name
        if widget_name: params["widget_name"] = widget_name
        return self.conn.send_command("add_track", params)

    def add_key(self, asset_path: Optional[str] = None, animation_name: Optional[str] = None, widget_name: Optional[str] = None, property_name: str = "", time: float = 0.0, value: float = 0.0) -> Dict[str, Any]:
        params = {
            "property_name": property_name,
            "time": time,
            "value": value
        }
        if asset_path: params["asset_path"] = asset_path
        if animation_name: params["animation_name"] = animation_name
        if widget_name: params["widget_name"] = widget_name
        return self.conn.send_command("add_key", params)
