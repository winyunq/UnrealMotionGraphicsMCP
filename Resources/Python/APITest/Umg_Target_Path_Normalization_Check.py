import ast
from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
SERVER_PATH = ROOT / "Resources/Python/UmgMcpServer.py"


def load_normalize_project_path():
    server_ast = ast.parse(SERVER_PATH.read_text(encoding="utf-8"))
    function_node = next(
        node
        for node in server_ast.body
        if isinstance(node, ast.FunctionDef) and node.name == "normalize_project_path"
    )
    module = ast.Module(body=[function_node], type_ignores=[])
    ast.fix_missing_locations(module)

    namespace = {}
    exec(compile(module, str(SERVER_PATH), "exec"), namespace)
    return namespace["normalize_project_path"]


def main() -> None:
    normalize_project_path = load_normalize_project_path()

    cases = {
        "/FogOfWar/NDC/WBP_MassBattleFrameMinimap_NDC_Runtime": "/FogOfWar/NDC/WBP_MassBattleFrameMinimap_NDC_Runtime",
        "/MountedPlugin/UI/WBP_PluginPanel": "/MountedPlugin/UI/WBP_PluginPanel",
        "/Game/UI/WBP_Main": "/Game/UI/WBP_Main",
        "Content/UI/WBP_Main": "/Game/UI/WBP_Main",
        "UI/WBP_Main": "/Game/UI/WBP_Main",
    }

    for raw_path, expected_path in cases.items():
        actual_path = normalize_project_path(raw_path)
        if actual_path != expected_path:
            raise AssertionError(f"{raw_path!r} normalized to {actual_path!r}, expected {expected_path!r}")

    try:
        normalize_project_path("../FogOfWar/WBP_Bad")
    except ValueError:
        pass
    else:
        raise AssertionError("Path traversal segments should be rejected.")

    print("UMG target path normalization check passed.")


if __name__ == "__main__":
    main()
