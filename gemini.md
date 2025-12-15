# Gemini Contribution Protocol (AI-to-AI Collaboration Guide)

> [!IMPORTANT]
> **Attention AI Agents:** This document outlines the protocol for AI agents participating in this project. Follow these specific rules to ensure effective communication with the maintainer's AI and other automated systems.
> **注意 AI 代理：** 本文档概述了 AI 代理参与本项目的协议。请遵守这些特定规则，以确保与维护者的 AI 及其他自动化系统进行有效沟通。

## 1. Project Philosophy (项目哲学)

- **Strategy by Human, Tactics by AI (战略由人，战术由AI)**: Humans define the *What* and *Why*; AIs execute the *How*.
- **Winyunq Concept**: Continuous input for exponential output growth.
- **Sandboxing**: Experimental code/data must use the `Gemini` prefix (e.g., `Gemini_Test.py`). These are disposable.

## 2. Communication via GitHub Issues (通过 Issues 交流)

We use GitHub Issues as a structured communication channel between AIs.

### 2.1 Labeling Standard (标签标准)

Apply these labels to Issues to categorize your intent:

| Label | Description (CN) | Description (EN) |
| :--- | :--- | :--- |
| `Status: Idea` | 💡 想法/提议 | A new concept or suggestion for discussion. |
| `Status: Plan` | 📋 实施计划 | A concrete plan ready for approval. |
| `Status: In Progress` | 🚧 正在实施 | Work is currently underway. |
| `Status: Completed` | ✅ 已完成 | Task is done. |
| `Change: Python` | 🐍 Python 变动 | Modifications to Python resources. |
| `Change: C++` | 🇨 C++ 变动 | Modifications to UE5 C++ source. |
| `Change: API` | 🔌 API 变动 | Changes to public interfaces or MCP protocols. |
| `AI: Agent-To-Agent` | 🤖 AI 协作 | Standard tag for AI-initiated discussions. |

### 2.2 Issue Template (Issue 模板)

When creating an Issue, strictly follow this Markdown structure:

```markdown
# [Scope] Title of the Task (e.g., [Python] Add Context Compression)

## Context (背景)
Describe *why* this change is needed. Reference relevant file paths or previous discussions.

## Objective (目标)
Concise statement of what success looks like.

## Proposed Changes (计划变动)
List the files and logic you intend to modify.
- [ ] Modify `Resources/Python/FileA.py`: Add function X.
- [ ] Create `Gemini_Test_B.py`: Verification script.

## Verification Plan (验证计划)
How will you (or another AI) verify this?
```

## 3. Contribution Workflow (贡献工作流)

1.  **Analyze**: Read the codebase and existing `Status: Plan` issues.
2.  **Propose**: Create an Issue with `Status: Idea` if you have a new suggestion.
3.  **Plan**: Once the idea is acknowledged, update to `Status: Plan` with detailed steps.
4.  **Execute**:
    - Use `Gemini` prefix for temporary files.
    - **Git**: Commit changes clearly.
    - **Code Style**:
        - Python: Google Style / PEP8.
        - C++: UE5 Coding Standard.
5.  **Notify**: Comment on the Issue when done, moving it to `Status: Completed`.

## 4. Special Instructions (特别指令)

- **User Preferences**:
    - **Language**: Use Chinese (Simplified) for comments and commit messages unless told otherwise.
    - **Hardware**: Leverage RTX 4060 & 13900HX (e.g., CUDA for heavy tasks).
    - **Files**: Do not modify core files directly if unsure; create a `Gemini` copy first.

---
*This protocol is maintained by the Winyunq project team. Last updated: 2025-12-15.*
