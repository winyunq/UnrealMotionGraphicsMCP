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

    def get_widget_properties(self, animation_name: str = "", widget_name: str = "", property_name: str = "") -> Dict[str, Any]:
        payload: Dict[str, Any] = {}
        if animation_name:
            payload["animation_name"] = animation_name
        if widget_name:
            payload["widget_name"] = widget_name
        if property_name:
            payload["property_name"] = property_name
        return self.conn.send_command("animation_widget_properties", payload)

    def get_time_properties(self, times: List[float], animation_name: str = "", widget_name: str = "", property_name: str = "") -> Dict[str, Any]:
        payload: Dict[str, Any] = {"times": times}
        if animation_name:
            payload["animation_name"] = animation_name
        if widget_name:
            payload["widget_name"] = widget_name
        if property_name:
            payload["property_name"] = property_name
        return self.conn.send_command("animation_time_properties", payload)

    def get_animation_overview(self, animation_name: str = "", widget_name: str = "", property_name: str = "") -> Dict[str, Any]:
        payload: Dict[str, Any] = {}
        if animation_name:
            payload["animation_name"] = animation_name
        if widget_name:
            payload["widget_name"] = widget_name
        if property_name:
            payload["property_name"] = property_name
        return self.conn.send_command("animation_overview", payload)

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

    def delete_animation(self, asset_path: Optional[str] = None, animation_name: str = "", confirm_delete: bool = False) -> Dict[str, Any]:
        params = {"animation_name": animation_name}
        params["confirm_delete"] = confirm_delete
        if asset_path:
            params["asset_path"] = asset_path
        return self.conn.send_command("delete_animation", params)

    def set_property_keys(self, property_name: str, keys: List[Dict[str, Any]]) -> Dict[str, Any]:
        return self.conn.send_command("set_property_keys", {
            "property_name": property_name,
            "keys": keys
        })

    def remove_property_track(self, property_name: str, confirm_delete: bool = False) -> Dict[str, Any]:
        return self.conn.send_command("remove_property_track", {"property_name": property_name, "confirm_delete": confirm_delete})

    def remove_keys(self, property_name: str, times: List[float], confirm_delete: bool = False) -> Dict[str, Any]:
        return self.conn.send_command("remove_keys", {
            "property_name": property_name,
            "times": times,
            "confirm_delete": confirm_delete
        })

    def append_widget_tracks(self, widget_name: str, tracks: List[Dict[str, Any]], animation_name: str = "") -> Dict[str, Any]:
        payload: Dict[str, Any] = {"widget_name": widget_name, "tracks": tracks}
        if animation_name:
            payload["animation_name"] = animation_name
        return self.conn.send_command("animation_append_widget_tracks", payload)

    def append_time_slice(self, time: float, widgets: List[Dict[str, Any]], animation_name: str = "") -> Dict[str, Any]:
        payload: Dict[str, Any] = {"time": time, "widgets": widgets}
        if animation_name:
            payload["animation_name"] = animation_name
        return self.conn.send_command("animation_append_time_slice", payload)

    def delete_widget_keys(self, property_name: str, times: List[float], widget_name: str = "", animation_name: str = "", confirm_delete: bool = False) -> Dict[str, Any]:
        payload: Dict[str, Any] = {
            "property_name": property_name,
            "times": times,
            "confirm_delete": confirm_delete
        }
        if widget_name:
            payload["widget_name"] = widget_name
        if animation_name:
            payload["animation_name"] = animation_name
        return self.conn.send_command("animation_delete_widget_keys", payload)
