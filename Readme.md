[中文版](Readme_zh.md)

<div align="center">
  <img src="Resources/Icon128.png" width="96" alt="UMG MCP logo">
  <h1>UMG MCP for Unreal Engine 5.8</h1>
  <p><strong>Build, inspect, refactor, and version UMG UI through AI agents and MCP tools.</strong></p>
  <p>
    <a href="https://winyunq.github.io/UnrealMotionGraphicsMCP/">Web Manual</a>
    · <a href="#install">Install</a>
    · <a href="#connect-an-mcp-client">MCP Client</a>
    · <a href="https://www.fab.com/zh-cn/listings/f70dbcb0-11e4-46bf-b3f7-9f30ba2c9b71">Fab Marketplace</a>
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

UMG MCP turns Unreal Editor UMG work into explicit, reviewable tool calls. An AI agent can create widgets, update layout, wire Blueprint logic, edit materials, drive UMG animation, and round-trip UI files through JSON instead of relying on screenshots or fragile click automation.

The README is the front door. The step-by-step tutorials live in the deployed documentation site:

<table>
  <tr>
    <td><strong>Start here</strong></td>
    <td><a href="https://winyunq.github.io/UnrealMotionGraphicsMCP/">Open the web manual</a></td>
  </tr>
  <tr>
    <td><strong>Repository copy</strong></td>
    <td><a href="Document/index.html">Document/index.html</a> and <a href="Document/index_zh.html">Document/index_zh.html</a></td>
  </tr>
</table>

## Why It Exists

| Problem in normal AI UI work | UMG MCP approach |
| --- | --- |
| Agents guess from screenshots | Agents read the actual widget tree, slots, properties, materials, and graph state |
| UI edits are hard to diff | UMG can be exported, patched, and re-applied as structured JSON |
| Blueprint wiring is click-heavy | BlueCode and MCP commands create nodes, pins, references, and links directly |
| Long chat history wastes context | Fab edition adds production packaging and context-management work for larger sessions |

## Install

UMG MCP currently targets **Unreal Engine 5.8 on Win64**. The plugin descriptor in this package is set to `EngineVersion: 5.8.0`.

### Option A: Fab Edition

Use this if you want the shortest install path and the in-editor workflow.

