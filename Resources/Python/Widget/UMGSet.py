# UMGSet.py

import json
from typing import Dict, Any, List, Optional, Union

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

    def delete_widget(self, widget_name: str) -> Dict[str, Any]:
        """Deletes a widget from the UMG asset."""
        params = {"widget_name": widget_name}
        return self.client.send_command("delete_widget", params)

    def move_widget(self, widget_name: str, target: str) -> Dict[str, Any]:
        """Moves a widget under a different parent panel."""
        params = {"widget_name": widget_name, "target": target}
        return self.client.send_command("move_widget", params)

    def reparent_widget(
        self,
        widget_name: str,
        new_parent_widget: Optional[Dict[str, Any]] = None,
        new_parent_name: Optional[str] = None,
    ) -> Dict[str, Any]:
        """
        Converts/reparents a widget using the C++ new_parent_widget JSON spec.
        For simple parent moves, pass new_parent_name and it will call move_widget.
        """
        if new_parent_name and not new_parent_widget:
            return self.move_widget(widget_name, new_parent_name)

        if not new_parent_widget:
            return {
                "success": False,
                "error": "reparent_widget requires new_parent_widget JSON or new_parent_name for move_widget.",
            }

        params = {"widget_name": widget_name, "new_parent_widget": new_parent_widget}
        return self.client.send_command("reparent_widget", params)

    def save_asset(self) -> Dict[str, Any]:
        """Saves the UMG asset."""
        return self.client.send_command("save_asset", {})
