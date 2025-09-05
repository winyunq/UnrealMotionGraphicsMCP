# UMGAttention.py (Refactored to Class-based structure)

from typing import Dict, Any, Optional, List

class UMGAttention:
    def __init__(self, client):
        self.client = client

    def get_active_umg_context(self) -> Dict[str, Any]:
        """Gets the asset path of the UMG Editor the user is currently focused on."""
        return self.client.execute_command("get_active_umg_context")

    def get_last_edited_umg_asset(self) -> Dict[str, Any]:
        """Gets the asset path of the last UMG asset that was opened or saved."""
        return self.client.execute_command("get_last_edited_umg_asset")

    def get_recently_edited_umg_assets(self, max_count: int = 5) -> Dict[str, Any]:
        """Gets a list of recently edited UMG assets."""
        return self.client.execute_command("get_recently_edited_umg_assets", {"max_count": max_count})

    def is_umg_editor_active(self, asset_path: Optional[str] = None) -> Dict[str, Any]:
        """Checks if a UMG editor is the active window."""
        params = {"asset_path": asset_path} if asset_path else {}
        return self.client.execute_command("is_umg_editor_active", params)

    def set_attention_target(self, asset_path: str) -> Dict[str, Any]:
        """Sets the UMG asset that should be considered the current attention target."""
        return self.client.execute_command("set_attention_target", {"asset_path": asset_path})