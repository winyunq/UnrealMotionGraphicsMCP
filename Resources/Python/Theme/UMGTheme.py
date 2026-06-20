# UMGTheme.py — Design token / theme API client

from typing import Dict, Any, Optional, List, Union


def _navigate_theme_path(theme: Dict[str, Any], dot_path: str) -> Any:
    current: Any = theme
    for segment in dot_path.split("."):
        if not segment or not isinstance(current, dict) or segment not in current:
            return None
        current = current[segment]
    return current


def resolve_token_in_theme(theme: Dict[str, Any], token_ref: str) -> Any:
    ref = (token_ref or "").strip()
    if ref.startswith("@"):
        ref = ref[1:]
    if not ref:
        return None
    return _navigate_theme_path(theme, ref)


def resolve_tokens_deep(theme: Dict[str, Any], value: Any) -> Any:
    if isinstance(value, str) and value.startswith("@"):
        resolved = resolve_token_in_theme(theme, value)
        return value if resolved is None else resolved
    if isinstance(value, dict):
        return {key: resolve_tokens_deep(theme, item) for key, item in value.items()}
    if isinstance(value, list):
        return [resolve_tokens_deep(theme, item) for item in value]
    return value


class UMGTheme:
    def __init__(self, client):
        self.client = client

    def theme_get(self, theme_path: Optional[str] = None) -> Dict[str, Any]:
        params: Dict[str, Any] = {}
        if theme_path:
            params["theme_path"] = theme_path
        return self.client.send_command("theme_get", params)

    def theme_apply(self, patch: Dict[str, Any], theme_path: Optional[str] = None) -> Dict[str, Any]:
        params: Dict[str, Any] = {"patch": patch}
        if theme_path:
            params["theme_path"] = theme_path
        return self.client.send_command("theme_apply", params)

    def theme_resolve_token(self, token: str) -> Dict[str, Any]:
        return self.client.send_command("theme_resolve_token", {"token": token})

    @staticmethod
    def extract_theme_dict(theme_get_response: Dict[str, Any]) -> Dict[str, Any]:
        if not isinstance(theme_get_response, dict):
            return {}
        theme = theme_get_response.get("theme")
        if isinstance(theme, dict):
            return theme
        result = theme_get_response.get("result")
        if isinstance(result, dict) and isinstance(result.get("theme"), dict):
            return result["theme"]
        return {}

    def resolve_tokens_in_layout(self, layout_json: Union[Dict[str, Any], List[Any]], theme: Optional[Dict[str, Any]] = None) -> Union[Dict[str, Any], List[Any]]:
        theme_dict = theme or {}
        return resolve_tokens_deep(theme_dict, layout_json)
