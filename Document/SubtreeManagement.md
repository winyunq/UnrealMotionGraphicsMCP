# Fab UMG MCP subtree management

This repository is the Fab edition. It vendors three upstream projects and owns the closed Fab overlay.

## Projects

| Project | Remote | Branch | Local path | Rule |
| --- | --- | --- | --- | --- |
| Open-source UMG MCP | `upstream` | `main` | repository root / `Source/UmgMcp` | Pull from upstream. Do not push Fab code here. |
| Chat With Unreal | `chat-upstream` | `main` | `Source/ChatWithUnreal` | Manage with `git subtree`. |
| LiteRT-LM Unreal | `litert-upstream` | `master` | `Source/LiteRTLMUnreal` | Manage with `git subtree`. |
| Fab closed overlay | `origin` | `master` | `Source/UmgMcp/Public/FabServer`, `Source/UmgMcp/Private/FabServer` | Maintain only in this Fab repository. |

## Ownership rule

Maintain Fab-specific work only in:

- `Source/UmgMcp/Public/FabServer`
- `Source/UmgMcp/Private/FabServer`
- `UmgMcp.uplugin`
- Fab export scripts, Codex install scripts, subtree-management scripts, and Fab-only documents

Everything else should be treated as upstream-owned. If a Fab integration needs a change outside those paths, make it a small, explicit integration commit and expect to re-check it during every upstream merge.

`UmgMcp.uplugin` is intentionally Fab-owned even though the open-source plugin also has a `.uplugin`. The Fab manifest declares the combined product modules (`UmgMcp`, `LiteRTLMUnreal`, `ChatWithUnreal`) and Fab marketplace metadata, so upstream merges should not overwrite it.

## Common commands

Run these from the repository root:

```powershell
.\Tools\Subtree\Sync-Subtrees.ps1 -Action status
.\Tools\Subtree\Sync-Subtrees.ps1 -Action fetch
.\Tools\Subtree\Sync-Subtrees.ps1 -Action merge-umg
.\Tools\Subtree\Sync-Subtrees.ps1 -Action pull-chat
.\Tools\Subtree\Sync-Subtrees.ps1 -Action pull-litert
```

`merge-umg` defaults to `-UmgProtectMode current-diffs`. It protects every current Fab-side path that differs from `upstream/main`, except the separately managed `Source/ChatWithUnreal` and `Source/LiteRTLMUnreal` subtrees. Use `-UmgProtectMode fab-overlay` only when you intentionally want to protect the minimal Fab overlay list.

For a full update:

```powershell
.\Tools\Subtree\Sync-Subtrees.ps1 -Action sync-all
```

The script requires a clean working tree for merge/pull actions. This avoids mixing local Fab work with upstream subtree updates. Use `-AllowDirty` only when you have already reviewed the local changes.

## Boundary check

Before committing Fab-only work:

```powershell
.\Tools\Subtree\Sync-Subtrees.ps1 -Action guard
```

If the guard reports files outside the Fab-owned boundary, either move the change to the correct upstream repository or document it as a Fab integration exception in the commit message.

## Conflict policy

When merging `upstream/main`, keep upstream changes for open-source files and keep Fab versions for Fab-owned files. In practice:

- `Source/UmgMcp/Public/FabServer` and `Source/UmgMcp/Private/FabServer`: keep Fab.
- `UmgMcp.uplugin`: keep Fab, then manually port any genuinely required upstream descriptor changes.
- `Source/UmgMcp` outside `FabServer`: prefer upstream.
- Current Fab-side files that already differ from `upstream/main`: protected by default during `merge-umg`; review intentionally before allowing upstream to replace them.
- `Source/ChatWithUnreal` and `Source/LiteRTLMUnreal`: update only through `git subtree pull`.

The helper script backs up the Fab overlay before `merge-umg` and restores it after a successful merge. Still review the diff before committing.
