# umg_mcp_client.py
# The client-side library for the UE5-UMG-MCP project.
# This code will be used by an AI assistant (like Gemini) to communicate with the Unreal Engine plugin.

import requests
import json

class UMG_MCP_Client:
    """
    A client to interact with the UE5-UMG-MCP plugin running inside Unreal Engine.
    It sends commands via HTTP requests and receives data as JSON.
    """

    def __init__(self, host: str = "127.0.0.1", port: int = 54517):
        """
        Initializes the client with the host and port of the Unreal Engine web server.
        """
        self.base_url = f"http://{host}:{port}/umg-mcp"
        print(f"UMG-MCP Client initialized. Targeting server at: {self.base_url}")

    def _send_command(self, endpoint: str, payload: dict) -> dict:
        """
        A private helper method to send a command to the specified endpoint.
        Handles connection errors and JSON parsing.
        """
        url = f"{self.base_url}/{endpoint}"
        try:
            response = requests.post(url, json=payload, timeout=5) # 5 second timeout
            response.raise_for_status()
            return response.json()
        except requests.exceptions.RequestException as e:
            error_message = f"Failed to connect to UE5 plugin at {url}. Is the editor running and the plugin enabled? Details: {e}"
            return {"status": "error", "message": error_message}
        except json.JSONDecodeError:
            error_message = "Failed to decode JSON response from the server."
            return {"status": "error", "message": error_message}

    # Sensing APIs
    def get_widget_tree(self, asset_path: str) -> dict:
        """Asks UE to return the complete widget tree of a UMG asset as JSON."""
        print(f"CLIENT: Requesting widget tree for '{asset_path}'...")
        payload = {"asset_path": asset_path}
        return self._send_command("get_widget_tree", payload)

    def query_widget_properties(self, widget_id: str, properties: list[str]) -> dict:
        """Queries specific properties of a single widget."""
        print(f"CLIENT: Querying properties {properties} for widget '{widget_id}'...")
        payload = {"widget_id": widget_id, "properties": properties}
        return self._send_command("query_widget_properties", payload)

    def get_layout_data(self, asset_path: str, resolution: dict = {"width": 1920, "height": 1080}) -> dict:
        """Gets the screen-space bounding boxes for all widgets at a given resolution."""
        print(f"CLIENT: Getting layout data for '{asset_path}' at {resolution['width']}x{resolution['height']}...")
        payload = {"asset_path": asset_path, "resolution": resolution}
        return self._send_command("get_layout_data", payload)

    # Action APIs
    def create_widget(self, asset_path: str, parent_id: str, widget_type: str, widget_name: str, properties: dict = {}) -> dict:
        """Requests UE to create a new widget and attach it to a parent."""
        print(f"CLIENT: Creating widget '{widget_name}' of type '{widget_type}'...")
        payload = {
            "asset_path": asset_path,
            "parent_id": parent_id,
            "widget_type": widget_type,
            "widget_name": widget_name,
            "properties": properties
        }
        return self._send_command("create_widget", payload)

    def set_widget_properties(self, widget_id: str, properties: dict) -> dict:
        """Requests UE to change properties of an existing widget."""
        print(f"CLIENT: Setting properties for widget '{widget_id}'...")
        payload = {"widget_id": widget_id, "properties": properties}
        return self._send_command("set_widget_properties", payload)
    
    def delete_widget(self, widget_id: str) -> dict:
        """Requests UE to delete a widget."""
        print(f"CLIENT: Deleting widget '{widget_id}'...")
        payload = {"widget_id": widget_id}
        return self._send_command("delete_widget", payload)


if __name__ == "__main__":
    print("--- Running UMG-MCP Client Test ---")
    print("This will fail as the UE5 plugin is not running. This is the expected behavior.")
    
    client = UMG_MCP_Client()
    
    print("\n1. Testing 'get_widget_tree'...")
    tree_response = client.get_widget_tree(asset_path="/Game/UI/WBP_MainMenu")
    print("Response from server:", tree_response)

    print("\n2. Testing 'create_widget'...")
    create_response = client.create_widget(
        asset_path="/Game/UI/WBP_MainMenu",
        parent_id="CanvasPanel_0",
        widget_type="Button",
        widget_name="NewPlayButton",
        properties={"Slot.Size": [200, 50]}
    )
    print("Response from server:", create_response)
    
    print("\n--- Test Complete ---")