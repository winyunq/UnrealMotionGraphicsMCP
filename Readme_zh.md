[English version please click here](Readme.md)

# UE5-UMG-MCP 🤖📄

**版本受控的 AI 辅助 UMG 工作流**

![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)![Status: Experimental](https://img.shields.io/badge/status-experimental-red.svg)![Built with AI](https://img.shields.io/badge/Built%20with-AI%20Assistance-blueviolet.svg)[![AgentSeal MCP](https://agentseal.org/api/v1/mcp/https-githubcom-winyunq-unrealmotiongraphicsmcp/badge)](https://agentseal.org/mcp/https-githubcom-winyunq-unrealmotiongraphicsmcp)

[Demo 设计 RTS UI](https://youtu.be/O86VCzxyF5o)

[Demo 在 UMG 窗口中复现 UE5 编辑器预览](https://youtu.be/h_J70I0m4Ls)

[Demo 在 UMG 编辑器中操作 UMG 控件](https://youtu.be/pq12x2MH1L4)

[与 Gemini 3 对话编辑 UMG 文件](https://youtu.be/93_Fiil9nd8)

---

### 🛍️ 需要更便捷的使用体验？尝试 Fab 版！

如果您觉得手动配置插件和 MCP 环境过于繁琐，可以尝试我们在 **Fab** 上架的商业化版本：
[**Fab 版 UMG MCP 直达链接**](https://www.fab.com/zh-cn/listings/f70dbcb0-11e4-46bf-b3f7-9f30ba2c9b71)

**Fab 版与开源版的区别：**
- **开箱即用**：Fab 版可以直接通过虚幻引擎后台安装，无需手动克隆和配置 Python 环境。
- **上下文压缩 (Context Compression)**：Fab 版内置了更智能的上下文管理，底层会自动过滤冗余的 MCP 历史信息，为 AI 腾出更多可用的 Token。
- **商业化支持**：Fab 版侧重于生产环境的商业逻辑。如果您在商业项目中遇到困难，我们将提供更优先的技术支持和解决方案。
- **持续同步**：开源版 (GitHub) 依然是我们的协议先行地，所有核心功能的更新都会同步到开源版本中。

---

### 🚀 快速开始

本指南涵盖了安装 `UmgMcp` 插件并将其连接到 Gemini CLI 的两个步骤。

*   **前提条件：** Unreal Engine 5.6 或更高版本。

#### 1. 安装插件 (Install the Plugin)

1.  **进入项目的 Plugins 文件夹：** `YourProject/Plugins/`（如果不存在则创建）。
2.  **直接克隆仓库**到此目录：

    ```bash
    git clone https://github.com/winyunq/UnrealMotionGraphicsMCP.git UmgMcp
    ```

3.  **重启 Unreal Editor。** 这允许引擎检测并编译新插件。

#### 2. 连接 Gemini CLI (Connect the Gemini CLI)

告诉 Gemini 如何找到并启动 MCP 服务器。

1.  **编辑您的 `settings.json`** 文件（通常位于 `C:\Users\用户名\.gemini\`）。
2.  **将工具定义添加**到 `mcpServers` 对象中。

    ```json
    "mcpServers": {
      "UmgMcp": {
        "command": "uv",
        "args": [
          "run",
          "--directory",
          "D:\\Path\\To\\YourUnrealProject\\Plugins\\UmgMcp\\Resources\\Python",
          "UmgMcpServer.py"
        ]
      }
    }
    ```

    **注意：** 您**必须**将路径替换为您机器上克隆仓库中 `Resources/Python` 文件夹的正确**绝对路径**。

就是这样！当您启动 Gemini CLI 时，它将自动在后台启动 MCP 服务器。

#### 测试连接

重启 Gemini CLI 并打开 Unreal 项目后，您可以通过调用任何工具函数来测试连接：

```python
  cd Resources/Python/APITest
  python UE5_Editor_Imitation.py
```

#### Python 环境（可选）

插件的 Python 环境由 `uv` 管理。在大多数情况下，它应该会自动工作。如果您遇到与 Python 依赖项相关的问题（例如找不到 `uv` 命令或模块导入错误），您可以手动设置环境：

1.  导航到目录：`cd YourUnrealProject/Plugins/UmgMcp/Resources/Python`
2.  运行设置：
    ```bash
    uv venv
    .\.venv\Scripts\activate
    uv pip install -e .
    ```

---

### 🧪 实验性功能：Gemini CLI Skill 支持

我们正在尝试使用 **Gemini CLI Skill** 系统作为标准 MCP 方法的替代方案。
Skill 架构允许 Python 工具直接由 CLI 运行时加载，通过 `prompts.json` 动态启用/禁用工具，潜在地**优化上下文使用**，并避免管理独立 MCP 服务器进程的开销。

> **注意**：上面配置的 MCP 服务器仍然是使用此插件的稳定且推荐的方式。如果您想测试最新的集成能力，请使用 Skill 模式。

#### 配置 (Skill 模式)

要启用 Skill 模式，请将以下内容添加到您的 `settings.json` 中（替换 `<YOUR_PROJECT_PATH>`）：

```json
  "skills": {
    "unreal_umg": {
      "path": "<YOUR_PROJECT_PATH>/Plugins/UmgMcp/Resources/Python/UmgMcpSkills.py",
      "type": "local",
      "description": "通过 Python Skills 直接控制 Unreal Engine UMG。从 prompts.json 自动加载工具。"
    }
  },
```

---

## 简体中文 (Chinese)

本项目为管理 Unreal Engine 的 UMG UI 资产提供了一个强大的、命令行驱动的工作流。通过将 **人类可读的 `.json` 文件视为唯一的真理源 (Source of Truth)**，它从根本上解决了在 Git 中对二进制 `.uasset` 文件进行版本控制的挑战。

受 `blender-mcp` 等工具的启发，该系统允许开发人员、UI 设计师和 AI 助手以编程方式与 UMG 资产进行交互，从而实现真正的 Git 协作、自动化 UI 生成和迭代。

---

## Prompt 管理器 (Prompt Manager)

一个用于配置系统指令、工具描述和用户提示模板的可视化 Web 工具。

### 功能

1.  **系统指令编辑器**：修改针对 AI 上下文的全局指令。
2.  **工具管理**：
    *   **启用/禁用**：开启或关闭特定的 MCP 工具。禁用的工具不会在 MCP 服务器上注册，从而有效地**压缩上下文窗口**，防止 AI 分心。
    *   **编辑描述**：自定义工具描述（提示词），使其更符合您的工作流。
3.  **用户模板 (Prompts)**：添加可重用的提示词模板，方便 MCP 客户端快速访问。

### 如何运行

在您的 Python 环境中执行以下命令：
```bash
python Resources/Python/PromptManager/server.py
```
浏览器将自动打开 `http://localhost:8085`。

### 使用提示

Prompt 对 AI 工具的有效性至关重要。使用 Prompt 管理器来为 AI 量身定制其行为：

*   **一键部署模式**：如果您希望 AI 仅专注于根据设计生成 UI，请禁用除 `apply_layout` 和 `export_umg_to_json` 之外的所有工具。
*   **导师模式**：如果您希望 AI 只是进行指导而不做逻辑更改，请仅保留只读工具（例如 `get_widget_tree`, `get_widget_schema`）。
*   **上下文优化**：对于上下文窗口较小的模型，禁用当前不使用的工具以提高速度和准确性。

欢迎贡献有效的 Prompt 配置！

---

### AI 署名与免责声明

本项目是在 **Gemini (一种 AI)** 的显著协助下开发的。因此：
*   **实验性质**：这是一个实验性项目，不保证其可靠性。
*   **商业用途**：在没有经过彻底的独立验证和了解其局限性的情况下，不建议用于商业用途。
*   **免责声明**：使用风险自负。开发人员和 AI 对因使用本项目而产生的任何后果概不负责。

---

### 当前技术架构概览

系统目前主要依靠 `UE5_UMG_MCP` 插件在外部客户端（如本 CLI）与 Unreal Engine 编辑器之间进行通信。

**架构图：**

```mermaid
flowchart LR
    subgraph "Local Execution Environment"
        CLI["Gemini CLI"] --"StdIO (JSON-RPC)"--> PY["Python (UmgMcpServer.py)"]
    end

    subgraph "Unreal Engine 5"
        PY --"TCP Socket (JSON)"--> TCP["UmgMcpBridge (C++)"]
        TCP --> API["Unreal API / UMG"]
    end
```

## API 实现状态

| 分类               | API 名称                         | 状态  |
| ------------------ | -------------------------------- | :---: |
| **上下文与注意力** | `get_target_umg_asset`           |   ✅   |
|                    | `set_target_umg_asset`           |   ✅   |
|                    | `get_last_edited_umg_asset`      |   ✅   |
|                    | `get_recently_edited_umg_assets` |   ✅   |
| **感知与查询**     | `get_widget_tree`                |   ✅   |
|                    | `query_widget_properties`        |   ✅   |
|                    | `get_creatable_widget_types`     |   ✅   |
|                    | `get_widget_schema`              |   ✅   |
|                    | `get_layout_data`                |   ✅   |
|                    | `check_widget_overlap`           |   ✅   |
| **动作与修改**     | `create_widget`                  |   ✅   |
|                    | `delete_widget`                  |   ✅   |
|                    | `set_widget_properties`          |   ✅   |
|                    | `reparent_widget`                |   ✅   |
|                    | `save_asset`                     |   ✅   |
| **文件转换**       | `export_umg_to_json`             |   ✅   |
|                    | `apply_json_to_umg`              |   ✅   |
|                    | `apply_layout`                   |   ✅   |

## UMG 蓝图 (Blueprint) API 实现状态 (New)

| 分类               | API 名称                  | 状态  | 描述                                                           |
| ------------------ | ------------------------- | :---: | -------------------------------------------------------------- |
| **上下文与注意力** | `set_edit_function`       |   ✅   | 设置当前编辑上下文（函数/事件）。支持自动创建自定义事件。      |
|                    | `set_cursor_node`         |   ✅   | 显式设置“光标”节点（程序计数器）。                             |
| **感知与查询**     | `get_function_nodes`      |   ✅   | 获取**当前上下文作用域**内的节点（过滤掉无关节点以减少噪音）。 |
|                    | `get_variables`           |   ✅   | 获取成员变量列表。                                             |
|                    | `search_function_library` |   ✅   | 搜索可调用的库 (C++/BP)。支持模糊搜索。                        |
| **动作与修改**     | `add_step(name)`          |   ✅   | **核心**：根据名称添加可执行节点。支持自动连线与布局。         |
|                    | `prepare_value(name)`     |   ✅   | 添加数据节点（例如 GetVariable）。                             |
|                    | `connect_data_to_pin`     |   ✅   | 精确连接引脚（支持 `NodeID:PinName` 格式）。                   |
|                    | `add_variable`            |   ✅   | 添加新的成员变量。                                             |
|                    | `delete_variable`         |   ✅   | 删除成员变量。                                                 |
|                    | `delete_node`             |   ✅   | 删除特定节点。                                                 |
|                    | `compile_blueprint`       |   ✅   | 编译并应用更改。                                               |

## Bluecode（草案）：文本优先的蓝图协议

目标：让 AI 一次性用文本描述并合并整个函数流程，避免节点级反复对话，遵循“易写难删”的偏好。

- **锚定函数**：`bluecode_set_function(path?)` 接受 `domain?:FunctionName`。域默认当前 UMG target；若存在 widget target 则优先。出现歧义返回候选而非盲猜。绑定后隐式入口/出口哨兵为 `begin` 与 `end`。
- **图模型**：执行流存放在 `node_list` 树，保留字有 `main`（入口）、`begin`（分支根）、`end`（默认空出口）、`return`（显式返回）。数据连线在 `connect_list`；无法运行的数据节点先落入修复桶，实在无法放置的符号进入 `floating_list`。
- **读取流程**：`bluecode_read_function` 按 `main -> end/return` 线性化执行，并输出带编号的表达式（如 `1:main(i)->2:if(i%2==0)->3:print("hi")->4:return`）。分支使用 `begin`/`end` 分组，AI 无需资产路径即可往返。
- **写入与合并**：`bluecode_write_function` 接受带号或不带号链，默认靠近 `end` 连接，左右锚点同时存在时优先右锚。禁止删除——后端按优先级（**id 邻近 > 参数 > 函数名 > 节点类型**）匹配已有节点，未匹配者追加到右侧。示例：已有 `main-print1-print3-end` + 输入 `main-print2-end` → `main-print1-print3-print2-end`（返回插入信息）；输入 `main-print2-print3-end` 识别重复 `print3` → `main-print1-print2-print3-end`。
- **不可连符号**：若节点无法进入执行流，先尝试当作参数折叠，再尝试一次 cast；仍失败则放入 `floating_list` 并附修复提示。仍可直接操作 `connect_list` 做外科级引脚连线。
- **编译**：`bluecode_compile` 编译目标蓝图并返回精简 JSON（成功/报错、涉及节点、悬空符号），不回填冗长历史。

## UMG 动画 (Sequencer) API 实现状态

| 命令                           |   状态   | 描述                                                                                  |
| :----------------------------- | :------: | :------------------------------------------------------------------------------------ |
| `animation_target`             | ✅ 已实现 | 设置/聚焦当前动画（等价 `set_animation_scope`，若不存在会自动创建）。                 |
| `widget_target`                | ✅ 已实现 | 设置/聚焦当前控件（等价 `set_widget_scope`）。                                        |
| `animation_overview`           | ✅ 已实现 | 返回关键帧数量、轨道数量、关键时间点、变更的属性列表。                                 |
| `animation_widget_properties`  | ✅ 已实现 | 按控件视角查看属性时间线（忽略未被动画驱动的属性）。                                   |
| `animation_time_properties`    | ✅ 已实现 | 按时间切片查看属性值，支持一次查询多个时间点。                                         |
| `animation_append_widget_tracks` | ✅ 已实现 | 按控件+属性批量追加/覆盖关键帧（仅并集/覆盖，不做删除）。                               |
| `animation_append_time_slice`  | ✅ 已实现 | 在指定时间为多个控件追加差分式属性值。                                                |
| `animation_delete_widget_keys` | ✅ 已实现 | 仅删除指定控件+属性在指定时间的关键帧（需 `confirm_delete=true`，符合 Issue 15 安全策略）。 |
| `create_animation`             | ✅ 已实现 | 创建或聚焦动画，支持自动命名。                                                        |
| `set_property_keys`            | ✅ 已实现 | 底层轨道写入辅助（支持 float/color/vector2D）。                                       |

说明：
- `animation_target` / `widget_target` 复用当前 UMG 目标资产，命名自动纠错（不再出现 “animal” 拼写），并在缺失时自动创建。
- 写入仅做并集/覆盖，不做隐式删除；如需删除，请使用 `animation_delete_widget_keys` 并显式传入 `confirm_delete=true`。
- 所有动画指令都会返回有价值的计数/时间线信息，便于 AI 复述与复现。

## UMG 材质 (Material) API 实现状态 (New: 五大核心能力)

| 分类             | API 名称                       |   状态   | 描述                                                                   |
| ---------------- | ------------------------------ | :------: | ---------------------------------------------------------------------- |
| **P0: 上下文**   | `material_set_target`          |    ✅     | **锚点**：指定当前编辑的材质资产（支持自动创建/重定向）。              |
| **P1: 输入定义** | `material_define_variable`     |    ✅     | 定义外部接口参数（定义而非连线）。支持 Scalar, Vector, Texture。       |
| **P2: 符号放置** | `material_add_node`            |    ✅     | **拖拽符号**：将 UE5 库中的符号放置到图表并分配实例句柄（Handle）。    |
|                  | `material_get_graph`           |    ✅     | 查询图表中的所有节点句柄及其状态。                                     |
| **P3: 连接拓扑** | `material_connect_nodes`       |    ✅     | **自然连接**：建立节点间的逻辑映射（A -> B），模拟功能性嵌套。         |
|                  | `material_connect_pins`        |    ✅     | **外科连线**：在复杂拓扑下手动连接特定的输入引脚。                     |
| **P4: 符号检索** | `material_search_library`      | 🚧 计划中 | 快速检索 UE5 材质表达式库中的符号（Symbol）。                          |
| **P5: 细节注入** | `material_set_hlsl_node_io`    |    ✅     | **战术细节**：编写 Custom 节点的 HLSL 代码并实时同步 IO 引脚。         |
|                  | `material_set_node_properties` |    ✅     | **属性编辑**：设置常规节点（如 Constant, TextureSample）的内部属性值。 |
| **生命周期**     | `material_compile_asset`       |    ✅     | 提交更改并分析 HLSL 报错。                                             |
|                  | `material_delete`              |    ✅     | 根据唯一句柄删除实例。                                                 |
| **维护**         | `material_get_pins`            |    ✅     | 内省特定节点句柄的引脚。                                               |

## UMG HLSL MCP API 实现状态（新增：面向 UMG 的文本编辑闭环）

| 命令                | 状态 | 描述                                                                                         |
| ------------------- | :--: | -------------------------------------------------------------------------------------------- |
| `hlsl_set_target`   |  ✅   | 锁定/创建 HLSL 目标材质。校验 UI 材质域 + 单 Custom 节点拓扑；不匹配时可先返回覆写确认。     |
| `hlsl_get`          |  ✅   | 读取当前 HLSL 代码和结构化输入参数信息。                                                      |
| `hlsl_set`          |  ✅   | 对 HLSL 与/或参数做增量更新。删除必须显式标记（`delete: true`），避免误删。                   |
| `hlsl_compile`      |  ✅   | 编译当前 HLSL 目标并返回精简诊断信息，便于 AI 后处理。                                        |

### HLSL 协议约定（UMG 垂直优化）

- 材质被视作单一 HLSL 程序。
- 后端约定 HLSL 返回 `float4`。
- 输出自动连线固定为：`.rgb -> FinalColor`，`.a -> Opacity`。
- 输入参数以结构化描述返回（如 `name`、`kind`、`source_handle`），便于 AI 学习与复用。
- `hlsl_set` 采用“易写难删”策略：写入简单，删除必须显式声明。

## UMG 样式与主题 (Style & Theming) API 实现状态 (New)

| 类别     | API 名称             | status | 描述                                          |
| -------- | -------------------- | :----: | --------------------------------------------- |
| **样式** | `set_widget_style`   |   ⏳    | 设置特定控件的详细样式（例如 FButtonStyle）。 |
| **主题** | `apply_global_theme` |   ⏳    | 根据主题配置批量应用样式、字体和配色。        |
| **资源** | `style_create_asset` |   ⏳    | 创建独立的 Slate 控件样式资源。               |

## 项目状态更新 (Project Status)

很抱歉，因为google取消了我的学生优惠，因此我空闲的时间无法推进此项目。但是不要担心，因为这迫使我使用其他平台的AI，也许能加速此插件支持其他平台的进度。具体来说，我们接下来的开发计划是在fab版中添加各种API key的接入因为我认为各位都是懂技术的，但是真正需要UMG的应该是不懂技术的美工。无论如何，如果你能在fab上购买一份付费版的UMG MCP，那就再好不过了，也许我就可以直接订阅Gemini AI进行开发了？
