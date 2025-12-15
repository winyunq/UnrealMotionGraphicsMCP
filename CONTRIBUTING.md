# Contributing to UE5-UMG-MCP

First off, thanks for taking the time to contribute! 🎉

The following is a set of guidelines for contributing to `UE5-UMG-MCP` on GitHub. These are mostly guidelines, not rules. Use your best judgment, and feel free to propose changes to this document in a pull request.

## 🤖 For AI Agents

If you are an AI Agent (Gemini, ChatGPT, or others) contributing to this repository:
**Please read [gemini.md](gemini.md) strictly.** It contains the "Source of Truth" protocol and specific rules for AI collaboration.

---

## 🛠️ Getting Started

1.  **Fork the repository** on GitHub.
2.  **Clone** your fork locally.
3.  **Install dependencies**:
    *   Unreal Engine 5.4+ is required.
    *   Python dependencies are managed via `uv` (see `Readme.md` for setup).

## 🐛 Reporting Bugs

Bugs are tracked as [GitHub Issues](https://github.com/winyunq/UnrealMotionGraphicsMCP/issues).

When filing an issue, please include:
1.  **Startup Sequence**: Did you start UE5 *before* the CLI? (See known issues).
2.  **Logs**: Provide output from the Gemini CLI or Python console (`UmgMcpServer.py`).
3.  **UE5 Logs**: Check the Output Log in Unreal Editor (filter for `LogUmgMcp`).

## 💡 Suggesting Enhancements

Enhancement suggestions are tracked as [GitHub Issues](https://github.com/winyunq/UnrealMotionGraphicsMCP/issues).
*   Use a **clear and descriptive title** for the issue to identify the suggestion.
*   Provide a **step-by-step description of the suggested enhancement** in as much detail as possible.

## 📥 Pull Requests

1.  **Create a branch** for your feature or fix.
2.  **Commit your changes** using descriptive commit messages.
    *   We use a simple format: `Fix: description` or `Feat: description`.
3.  **Push** to your fork.
4.  **Submit a Pull Request** to the `main` branch.

### Code Style
*   **C++**: Follow [Epic Games Coding Standard](https://docs.unrealengine.com/5.0/en-US/epic-cplusplus-coding-standard-for-unreal-engine/).
*   **Python**: Follow PEP 8 where possible.
    *   Use `asyncio` for I/O bound operations.
    *   Keep `UmgMcpServer.py` clean and focused on dispatching commands.

## 📜 License

By contributing, you agree that your contributions will be licensed under its MIT License.
