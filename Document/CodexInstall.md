# Install UMG MCP for Codex

Codex supports MCP servers through `config.toml` or `codex mcp add`. The installer in this repository registers the UMG MCP Python server as a Codex STDIO MCP server.

## Requirements

- Codex CLI or Codex desktop/IDE using the same `~/.codex/config.toml`.
- `uv` available on `PATH`.
- Unreal Editor running with this plugin loaded when you want to call UMG tools.

## One-command install

From this plugin root:

```powershell
.\Scripts\Install-CodexUmgMcp.ps1
```

This registers:

```toml
[mcp_servers.umg-mcp]
command = "uv"
args = ["run", "--directory", "...\Resources\Python", "UmgMcpServer.py"]
startup_timeout_sec = 20
tool_timeout_sec = 120
enabled = true
```

Use a custom plugin path:

```powershell
.\Scripts\Install-CodexUmgMcp.ps1 -PluginRoot "D:\Path\To\Project\Plugins\FabUmgMcp"
```

Install into a project-scoped Codex config instead of the user config:

```powershell
.\Scripts\Install-CodexUmgMcp.ps1 -Scope project
```

Project-scoped config only loads after the project is trusted in Codex. User-level install is the recommended default.

## Verify

Restart Codex or open a new session, then run `/mcp` in Codex. You should see `umg-mcp`.

If the server starts but tool calls fail, confirm Unreal Editor is open and the UMG MCP plugin is listening on the host/port configured in `Resources/Python/mcp_config.py`.

## Use Codex As A FabServer AI Provider

The MCP install above lets Codex call UMG MCP tools. The plugin can also expose Codex as a FabServer AI provider by launching an installed Codex CLI through `codex exec`.

This does not make ChatUI route around FabServer. ChatUI still sends messages to FabServer, and FabServer chooses the active `IUmgMcpAiProvider`.

In **Project Settings > Plugins > Unreal Motion Graphics MCP > McpCli**, set:

```text
LocalCliProvider = Codex CLI
LocalCliCommand = codex
```

Then set **MultiRetry > PrimaryProvider** to:

```text
Codex CLI
```

If the Codex desktop WindowsApps executable cannot be launched from other processes, install or expose a standalone CLI command and point `LocalCliCommand` to it, for example:

```text
LocalCliCommand = npx -y @openai/codex
```

The FabServer provider runs:

```text
codex exec --ephemeral --sandbox read-only
```

This is intentionally read-only and returns text through the normal FabServer provider response path.

## Authentication Boundary

Do not implement a separate ChatGPT account login inside this plugin for Codex. Public Codex documentation describes ChatGPT sign-in and API-key auth for Codex's own surfaces, but it does not document a third-party Unreal plugin flow for reusing or exchanging a ChatGPT login session.

Supported integration paths are:

- Codex CLI provider: use an installed, already-authenticated `codex` command.
- Codex MCP consumer: install `umg-mcp` so Codex can call UMG MCP tools.
- OpenAI-compatible API provider: use an API key through the plugin's existing API provider settings if you want direct model API calls instead of Codex.
