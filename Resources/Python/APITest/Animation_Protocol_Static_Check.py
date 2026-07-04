import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]


def require_text(path: str, needle: str) -> None:
    text = (ROOT / path).read_text(encoding="utf-8")
    if needle not in text:
        raise AssertionError(f"{needle!r} not found in {path}")


def main() -> None:
    prompts = json.loads((ROOT / "Resources/prompts.json").read_text(encoding="utf-8"))
    tools = {tool["name"]: tool for tool in prompts.get("tools", [])}
    enabled_tool_names = {name for name, tool in tools.items() if tool.get("enabled", True)}
    animation_mode = json.loads((ROOT / "Resources/ToolModes/Animation.json").read_text(encoding="utf-8"))
    allowed_tools = set(animation_mode.get("allowed_tools", []))

    required_tools = {
        "set_animation_scope",
        "set_widget_scope",
        "get_all_animations",
        "animation_overview",
        "animation_widget_properties",
        "animation_time_properties",
        "create_animation",
        "delete_animation",
        "animation_append_widget_tracks",
        "animation_append_time_slice",
        "animation_delete_widget_keys",
    }
    support_tools = {
        "set_target_umg_asset",
        "get_target_umg_asset",
        "set_target_widget",
        "get_target_widget",
        "save_asset",
    }

    compatibility_tools = {
        "get_animation_keyframes",
        "get_animated_widgets",
        "get_animation_full_data",
        "get_widget_animation_data",
        "set_property_keys",
        "set_animation_data",
        "remove_property_track",
        "remove_keys",
    }

    missing_prompts = required_tools - enabled_tool_names
    missing_mode = (required_tools | support_tools) - allowed_tools
    if missing_prompts:
        raise AssertionError(f"Missing prompt tools: {sorted(missing_prompts)}")
    if missing_mode:
        raise AssertionError(f"Missing Animation mode tools: {sorted(missing_mode)}")

    extra_mode = allowed_tools - required_tools - support_tools
    if extra_mode:
        raise AssertionError(f"Animation mode exposes non-default tools: {sorted(extra_mode)}")

    for name in sorted(compatibility_tools):
        tool = tools.get(name)
        if tool is None:
            raise AssertionError(f"Compatibility tool missing from prompts config: {name}")
        if tool.get("enabled", True):
            raise AssertionError(f"Compatibility tool should be disabled in prompts: {name}")
        if name in allowed_tools:
            raise AssertionError(f"Compatibility tool should not be in Animation mode: {name}")

    require_text("Document/AnimationMcpProtocol.md", "animation_append_widget_tracks")
    require_text("Document/AnimationMcpProtocol.md", "confirm_delete=true")
    require_text("README.md", "Legacy low-level reads/writes")
    require_text("Readme_zh.md", "旧的底层读写命令")
    require_text("Resources/Python/UmgMcpServer.py", "async def delete_animation")
    require_text("Resources/Python/UmgMcpServer.py", "requires confirm_delete=true")
    require_text("Source/UmgMcp/Private/Animation/UmgMcpSequencerCommands.cpp", "Deletion hardened (Issue 15)")
    require_text("Source/UmgMcp/Private/Animation/UmgMcpSequencerCommands.cpp", "SetPlaybackRange")

    print("Animation protocol static check passed.")


if __name__ == "__main__":
    main()
