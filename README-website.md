# UMG MCP documentation website

This branch contains the static site deployed to
`https://winyunq.github.io/UnrealMotionGraphicsMCP/`.

## Documentation model

The site intentionally has one linear source of truth instead of duplicating
product claims across many marketing pages:

- `index.html` — the ordered project overview: product layers, edition matrix,
  tool map, architecture, workflow, setup choices, and known boundaries.
- `mcp-tools.html` — the complete public MCP tool catalog.
- `getting-started.html` — the two supported installation and connection paths.
- `contact.html` — support channels.
- legacy root pages redirect to the relevant section above.

The current catalog data is stored in:

- `assets/data/release.json` — separate `sourceVersion` / `sourceInternalVersion`
  and `fabVersion` / `fabInternalVersion` fields prevent the two release lines
  from being conflated.
- `assets/data/mcp-tools.json`

Every tool entry has Chinese and English descriptions. `assets/js/mcp-tools.js`
renders and filters the catalog.

## What “89 tools” means

The count is the public external MCP Server surface registered by
`Resources/Python/UmgMcpServer.py` on the public `main` branch:

- 3 connection and multi-editor session tools
- 86 domain tools

Fab-only orchestration pseudo-tools such as `task_begin` and `task_end`, and raw
C++ `CommandType` helpers that are not registered by the Python MCP server, are
not mixed into this number.

## Preventing documentation drift

Run the validator from the website worktree:

```powershell
node Scripts/validate-site-docs.mjs --source-ref upstream/main
```

It fails when any of these drift:

- public MCP tool names or count
- open-source plugin version, Fab release metadata, or audited source commit
- Chinese/English translation keys
- internal links
- known stale claims such as the old tool count or UE 5.3+ requirement

The GitHub Pages workflow fetches the current public `main` branch and runs the
same check before deployment. The protocol/tutorial subtree under `/Document/`
is archived from that same `main` commit, so the site does not publish the stale
source snapshot stored historically on the website branch.

## Local preview

Serve the branch through HTTP so translations and catalog JSON can load:

```powershell
python -m http.server 8080
```

Then open `http://127.0.0.1:8080/`.

## Deployment

Push the reviewed site commit to the public repository's `Document` branch.
`.github/workflows/pages.yml` validates and deploys the static artifact. The
workflow is intentionally triggered by `Document`; the former
`copilot/website` branch is no longer used.
