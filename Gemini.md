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

## 当前状态与进展 (V3.0 - 核心逻辑重构与清理)

1.  **核心逻辑重构 (C++):** 彻底重构了 `WidgetCommands` 和相关子系统 (`Get`/`Set`) 的工作方式。创建了新的目标解析逻辑，实现了“**优先使用命令参数，失败则回退到全局目标**”的健壮工作流。修复了资生路径中包含 `.uasset` 后缀的加载问题。

2.  **命令清理 (C++):** 从 `UmgMcpBridge` 中移除了所有与 UMG 不直接相关的通用命令（如 `EditorCommands` 和 `BlueprintCommands`），使插件的职责更专注、更清晰。

3.  **Python 端同步重构:**
    *   完全重写了 `UmgMcpServer.py` 中的工具定义，移除了冗余的客户端检查逻辑。
    *   所有 `Sensing` 和 `Action` 类的工具现在都接受一个可选的 `asset_path` 参数，将目标决策完全交给 C++ 后端处理。
    *   更新了所有工具的文档字符串（AI HINT），以匹配新的工作流程。

## API 清单与评估 (V3.0)

### 1. 感知 (Sensing) - AI的“眼睛”

*   `get_widget_tree(asset_path: Optional[str] = None)`
    *   **作用**: 获取UI的完整层级结构。**这是最核心的感知API**。
    *   **工作流**: 如果提供了 `asset_path`，则直接操作该文件。如果省略，则自动操作由 `set_target_umg_asset` 设定的全局目标。

*   `query_widget_properties(widget_name: str, properties: List[str], asset_path: Optional[str] = None)`
    *   **作用**: 精确查询某个控件的一个或多个具体属性的值。
    *   **工作流**: 同上，优先使用 `asset_path`，否则回退到全局目标。

*   `get_layout_data(resolution_width: int = 1920, resolution_height: int = 1080, asset_path: Optional[str] = None)`
    *   **作用**: 获取所有控件在给定分辨率下的屏幕坐标和尺寸。
    *   **工作流**: 同上。

*   `check_widget_overlap(widget_names: Optional[List[str]] = None, asset_path: Optional[str] = None)`
    *   **作用**: 一个高效的后端API，直接判断界面上是否存在控件重叠。
    *   **工作流**: 同上。

### 2. 行动 (Action) - AI的“双手”

*   `create_widget(parent_name: str, widget_type: str, new_widget_name: str, asset_path: Optional[str] = None)`
    *   **作用**: 在指定的父控件下创建一个新的控件。
    *   **工作流**: 同上。

*   `delete_widget(widget_name: str, asset_path: Optional[str] = None)`
    *   **作用**: 删除一个指定的控件。
    *   **工作流**: 同上。

*   `set_widget_properties(widget_name: str, properties: Dict[str, Any], asset_path: Optional[str] = None)`
    *   **作用**: 修改一个控件的一个或多个属性。**这是最核心的修改API**。
    *   **工作流**: 同上。

*   `reparent_widget(widget_name: str, new_parent_name: str, asset_path: Optional[str] = None)`
    *   **作用**: 将一个控件从一个父容器移动到另一个。
    *   **工作流**: 同上。

### 3. 自省与上下文 (Introspection & Context)

*   `get_creatable_widget_types()`
    *   **作用**: 告诉AI它“工具箱”里有哪些类型的控件可以被创建。

*   `get_widget_schema(widget_type)`
    *   **作用**: 告诉AI某个特定类型的控件有哪些可以被编辑的属性。

*   `set_target_umg_asset(asset_path: str)`
    *   **作用**: 设置一个全局工作目标，所有后续省略 `asset_path` 参数的命令都将操作此目标。

*   `get_target_umg_asset()`
    *   **作用**: 查询当前设定的全局工作目标是什么。

### 评估结论

*   **核心能力**: API 逻辑和工作流已得到极大改善，更加健壮和灵活。
*   **下一步建议**: 对重构后的 API 进行全面的功能性测试，确保所有分支（提供/不提供 `asset_path`）都能正常工作。
