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
    material_mode = json.loads((ROOT / "Resources/ToolModes/Material.json").read_text(encoding="utf-8"))
    allowed_tools = set(material_mode.get("allowed_tools", []))

    required_tools = {
        "hlsl_set_target",
        "hlsl_get",
        "hlsl_set",
        "hlsl_delete",
        "hlsl_compile",
    }

    disabled_aliases = {
        "material_modify_type",
        "hlsl_delete_parameter",
        "hlsl_delete_output",
    }

    missing_prompts = required_tools - enabled_tool_names
    missing_mode = required_tools - allowed_tools
    if missing_prompts:
        raise AssertionError(f"Missing prompts tools: {sorted(missing_prompts)}")
    if missing_mode:
        raise AssertionError(f"Missing Material mode tools: {sorted(missing_mode)}")
    for alias in disabled_aliases:
        if tools.get(alias, {}).get("enabled", True):
            raise AssertionError(f"Compatibility alias should be disabled in prompts: {alias}")
        if alias in allowed_tools:
            raise AssertionError(f"Compatibility alias should not be in Material mode: {alias}")

    require_text("Source/UmgMcp/Private/Material/UmgMcpMaterialCommands.cpp", "CommandType == TEXT(\"hlsl_delete\")")
    require_text("Source/UmgMcp/Private/Material/UmgMcpMaterialCommands.cpp", "HasMaterialTypeOptions")
    require_text("Source/UmgMcp/Private/Material/UmgMcpMaterialCommands.cpp", "ApplyHlslOutputs")
    require_text("Source/UmgMcp/Private/Material/UmgMcpMaterialCommands.cpp", "AdditionalOutputs")
    require_text("Source/UmgMcp/Private/Material/UmgMcpMaterialSubsystem.cpp", "ResolveExpressionOutputIndex")
    require_text("Resources/Python/UmgMcpServer.py", "async def hlsl_delete")
    require_text("Resources/Python/Material/UMGMaterial.py", "async def hlsl_delete")
    require_text("Document/MaterialMcpProtocol.md", "outputs")

    print("Material protocol static check passed.")


if __name__ == "__main__":
    main()
