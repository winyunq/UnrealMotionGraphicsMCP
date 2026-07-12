[English](Readme.md)

<div align="center">
  <img src="Resources/Icon128.png" width="96" alt="UMG MCP logo">
  <h1>Unreal Engine 5.8 的 UMG MCP</h1>
  <p><strong>让 AI Agent 通过 MCP 工具创建、检查、重构并版本化 UMG UI。</strong></p>
  <p>
    <a href="https://winyunq.github.io/UnrealMotionGraphicsMCP/Document/index_zh.html">网页教程</a>
    · <a href="#安装">安装</a>
    · <a href="#连接-mcp-客户端">MCP 客户端</a>
    · <a href="https://www.fab.com/zh-cn/listings/f70dbcb0-11e4-46bf-b3f7-9f30ba2c9b71">Fab 商店</a>
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

UMG MCP 会把 Unreal Editor 里的 UMG 工作变成明确、可审查、可复现的工具调用。AI Agent 可以创建控件、修改布局、连接 Blueprint、编辑材质、驱动 UMG 动画，并用 JSON 对 UI 资产做导出、补丁和回写，而不是依赖截图或脆弱的自动点击。

README 只负责快速入口。完整安装流程、模型配置和使用教程都放在已经部署的网页文档里：

<table>
  <tr>
    <td><strong>推荐入口</strong></td>
    <td><a href="https://winyunq.github.io/UnrealMotionGraphicsMCP/Document/index_zh.html">打开中文网页教程</a></td>
  </tr>
  <tr>
    <td><strong>仓库内文档</strong></td>
    <td><a href="Document/index_zh.html">Document/index_zh.html</a> 和 <a href="Document/index.html">Document/index.html</a></td>
  </tr>
</table>

## 为什么需要它

| 普通 AI 做 UI 的问题 | UMG MCP 的方式 |
| --- | --- |
| Agent 只能看截图猜结构 | Agent 可以读取真实 Widget Tree、Slot、属性、材质和图表状态 |
| UI 修改难以 diff 和复现 | UMG 可以导出为结构化 JSON，再按补丁回写 |
| Blueprint 连线依赖大量点击 | BlueCode 和 MCP 命令可以直接创建节点、Pin、引用和连线 |
| 长会话很快耗尽上下文 | Fab 版提供更面向生产的打包和上下文管理能力 |

## 安装

当前包面向 **Unreal Engine 5.8 / Win64**。本插件描述文件里的 `EngineVersion` 是 `5.8.0`。

### 方案 A：Fab 版

如果你希望安装路径最短，并直接使用编辑器内工作流，优先选择 Fab 版。

