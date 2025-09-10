# UMGSet.py

import json
from typing import Dict, Any, List

class UMGSet:
    def __init__(self, client):
        self.client = client

    def set_widget_properties(self, asset_path: str, widget_name: str, properties: Dict[str, Any]) -> Dict[str, Any]:
        """Sets one or more properties on a specific widget."""
        properties_json_string = json.dumps(properties)
        params = {"asset_path": asset_path, "widget_name": widget_name, "properties_json": properties_json_string}
        return self.client.send_command("set_widget_properties", params)

    def create_widget(self, asset_path: str, parent_name: str, widget_type: str, widget_name: str) -> Dict[str, Any]:
        """Creates a new widget and attaches it to a parent."""
        params = {
            "asset_path": asset_path,
            "parent_name": parent_name,
            "widget_type": widget_type,
            "widget_name": widget_name
        }
        return self.client.send_command("create_widget", params)

    def delete_widget(self, asset_path: str, widget_name: str) -> Dict[str, Any]:
        """Deletes a widget from the UMG asset."""
        params = {"asset_path": asset_path, "widget_name": widget_name}
        return self.client.send_command("delete_widget", params)

    def reparent_widget(self, asset_path: str, widget_name: str, new_parent_name: str) -> Dict[str, Any]:
        """Moves a widget to be a child of a different parent."""
        params = {"asset_path": asset_path, "widget_name": widget_name, "new_parent_name": new_parent_name}
        return self.client.send_command("reparent_widget", params)