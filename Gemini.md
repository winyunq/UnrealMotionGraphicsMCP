# Gemini.md - UMG Agent Development Log

## 项目目标

开发一个针对 Unreal Engine 5 (UE5) UMG (Unreal Motion Graphics) 界面的 AI Agent，使其能够感知和操作 UMG UI。

## 插件信息

*   **插件名称:** `UmgMcp`
*   **插件类型:** C++ 引擎/项目插件 (目前作为项目插件安装在 `D:\ModelContextProtocol\unreal-engine-mcp\FlopperamUnrealMCP\Plugins\UE5_UMG_MCP`)
*   **核心功能:**
    *   提供与 UE5 UMG 交互的 C++ 接口。
    *   通过 MCP 服务器暴露这些功能，供外部 Python 客户端调用。

## 关键文件

*   `UmgMcp.uplugin`: 插件描述文件。
*   `Source/UmgMcp/UmgMcp.Build.cs`: 模块构建文件，定义依赖项。
*   `Source/UmgMcp/Public/UmgGetSubsystem.h`: “感知”功能子系统头文件。
*   `Source/UmgMcp/Private/UmgGetSubsystem.cpp`: “感知”功能子系统实现。
*   `Source/UmgMcp/Public/UmgSetSubsystem.h`: “行动”功能子系统头文件。
*   `Source/UmgMcp/Private/UmgSetSubsystem.cpp`: “行动”功能子系统实现。
*   `Resources/Python/UmgMcpServer.py`: 核心MCP服务器，定义所有工具。
*   `Resources/Python/UMGGet.py`: “感知”功能的Python客户端模块。
*   `Resources/Python/UMGSet.py`: “行动”功能的Python客户端模块。

## 当前状态与进展 (V2.0 - API重构完成)

1.  **API 设计重构:** 遵循用户的战略指导，放弃了单一的`apply_json_to_umg`方案，重构为一套更精细、更健壮的原子化API，分为“感知”和“行动”两大类别。

2.  **C++ 后端实现:**
    *   在 `UmgGetSubsystem` 中完整实现了所有“感知”功能，包括 `GetWidgetTree`, `QueryWidgetProperties`, `GetLayoutData`, `CheckWidgetOverlap`。
    *   创建并完整实现了 `UmgSetSubsystem`，包含所有“行动”功能，如 `SetWidgetProperties`, `CreateWidget`, `DeleteWidget`, `ReparentWidget`。
    *   解决了开发过程中遇到的多轮编译问题，包括头文件引用错误、API调用错误等。

3.  **Python 端实现:**
    *   创建了与C++后端对应的 `UMGGet.py` 和 `UMGSet.py` 模块。
    *   对所有Python模块 (`UMGGet`, `UMGSet`, `UMGAttention`) 进行了统一的类结构化重构，解决了依赖引用问题。
    *   更新了主服务器 `UmgMcpServer.py`，使其能正确实例化并调用所有新模块和新API。

## 下一步行动

*   **用户操作:**
    1.  **编译项目:** 最后一次编译Unreal项目，使所有C++更改生效。
    2.  **重启服务器:** 重启Python MCP服务器，加载所有Python脚本的更改。

*   **AI 准备:** 等待用户完成操作后，开始对全新的、完整的API进行功能性测试。