# Contributing to UmgMcp

## Getting Started

1.  **Prerequisites**: Ensure you have Python 3.10+ and `uv` installed.
2.  **Setup**:
    ```bash
    uv sync
    ```

## Running with Antigravity (Windows)

Antigravity on Windows requires strict JSON-RPC stream compliance (no `\r` characters).
We provide a wrapper script to handle this automatically.

**Do NOT modify `UmgMcpServer.py` to add binary hacks.**
Instead, configure Antigravity to use the wrapper:

```json
"command": ".../python.exe",
"args": [".../antigravity_wrapper.py", "UmgMcpServer.py"]
```

## Release Workflow

- We use Semantic Versioning.
- Current Release: **v1.5**
    - Feature: Antigravity Support
    - Feature: Animation Sequencer (Beta)

## Code Style

- Keep `UmgMcpServer.py` and `UmgSequencerServer.py` pure Python.
- Platform-specific fixes should go into wrappers or adapters.
