# UmgComponentCatalog.py — semantic WBP component catalog client

from typing import Dict, Any, Optional


class UmgComponentCatalog:
    def __init__(self, client):
        self.client = client

    def catalog_list(self, root: str = "/Game/UI", recursive: bool = True) -> Dict[str, Any]:
        return self.client.send_command(
            "catalog_list",
            {"root": root, "recursive": recursive},
        )

    def catalog_describe(self, path: str) -> Dict[str, Any]:
        return self.client.send_command("catalog_describe", {"path": path})
