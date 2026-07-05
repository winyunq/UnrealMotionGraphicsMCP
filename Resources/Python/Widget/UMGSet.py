# UMGSet.py

import json
from typing import Dict, Any, List, Optional

class UMGSet:
    def __init__(self, client):
        self.client = client

    def set_widget_properties(self, widget_name: str, properties: Dict[str, Any]) -> Dict[str, Any]:
        """Sets one or more properties on a specific widget."""
        params = {"widget_name": widget_name, "properties": properties}
        return self.client.send_command("set_widget_properties", params)

    def create_widget(self, widget_type: str, widget_name: str, parent_name: str = "") -> Dict[str, Any]:
        """Creates a new widget and attaches it to a parent."""
        params = {
            "widget_type": widget_type,
            "new_widget_name": widget_name,
            "parent_name": parent_name  # Always send parent_name, even if empty
        }
        return self.client.send_command("create_widget", params)

    def delete_widget(self, widget_name: str, confirm_delete: bool = False) -> Dict[str, Any]:
        """Deletes a widget from the UMG asset."""
        params = {"widget_name": widget_name, "confirm_delete": confirm_delete}
        return self.client.send_command("delete_widget", params)

    def reorder_widget_tree(self, tree: Any, root: str = "") -> Dict[str, Any]:
        """Reorders existing widgets using a partial tree/order specification."""
        params = {"tree": tree}
        if root:
            params["root"] = root
        return self.client.send_command("reorder_widget_tree", params)

    def reparent_widget(self, widget_name: str, new_parent_widget: Any) -> Dict[str, Any]:
        """Compatibility structural replacement/wrap command."""
        params = {"widget_name": widget_name, "new_parent_widget": new_parent_widget}
        return self.client.send_command("reparent_widget", params)

    def save_asset(self) -> Dict[str, Any]:
        """Saves the UMG asset."""
        return self.client.send_command("save_asset", {})
