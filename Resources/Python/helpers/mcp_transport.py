import hashlib
import json
from typing import Any, Dict, Union


async def read_until_null_delimiter(reader, chunk_size: int = 4096) -> bytes:
    """Read from an async stream until the first null delimiter or EOF."""
    chunks = []
    while True:
        chunk = await reader.read(chunk_size)
        if not chunk:
            break
        if b"\x00" in chunk:
            chunks.append(chunk[: chunk.find(b"\x00")])
            break
        chunks.append(chunk)
    return b"".join(chunks)


def _redact_image_field(container: Dict[str, Any], field_name: str = "image_base64") -> None:
    value = container.get(field_name)
    if not isinstance(value, str) or not value:
        return

    encoded = value.encode("utf-8")
    container.pop(field_name, None)
    container["image_base64_bytes"] = len(encoded)
    container["image_base64_sha256"] = hashlib.sha256(encoded).hexdigest()[:16]


def summarize_response_for_log(payload: Union[str, Dict[str, Any]]) -> str:
    """Return a compact log-safe summary that never includes raw base64 image data."""
    if isinstance(payload, str):
        try:
            data = json.loads(payload)
        except json.JSONDecodeError:
            return f"<unparsed response, {len(payload)} chars>"
    else:
        data = payload

    if not isinstance(data, dict):
        return str(data)

    summary: Dict[str, Any] = dict(data)
    _redact_image_field(summary)

    renders = summary.get("renders")
    if isinstance(renders, list):
        redacted_renders = []
        for item in renders:
            if isinstance(item, dict):
                item_copy = dict(item)
                _redact_image_field(item_copy)
                redacted_renders.append(item_copy)
            else:
                redacted_renders.append(item)
        summary["renders"] = redacted_renders

    return json.dumps(summary, ensure_ascii=False)
