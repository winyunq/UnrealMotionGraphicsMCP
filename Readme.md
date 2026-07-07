[中文版](Readme_zh.md)

<div align="center">
  <h1>UnrealMotionGraphicsMCP</h1>
  <p><strong>Open-source MCP tools for AI-assisted Unreal Engine UMG authoring.</strong></p>
  <p>
    <a href="https://winyunq.github.io/UnrealMotionGraphicsMCP/">Web Manual</a>
    · <a href="#quick-start">Quick Start</a>
    · <a href="#protocol-reference">Protocol Reference</a>
    · <a href="https://github.com/winyunq/UnrealMotionGraphicsMCP/issues">Issues</a>
    · <a href="https://discord.gg/yk5VEQ5S9U">Discord</a>
  </p>
  <p>
    <img alt="License MIT" src="https://img.shields.io/badge/License-MIT-yellow.svg">
    <img alt="Unreal Engine 5.8" src="https://img.shields.io/badge/UE-5.8-0E1128.svg?logo=unrealengine">
    <img alt="Platform Win64" src="https://img.shields.io/badge/Platform-Win64-2563EB.svg">
    <a href="https://agentseal.org/mcp/https-githubcom-winyunq-unrealmotiongraphicsmcp"><img alt="AgentSeal MCP" src="https://agentseal.org/api/v1/mcp/https-githubcom-winyunq-unrealmotiongraphicsmcp/badge"></a>
  </p>
</div>

---

**UnrealMotionGraphicsMCP** makes Unreal UMG editing accessible to AI agents through explicit, versionable MCP commands. Instead of asking an agent to guess from screenshots or click through the editor, this plugin exposes the real editor state: widget trees, slot properties, Blueprint graph operations, UI materials, animation tracks, and JSON round-trips.

The open-source plugin is the protocol and editor bridge. It is meant to be readable, debuggable, and useful for research, custom pipelines, issue reports, and community fixes.

## What This Project Does

| Area | Open-source capability |
| --- | --- |
| UMG widgets | Create widgets, inspect trees, set properties and slots, reorder children, replace subtrees, export and apply JSON |
| Blueprint / BlueCode | Create graph nodes, wire pins, bind widget references, and apply compact code-like graph statements |
| Materials | Create UI materials, set parameters, add supported expressions, and test HLSL-oriented material workflows |
| Animation | Create UMG animation tracks and keyframes for render transform, opacity, color, and timing |
| Editor bridge | Let external MCP clients call Unreal Editor through a local loopback bridge on `127.0.0.1:55557` |

## Why It Exists

AI-assisted UI work needs more than screenshots. UMG assets are structured editor objects, and serious automation needs structured access. This project gives agents a protocol surface they can inspect, test, and improve without hiding behavior behind a black-box UI.

The core design goals are:

- **Observable edits**: Every meaningful UI change should be expressible as a tool call and reviewable as data.
- **Versionable workflows**: UMG can be exported, patched, and reapplied as structured JSON.
- **Editor-native behavior**: Commands operate through Unreal editor systems instead of browser-style click automation.
- **Open protocol first**: The repository keeps the protocol, schemas, tests, and bug fixes public.

## Quick Start

Current target: **Unreal Engine 5.8 on Win64**. The plugin descriptor is set to `EngineVersion: 5.8.0`.

### 1. Install The Plugin

Clone the open-source plugin into your Unreal project:

```powershell
cd D:\UE5Project\YourProject
mkdir Plugins
cd Plugins
git clone https://github.com/winyunq/UnrealMotionGraphicsMCP.git UmgMcp
```

Open the project in Unreal Engine 5.8, enable `UmgMcp` if needed, and restart the editor so Unreal Build Tool can compile the plugin.

### 2. Connect An MCP Client

Start Unreal Editor first. The plugin listens locally on `127.0.0.1:55557`; the Python MCP server forwards tool calls from your MCP client to that editor bridge.

Add this to your MCP client configuration, replacing the path with the absolute path to your project:

```json
{
  "mcpServers": {
    "UmgMcp": {
      "command": "uv",
      "args": [
        "run",
        "--directory",
        "D:\\UE5Project\\YourProject\\Plugins\\UmgMcp\\Resources\\Python",
        "UmgMcpServer.py"
      ]
    }
  }
}
```