1. 打开 [Fab 商店页面](https://www.fab.com/zh-cn/listings/f70dbcb0-11e4-46bf-b3f7-9f30ba2c9b71)。
2. 将插件加入 Epic Games / Unreal Engine 库。
3. 为 Unreal Engine 5.8 安装插件。
4. 打开项目，启用 `UmgMcp`，然后重启编辑器。
5. 进入插件设置，配置模型供应商。网页教程覆盖 Google OAuth、Gemini API、智谱 AI 和 OpenAI 兼容端点。

### 方案 B：源码版

如果你要检查协议、开发修复、或手动集成插件，可以使用源码版。

```powershell
cd D:\UE5Project\YourProject
mkdir Plugins
cd Plugins
git clone https://github.com/winyunq/UnrealMotionGraphicsMCP.git UmgMcp
```

然后用 Unreal Engine 5.8 打开项目，让 Unreal Build Tool 编译插件。如果把插件复制到别的项目，请保持最终目录名为 `UmgMcp`，除非你也同步修改 MCP 客户端配置。

## 连接 MCP 客户端

外部 MCP 客户端会启动 `Resources/Python` 下的 Python server。请先打开 Unreal Editor，让插件在本机 `127.0.0.1:55557` 上启动编辑器桥接。

把下面配置加入客户端设置中的 `mcpServers`，并把路径替换为你机器上的插件绝对路径：

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

注意：

- 路径必须指向 `Resources/Python`，不是仓库根目录。
- Windows JSON 路径里的反斜杠要写成 `D:\\Path\\To\\Project`。
- 如果系统找不到 `uv`，先安装 `uv`，或使用下面的本地虚拟环境流程。

可选的本地 Python 环境：

```powershell
cd D:\UE5Project\YourProject\Plugins\UmgMcp\Resources\Python
uv venv
.\.venv\Scripts\activate
uv pip install "mcp[cli]>=1.4.1" fastmcp uvicorn fastapi "pydantic>=2.6.1" requests
python -c "import mcp, fastapi, pydantic, requests; print('deps ok')"
```

## 验证安装

不打开项目时，可以先跑静态协议检查：

```powershell
cd D:\UE5Project\YourProject\Plugins\UmgMcp\Resources\Python
uv run python APITest\Umg_Widget_Protocol_Static_Check.py
uv run python APITest\Blueprint_MCP_Schema_Check.py
uv run python APITest\Material_Protocol_Static_Check.py
uv run python APITest\Animation_Protocol_Static_Check.py
```

打开 Unreal Editor 后，再跑编辑器联通测试：

```powershell
uv run python APITest\UE5_Editor_Imitation.py
```

如果编辑器侧测试无法连接，先确认插件已启用，并检查是否有其他进程占用了 `55557` 端口。

## 文档导航

| 目标 | 网页 | 仓库文件 |
| --- | --- | --- |
| 配置模型供应商 | [模型配置](https://winyunq.github.io/UnrealMotionGraphicsMCP/Document/model_config_zh.html) | [Document/model_config_zh.html](Document/model_config_zh.html) |
| 使用编辑器内对话工作流 | [对话工作流](https://winyunq.github.io/UnrealMotionGraphicsMCP/Document/chat_dialogue_zh.html) | [Document/chat_dialogue_zh.html](Document/chat_dialogue_zh.html) |
| 理解 MCP 支持方式 | [MCP 支持](https://winyunq.github.io/UnrealMotionGraphicsMCP/Document/mcp_support_zh.html) | [Document/mcp_support_zh.html](Document/mcp_support_zh.html) |
| Widget 命令协议 | - | [Document/UmgWidgetMcpProtocol.md](Document/UmgWidgetMcpProtocol.md) |
| Blueprint / BlueCode 协议 | - | [Document/BlueprintBluecodeProtocol.md](Document/BlueprintBluecodeProtocol.md) |
| Material 与 HLSL 协议 | - | [Document/MaterialMcpProtocol.md](Document/MaterialMcpProtocol.md) |
| Animation 协议 | - | [Document/AnimationMcpProtocol.md](Document/AnimationMcpProtocol.md) |
| Google OAuth 配置 | - | [Document/Google_OAuth_Setup.md](Document/Google_OAuth_Setup.md) |

## Agent 可以做什么

| 范围 | 示例 |
| --- | --- |
| UMG Widget | 创建控件、设置 Slot、重排树节点、替换子树、导出和回写 JSON |
| Blueprint / BlueCode | 创建图表、添加节点、连接 Pin、绑定控件引用、应用紧凑代码式语句 |
| Material | 创建 UI 材质、设置标量和向量参数、写入 HLSL 片段、验证材质字段 |
| Animation / Sequencer | 为 Render Transform、Opacity、Color 和 Timing 创建 UMG 动画轨道与关键帧 |
| Editor 工具 | 检查选中资产、读取项目状态、管理当前目标、运行聚焦测试 |

可以给 MCP Agent 这样的任务：

```text
创建 /Game/UI/WBP_LoginPanel：居中的登录卡片、邮箱和密码输入框、
主按钮，以及一个轻量材质背景。完成后导出最终 Widget JSON，
并总结本次修改用到了哪些 MCP 工具。
```

## 视频演示

- [设计 RTS UI](https://youtu.be/O86VCzxyF5o)
- [复现 UE5 编辑器](https://youtu.be/h_J70I0m4Ls)
- [在 UMG 编辑器中编辑 UMG](https://youtu.be/pq12x2MH1L4)
- [和 Gemini 对话编辑 UMG 文件](https://youtu.be/93_Fiil9nd8)

## 常见问题

| 现象 | 处理方式 |
| --- | --- |
| MCP 客户端报 `ConnectionRefusedError`、`WinError 10061` 或 `WinError 1225` | 先打开 Unreal Editor，启用 `UmgMcp`，重启编辑器，再重新连接 MCP 客户端。编辑器桥接监听 `127.0.0.1:55557`。 |
| 系统找不到 `uv` | 安装 `uv` 后重开终端，或使用上面的本地虚拟环境流程。 |
| 复制到其他项目后插件无法编译 | 确认项目使用 UE 5.8 Win64，清理该插件/项目的旧生成产物，然后让 Unreal 重新构建。 |
| 工具调用成功但改错了 Widget | 先设置当前 UMG 目标，或直接传完整资产路径，例如 `/Game/UI/WBP_LoginPanel.WBP_LoginPanel`。 |
| 不确定某个协议命令怎么写 | 工作流看网页教程，精确 payload 看 `Document/` 下的协议 markdown。 |

## Fab 与开源版

| 版本 | 适合场景 | 说明 |
| --- | --- | --- |
| 开源仓库 | 协议审查、研究、自定义集成、Issue 修复 | 核心 UMG MCP 行为会保留在这里。 |
| Fab 版 | 希望用市场安装包、编辑器内工作流和生产支持的美术与技术美术 | 提供更方便的安装路径、编辑器内工作流打包，以及更面向生产的支持。 |

核心协议改进会尽量回流到开源仓库。如果你从 Fab 包遇到问题，请在 Issue 中包含 UE 版本、插件版本、目标资产路径，以及失败的 MCP 工具 payload。

## 开发者计划

贡献有效修复、测试、文档或协议案例的开发者，可以申请 Fab 版访问权限用于验证。请在 Issue 中附上贡献链接和你要验证的场景，或联系维护者。

## 许可证

本仓库使用 [MIT License](LICENSE)。
