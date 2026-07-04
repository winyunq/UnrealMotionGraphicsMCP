import asyncio
import json
import os
import sys

current_dir = os.path.dirname(os.path.abspath(__file__))
parent_dir = os.path.dirname(current_dir)
sys.path.append(parent_dir)

from mcp_config import UNREAL_HOST, UNREAL_PORT


class SimpleConnection:
    async def send_command(self, command: str, params: dict = None) -> dict:
        reader = None
        writer = None
        try:
            reader, writer = await asyncio.open_connection(UNREAL_HOST, UNREAL_PORT)
            payload = {"command": command, "params": params or {}}
            data = json.dumps(payload).encode("utf-8")
            writer.write(data + b"\0")
            await writer.drain()

            chunks = []
            while True:
                chunk = await reader.read(4096)
                if not chunk:
                    break
                if b"\0" in chunk:
                    chunks.append(chunk[:chunk.find(b"\0")])
                    break
                chunks.append(chunk)

            response_data = b"".join(chunks).decode("utf-8")
            return json.loads(response_data)
        except Exception as exc:
            return {"success": False, "error": str(exc)}
        finally:
            if writer:
                writer.close()
                await writer.wait_closed()


async def call(conn: SimpleConnection, command: str, params: dict = None) -> dict:
    response = await conn.send_command(command, params)
    print(command, json.dumps(response, ensure_ascii=False, indent=2))
    if response.get("success") is False or response.get("status") == "error":
        raise RuntimeError(response.get("error", f"{command} failed"))
    return response


async def run_demo():
    conn = SimpleConnection()

    await call(conn, "hlsl_set_target", {
        "path": "/Game/UMGMCP/Demos/M_Hlsl_UI",
        "create_if_not_found": True
    })
    await call(conn, "hlsl_set", {
        "hlsl": "return float4(UV.x, UV.y, 1.0, 0.85);",
        "parameters": []
    })
    await call(conn, "hlsl_compile")

    await call(conn, "hlsl_set_target", {
        "path": "/Game/UMGMCP/Demos/M_Hlsl_Surface",
        "create_if_not_found": True,
        "domain": "surface",
        "shading_model": "unlit",
        "blend_mode": "opaque"
    })
    await call(conn, "hlsl_get")
    await call(conn, "hlsl_set", {
        "hlsl": "\n".join([
            "float glow = saturate(Intensity);",
            "Roughness = 0.28;",
            "Metallic = 0.0;",
            "Normal = normalize(float3(0.0, 0.0, 1.0));",
            "return float4((0.05 + UV.x * 0.7) * glow, 0.2, 1.0 - UV.y * 0.6, 1.0);"
        ]),
        "parameters": [{"name": "Intensity", "kind": "Scalar"}],
        "outputs": ["Roughness", "Metallic", "Normal"]
    })
    await call(conn, "hlsl_compile")


if __name__ == "__main__":
    asyncio.run(run_demo())