Important details:

- The `--directory` value must point to `Resources/Python`.
- Windows JSON paths need escaped backslashes, for example `D:\\UE5Project\\...`.
- Keep Unreal Editor open while using the MCP client.

### 3. Verify The Setup

Run protocol checks without opening a large test scenario:

```powershell
cd D:\UE5Project\YourProject\Plugins\UmgMcp\Resources\Python
uv run python APITest\Umg_Widget_Protocol_Static_Check.py
uv run python APITest\Blueprint_MCP_Schema_Check.py
uv run python APITest\Material_Protocol_Static_Check.py
uv run python APITest\Animation_Protocol_Static_Check.py
```

Run an editor bridge smoke test after Unreal Editor is open:

```powershell
uv run python APITest\UE5_Editor_Imitation.py
```

If you want a manual virtual environment:

```powershell
cd D:\UE5Project\YourProject\Plugins\UmgMcp\Resources\Python
uv venv
.\.venv\Scripts\activate
uv pip install "mcp[cli]>=1.4.1" fastmcp uvicorn fastapi "pydantic>=2.6.1" requests
python -c "import mcp, fastapi, pydantic, requests; print('deps ok')"
```

## Documentation

The documentation site is the best place for guided setup and usage:

- [Web Manual](https://winyunq.github.io/UnrealMotionGraphicsMCP/)
- [Chinese Web Manual](https://winyunq.github.io/UnrealMotionGraphicsMCP/Document/index_zh.html)

The repository keeps protocol references close to the code:

| Topic | File |
| --- | --- |
| Widget commands and JSON round-trip | [Document/UmgWidgetMcpProtocol.md](Document/UmgWidgetMcpProtocol.md) |
| Blueprint / BlueCode graph operations | [Document/BlueprintBluecodeProtocol.md](Document/BlueprintBluecodeProtocol.md) |
| Material and HLSL-oriented operations | [Document/MaterialMcpProtocol.md](Document/MaterialMcpProtocol.md) |
| Animation / Sequencer operations | [Document/AnimationMcpProtocol.md](Document/AnimationMcpProtocol.md) |

## Example Agent Request

```text
Create /Game/UI/WBP_LoginPanel with a centered login card, email and password fields,
a primary submit button, and a subtle material background. Export the final widget JSON
and summarize which MCP tools changed the asset.
```

## Demos

- [Designing an RTS UI](https://youtu.be/O86VCzxyF5o)
- [Recreating the UE5 editor](https://youtu.be/h_J70I0m4Ls)
- [Editing UMG inside the UMG editor](https://youtu.be/pq12x2MH1L4)
- [Chatting with Gemini to edit a UMG file](https://youtu.be/93_Fiil9nd8)

## Troubleshooting

| Symptom | Fix |
| --- | --- |
| `ConnectionRefusedError`, `WinError 10061`, or `WinError 1225` | Open Unreal Editor first, enable `UmgMcp`, restart the editor, then reconnect the MCP client. |
| `uv` is not recognized | Install `uv`, restart the terminal, or create the manual virtual environment shown above. |
| Plugin does not compile | Confirm the project is UE 5.8 Win64, remove stale generated build output for this plugin/project, and let Unreal rebuild. |
| The wrong widget was edited | Set the active UMG target first or pass a full asset path such as `/Game/UI/WBP_LoginPanel.WBP_LoginPanel`. |

## Open Source And Fab

This repository is the open-source home of the UMG MCP protocol and editor bridge. The Fab package is an optional packaged distribution for users who prefer marketplace installation and production-focused packaging. Core protocol fixes should remain visible here so issues can be reported, reviewed, and reproduced in public.

Fab listing: [UMG MCP on Fab Marketplace](https://www.fab.com/zh-cn/listings/f70dbcb0-11e4-46bf-b3f7-9f30ba2c9b71)

## Contributing

Good contributions include reproducible bug reports, focused protocol tests, payload examples, documentation fixes, and small editor-side fixes with clear before/after behavior. Please include your Unreal Engine version, plugin commit, target asset path, and the MCP payload that failed when reporting an issue.

## License

This repository is released under the [MIT License](LICENSE).
