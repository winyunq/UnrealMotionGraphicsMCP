'''
Handles UMG asset transformations, such as exporting to JSON and applying JSON back to a UMG asset.
'''
import json
from typing import Dict, Any, List, Optional

class UMGFileTransformation:
    def __init__(self, client):
        """Initializes the client for sending commands to Unreal Engine."""
        self.client = client

    def export_umg_to_json(self, asset_path: str) -> Dict[str, Any]:
        """
        'Decompiles' a UMG .uasset file into a JSON object.

        Args:
            asset_path: The asset path of the UMG widget to export.

        Returns:
            A dictionary containing the result of the operation.
        """
        return self.client.send_command('export_umg_to_json', {'asset_path': asset_path})

    def apply_json_to_umg(self, asset_path: str, json_data: Dict[str, Any]) -> Dict[str, Any]:
        """
        'Compiles' a JSON object into a UMG .uasset file, creating or overwriting it.

        Args:
            asset_path: The asset path of the UMG widget to apply the JSON to.
            json_data: The dictionary representing the UMG data.

        Returns:
            A dictionary containing the result of the operation.
        """
        # The MCP framework deserializes the incoming JSON argument into a Python dict.
        # We must re-serialize it back into a JSON string for the C++ backend.
        json_string_for_cpp = json.dumps(json_data)
        return self.client.send_command('apply_json_to_umg', {'asset_path': asset_path, 'json_data': json_string_for_cpp})