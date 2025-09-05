# UMGGet.py (Refactored to Class-based structure)

import json
from typing import Dict, Any, List, Optional

class UMGGet:
    def __init__(self, client):
        self.client = client

    def get_widget_tree(self, asset_path: str) -> Dict[str, Any]:
        """Retrieves the full widget hierarchy for a UMG asset as a nested JSON object."""
        return self.client.execute_command("get_widget_tree", {"asset_path": asset_path})

    def query_widget_properties(self, widget_id: str, properties: List[str]) -> Dict[str, Any]:
        """Queries a list of specific properties from a single widget (e.g., 'Slot.Size')."""
        return self.client.execute_command("query_widget_properties", {"widget_id": widget_id, "properties": properties})

    def get_layout_data(self, asset_path: str, resolution_width: int = 1920, resolution_height: int = 1080) -> Dict[str, Any]:
        """Gets screen-space bounding boxes for all widgets at a given resolution."""
        resolution = {"width": resolution_width, "height": resolution_height}
        return self.client.execute_command("get_layout_data", {"asset_path": asset_path, "resolution": resolution})

    def check_widget_overlap(self, asset_path: str, widget_ids: Optional[List[str]] = None) -> Dict[str, Any]:
        """Checks for layout overlap between widgets."""
        params = {"asset_path": asset_path}
        if widget_ids:
            params["widget_ids"] = widget_ids
        return self.client.execute_command("check_widget_overlap", params)

    def get_asset_file_system_path(self, asset_path: str) -> Dict[str, Any]:
        """Converts an Unreal Engine asset path to an absolute file system path."""
        return self.client.execute_command("get_asset_file_system_path", {"asset_path": asset_path})