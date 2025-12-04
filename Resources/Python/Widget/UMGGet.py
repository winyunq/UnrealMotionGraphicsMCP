# UMGGet.py (Refactored for v3.0 Context-Aware Server)

import json
from typing import Dict, Any, List, Optional

class UMGGet:
    def __init__(self, client):
        self.client = client

    # --- Introspection ---
    def get_widget_schema(self, widget_type: str) -> Dict[str, Any]:
        """Retrieves the schema for a given widget type."""
        return self.client.send_command("get_widget_schema", {"widget_type": widget_type})

    def get_creatable_widget_types(self) -> Dict[str, Any]:
        """Returns a list of all creatable widget class names."""
        return self.client.send_command("get_creatable_widget_types")

    # --- Sensing ---
    def get_widget_tree(self) -> Dict[str, Any]:
        """Retrieves the full widget hierarchy for a UMG asset as a nested JSON object."""
        return self.client.send_command("get_widget_tree", {})

    def query_widget_properties(self, widget_name: str, properties: List[str]) -> Dict[str, Any]:
        """Queries a list of specific properties from a single widget by its name."""
        params = {"widget_name": widget_name, "properties": properties}
        return self.client.send_command("query_widget_properties", params)

    def get_layout_data(self, resolution_width: int = 1920, resolution_height: int = 1080) -> Dict[str, Any]:
        """Gets screen-space bounding boxes for all widgets at a given resolution."""
        resolution = {"width": resolution_width, "height": resolution_height}
        return self.client.send_command("get_layout_data", {"resolution": resolution})

    def check_widget_overlap(self, widget_names: Optional[List[str]] = None) -> Dict[str, Any]:
        """Checks for layout overlap between specified widgets by name."""
        params = {}
        if widget_names:
            params["widget_names"] = widget_names
        return self.client.send_command("check_widget_overlap", params)