# UMGAttention.py (Refactored to Class-based structure)

from typing import Dict, Any, Optional, List

class UMGAttention:
    def __init__(self, client):
        self.client = client

    def get_target_umg_asset(self) -> Dict[str, Any]:
        """
        Gets the asset path of the current target UMG asset.
        If a target is explicitly set, it returns that target.
        Otherwise, it falls back to the last edited UMG asset.
        """
        return self.client.send_command("get_target_umg_asset")

    def get_last_edited_umg_asset(self) -> Dict[str, Any]:
        """Gets the asset path of the last UMG asset that was opened or saved."""
        return self.client.send_command("get_last_edited_umg_asset")

    def get_recently_edited_umg_assets(self, max_count: int = 5) -> Dict[str, Any]:
        """Gets a list of recently edited UMG assets."""
        return self.client.send_command("get_recently_edited_umg_assets", {"max_count": max_count})

    def set_target_umg_asset(self, asset_path: str) -> Dict[str, Any]:
        """
        Sets the UMG asset that should be considered the current attention target.
        This allows programmatically setting the active UMG context.
        """
        return self.client.send_command("set_target_umg_asset", {"asset_path": asset_path})
