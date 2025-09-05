# UMGSet.py

import json
from typing import Dict, Any, List

# This file provides Python functions that correspond to the UUmgSetSubsystem in C++.
# It assumes a client object with an execute_command method is available.

class UMGSet:
    def __init__(self, client):
        self.client = client

    def set_widget_properties(self, widget_id: str, properties: Dict[str, Any]) -> Dict[str, Any]:
        """Sets one or more properties on a specific widget."""
        # The C++ backend expects the properties dictionary to be a JSON string.
        properties_json_string = json.dumps(properties)
        params = {"widget_id": widget_id, "properties_json": properties_json_string}
        return self.client.execute_command("set_widget_properties", params)

    def create_widget(self, asset_path: str, parent_id: str, widget_type: str, widget_name: str) -> Dict[str, Any]:
        """Creates a new widget and attaches it to a parent."""
        params = {
            "asset_path": asset_path,
            "parent_id": parent_id,
            "widget_type": widget_type,
            "widget_name": widget_name
        }
        return self.client.execute_command("create_widget", params)

    def delete_widget(self, widget_id: str) -> Dict[str, Any]:
        """Deletes a widget from the UMG asset."""
        return self.client.execute_command("delete_widget", {"widget_id": widget_id})

    def reparent_widget(self, widget_id: str, new_parent_id: str) -> Dict[str, Any]:
        """Moves a widget to be a child of a different parent."""
        params = {"widget_id": widget_id, "new_parent_id": new_parent_id}
        return self.client.execute_command("reparent_widget", params)
