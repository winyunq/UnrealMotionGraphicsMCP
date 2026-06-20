# UMGStorybook.py - Isolated widget preview / screenshot tools

from typing import Dict, Any, List, Optional


class UMGStorybook:
    def __init__(self, client):
        self.client = client

    def render_widget_preview(
        self,
        asset_path: Optional[str] = None,
        widget_name: Optional[str] = None,
        viewport_w: int = 400,
        viewport_h: int = 300,
        theme: Optional[str] = None,
    ) -> Dict[str, Any]:
        params: Dict[str, Any] = {
            "viewport_w": viewport_w,
            "viewport_h": viewport_h,
        }
        if asset_path:
            params["asset_path"] = asset_path
        if widget_name:
            params["widget_name"] = widget_name
        if theme:
            params["theme"] = theme
        return self.client.send_command("render_widget_preview", params)

    def storybook_list_variants(
        self,
        asset_path: Optional[str] = None,
        parent_widget: Optional[str] = None,
        catalog_component_id: Optional[str] = None,
    ) -> Dict[str, Any]:
        params: Dict[str, Any] = {}
        if asset_path:
            params["asset_path"] = asset_path
        if parent_widget:
            params["parent_widget"] = parent_widget
        if catalog_component_id:
            params["catalog_component_id"] = catalog_component_id
        return self.client.send_command("storybook_list_variants", params)

    def storybook_render(
        self,
        asset_path: Optional[str] = None,
        widget_names: Optional[List[str]] = None,
        viewport_w: int = 400,
        viewport_h: int = 300,
        theme: Optional[str] = None,
    ) -> Dict[str, Any]:
        params: Dict[str, Any] = {
            "viewport_w": viewport_w,
            "viewport_h": viewport_h,
        }
        if asset_path:
            params["asset_path"] = asset_path
        if widget_names:
            params["widget_names"] = widget_names
        if theme:
            params["theme"] = theme
        return self.client.send_command("storybook_render", params)