1. Open the [Fab Marketplace listing](https://www.fab.com/zh-cn/listings/f70dbcb0-11e4-46bf-b3f7-9f30ba2c9b71).
2. Add the plugin to your Epic Games / Unreal Engine library.
3. Install it for Unreal Engine 5.8.
4. Open your project, enable `UmgMcp`, then restart the editor.
5. Open the plugin settings and configure your model provider. The web manual covers Google OAuth, Gemini API, ZhipuAI, and OpenAI-compatible endpoints.

### Option B: Source Edition

Use this if you want to inspect the protocol, develop fixes, or integrate the plugin manually.

```powershell
cd D:\UE5Project\YourProject
mkdir Plugins
cd Plugins
git clone https://github.com/winyunq/UnrealMotionGraphicsMCP.git UmgMcp
```

Then open the project in Unreal Engine 5.8 and allow Unreal Build Tool to compile the plugin. If you move the plugin between projects, keep the final folder name as `UmgMcp` unless you also update your MCP client configuration.

## Connect An MCP Client

External MCP clients use the Python server under `Resources/Python`. Start Unreal Editor first so the plugin can bind the local bridge on `127.0.0.1:55557`.

Add this to the `mcpServers` section of your client settings, replacing the path with your own absolute plugin path:

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

Notes:

- The path must point to `Resources/Python`, not the repository root.
- On Windows JSON paths need escaped backslashes: `D:\\Path\\To\\Project`.
- If `uv` is missing, install it first or create the optional virtual environment below.

Optional local Python setup:

```powershell
cd D:\UE5Project\YourProject\Plugins\UmgMcp\Resources\Python
uv venv
.\.venv\Scripts\activate
uv pip install "mcp[cli]>=1.4.1" fastmcp uvicorn fastapi "pydantic>=2.6.1" requests
python -c "import mcp, fastapi, pydantic, requests; print('deps ok')"
```

## Verify The Setup

Run static protocol checks without opening a project:

```powershell
cd D:\UE5Project\YourProject\Plugins\UmgMcp\Resources\Python
uv run python APITest\Umg_Widget_Protocol_Static_Check.py
uv run python APITest\Blueprint_MCP_Schema_Check.py
uv run python APITest\Material_Protocol_Static_Check.py
uv run python APITest\Animation_Protocol_Static_Check.py
```

Run an editor smoke test after Unreal Editor is open:

```powershell
uv run python APITest\UE5_Editor_Imitation.py
```

If the editor-side test cannot connect, check that the plugin is enabled and that no other process owns port `55557`.

## Documentation Map

| Goal | Web page | Repository file |
| --- | --- | --- |
| Set up model providers | [Model configuration](https://winyunq.github.io/UnrealMotionGraphicsMCP/Document/model_config.html) | [Document/model_config.html](Document/model_config.html) |
| Use the in-editor chat workflow | [Chat dialogue guide](https://winyunq.github.io/UnrealMotionGraphicsMCP/Document/chat_dialogue.html) | [Document/chat_dialogue.html](Document/chat_dialogue.html) |
| Understand MCP support | [MCP support](https://winyunq.github.io/UnrealMotionGraphicsMCP/Document/mcp_support.html) | [Document/mcp_support.html](Document/mcp_support.html) |
| Widget command protocol | - | [Document/UmgWidgetMcpProtocol.md](Document/UmgWidgetMcpProtocol.md) |
| Blueprint / BlueCode protocol | - | [Document/BlueprintBluecodeProtocol.md](Document/BlueprintBluecodeProtocol.md) |
| Material and HLSL protocol | - | [Document/MaterialMcpProtocol.md](Document/MaterialMcpProtocol.md) |
| Animation protocol | - | [Document/AnimationMcpProtocol.md](Document/AnimationMcpProtocol.md) |
| Google OAuth setup | - | [Document/Google_OAuth_Setup.md](Document/Google_OAuth_Setup.md) |

## What Agents Can Do

| Area | Examples |
| --- | --- |
| UMG widgets | Create widgets, set slot data, reorder tree nodes, replace subtrees, export and apply JSON |
| Blueprint / BlueCode | Create graphs, add nodes, connect pins, bind widget references, apply compact code-like statements |
| Materials | Build UI materials, set scalar/vector parameters, add HLSL snippets, validate supported material fields |
| Animation / Sequencer | Create UMG animation tracks and keyframes for render transform, opacity, color, and timing |
| Editor utilities | Inspect selected assets, read project state, manage the active target, and run focused tests |

Example prompt for an MCP-capable agent:

```text
Create /Game/UI/WBP_LoginPanel with a centered login card, email and password fields,
a primary submit button, and a subtle material background. Export the final widget JSON
and summarize which MCP tools changed the asset.
```

## Videos

- [Designing an RTS UI](https://youtu.be/O86VCzxyF5o)
- [Recreating the UE5 editor](https://youtu.be/h_J70I0m4Ls)
- [Editing UMG inside the UMG editor](https://youtu.be/pq12x2MH1L4)
- [Chatting with Gemini to edit a UMG file](https://youtu.be/93_Fiil9nd8)

## Troubleshooting

| Symptom | Fix |
| --- | --- |
| MCP client reports `ConnectionRefusedError`, `WinError 10061`, or `WinError 1225` | Open Unreal Editor first, enable `UmgMcp`, restart the editor, then reconnect the MCP client. The editor bridge listens on `127.0.0.1:55557`. |
| `uv` is not recognized | Install `uv`, restart the terminal, or use the optional local virtual environment setup above. |
| Plugin does not compile after copying between projects | Confirm the project is using UE 5.8 Win64, remove stale generated build output for this plugin/project, then let Unreal rebuild. |
| Tool calls succeed but edit the wrong widget | Set the active UMG target first or pass the full asset path, for example `/Game/UI/WBP_LoginPanel.WBP_LoginPanel`. |
| A protocol command is unclear | Use the web manual for workflow guidance and the protocol markdown files for exact payload shapes. |

## Fab And Open Source

| Edition | Best for | Notes |
| --- | --- | --- |
| Open source repository | Protocol review, research, custom integration, issue fixes | Core UMG MCP behavior remains available here. |
| Fab edition | Artists, technical artists, and production projects that want a packaged install | Adds the marketplace install path, in-editor workflow packaging, and production-focused support. |

Core protocol improvements are intended to flow back to the open repository. If you report an issue from a project using the Fab package, include your UE version, plugin version, target asset path, and the failing MCP tool payload when possible.

## Developer Program

Developers who contribute useful fixes, tests, documentation, or protocol examples can request Fab access for validation work. Open an issue or contact the maintainer with the contribution link and the scenario you want to test.

## License

This repository is released under the [MIT License](LICENSE).
