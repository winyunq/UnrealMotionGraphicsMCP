# UMGGet.py

import json
from typing import Dict, Any, List, Optional

# Assuming get_unreal_connection is available from UmgMcpServer.py or a similar core client file.
# This would typically be imported as:
# from UmgMcpServer import get_unreal_connection

# ==============================================================================
#  Category: Sensing & Insight (from UmgGetSubsystem)
#  AI Hint: Use these tools to understand the details of UMG assets.
# ==============================================================================

def get_widget_tree(asset_path: str) -> Dict[str, Any]:
    """Retrieves the full widget hierarchy for a UMG asset as a nested JSON object."""
    unreal = get_unreal_connection()
    return unreal.send_command("get_widget_tree", {"asset_path": asset_path})

def query_widget_properties(widget_id: str, properties: List[str]) -> Dict[str, Any]:
    """Queries a list of specific properties from a single widget (e.g., 'Slot.Size')."""
    unreal = get_unreal_connection()
    return unreal.send_command("query_widget_properties", {"widget_id": widget_id, "properties": properties})

def get_layout_data(asset_path: str, resolution_width: int = 1920, resolution_height: int = 1080) -> Dict[str, Any]:
    """Gets screen-space bounding boxes for all widgets at a given resolution."""
    unreal = get_unreal_connection()
    resolution = {"width": resolution_width, "height": resolution_height} # Match C++ struct
    return unreal.send_command("get_layout_data", {"asset_path": asset_path, "resolution": resolution})

def check_widget_overlap(asset_path: str, widget_ids: Optional[List[str]] = None) -> Dict[str, Any]:
    """
    "Are any widgets overlapping?" - Efficiently checks for layout overlap between widgets.
    AI HINT: PREFER this server-side check over fetching all layout data and calculating it yourself. It's much faster.
    """
    unreal = get_unreal_connection()
    params = {"asset_path": asset_path}
    if widget_ids: # Only add if not None and not empty
        params["widget_ids"] = widget_ids
    return unreal.send_command("check_widget_overlap", params)

def get_asset_file_system_path(asset_path: str) -> Dict[str, Any]:
    """
    Converts an Unreal Engine asset path to an absolute file system path.
    """
    unreal = get_unreal_connection()
    return unreal.send_command("get_asset_file_system_path", {"asset_path": asset_path})
