import logging
from typing import Dict, Any, List, Optional

logger = logging.getLogger("UmgMcpServerV3")

class UMGSequencer:
    def __init__(self, connection):
        self.conn = connection

    # =============================================================================
    #  Attention (Context)
    # =============================================================================

    def set_animation_scope(self, animation_name: str) -> Dict[str, Any]:
        return self.conn.send_command("set_animation_scope", {"animation_name": animation_name})

    def set_widget_scope(self, widget_name: str) -> Dict[str, Any]:
        return self.conn.send_command("set_widget_scope", {"widget_name": widget_name})

    # =============================================================================
    #  Read (Sensing)
    # =============================================================================

    def get_all_animations(self) -> Dict[str, Any]:
        # Note: asset_path is now handled implicitly by the Engine context
        return self.conn.send_command("get_all_animations", {})

    def get_animation_keyframes(self, animation_name: str) -> Dict[str, Any]:
        return self.conn.send_command("get_animation_keyframes", {"animation_name": animation_name})

    def get_animated_widgets(self, animation_name: str) -> Dict[str, Any]:
        return self.conn.send_command("get_animated_widgets", {"animation_name": animation_name})

    def get_animation_full_data(self, animation_name: str) -> Dict[str, Any]:
        return self.conn.send_command("get_animation_full_data", {"animation_name": animation_name})

    def get_widget_animation_data(self, animation_name: str, widget_name: str) -> Dict[str, Any]:
        return self.conn.send_command("get_widget_animation_data", {
            "animation_name": animation_name,
            "widget_name": widget_name
        })

    # =============================================================================
    #  Write (Action)
    # =============================================================================

    def create_animation(self, asset_path: Optional[str] = None, animation_name: str = "") -> Dict[str, Any]:
        # Note: asset_path is kept optional for backward compatibility in signature, 
        # but the server implementation relies on context.
        params = {"animation_name": animation_name}
        if asset_path:
            params["asset_path"] = asset_path
        return self.conn.send_command("create_animation", params)

    def delete_animation(self, asset_path: Optional[str] = None, animation_name: str = "") -> Dict[str, Any]:
        params = {"animation_name": animation_name}
        if asset_path:
            params["asset_path"] = asset_path
        return self.conn.send_command("delete_animation", params)

    def set_property_keys(self, property_name: str, keys: List[Dict[str, Any]]) -> Dict[str, Any]:
        return self.conn.send_command("set_property_keys", {
            "property_name": property_name,
            "keys": keys
        })

    def remove_property_track(self, property_name: str) -> Dict[str, Any]:
        return self.conn.send_command("remove_property_track", {"property_name": property_name})

    def remove_keys(self, property_name: str, times: List[float]) -> Dict[str, Any]:
        return self.conn.send_command("remove_keys", {
            "property_name": property_name,
            "times": times
        })
