[English](Readme.md)

<div align="center">
  <h1>UnrealMotionGraphicsMCP</h1>
  <p><strong>面向 Unreal Engine UMG 自动化制作的开源 MCP 工具集。</strong></p>
  <p>
    <a href="https://winyunq.github.io/UnrealMotionGraphicsMCP/Document/index_zh.html">网页教程</a>
    · <a href="#快速开始">快速开始</a>
    · <a href="#协议文档">协议文档</a>
    · <a href="https://github.com/winyunq/UnrealMotionGraphicsMCP/issues">Issues</a>
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

**UnrealMotionGraphicsMCP** 让 AI Agent 可以通过明确、可版本化的 MCP 命令编辑 Unreal UMG。它不要求 Agent 靠截图猜测，也不依赖脆弱的自动点击，而是把真实的编辑器状态暴露出来：Widget Tree、Slot 属性、Blueprint 图表操作、UI 材质、动画轨道，以及 UMG JSON 导出/回写。

开源插件是协议与编辑器桥接本体。它应该是可阅读、可调试、可复现的，适合研究、自定义管线、Issue 汇报和社区修复。

## 这个项目能做什么

| 范围 | 开源能力 |
| --- | --- |
| UMG Widget | 创建控件、读取树结构、设置属性和 Slot、重排子节点、替换子树、导出和回写 JSON |
| Blueprint / BlueCode | 创建图表节点、连接 Pin、绑定 Widget 引用、应用紧凑的代码式图表语句 |
| Material | 创建 UI 材质、设置参数、添加支持的表达式、测试面向 HLSL 的材质工作流 |
| Animation | 为 Render Transform、Opacity、Color 和 Timing 创建 UMG 动画轨道与关键帧 |
| Editor Bridge | 让外部 MCP 客户端通过 `127.0.0.1:55557` 的本机桥接调用 Unreal Editor |

## 为什么需要它

AI 辅助 UI 制作不能只靠截图。UMG 资产是结构化编辑器对象，严肃的自动化需要结构化访问。这个项目给 Agent 一个可以检查、测试和改进的协议表面，而不是把行为藏在黑盒 UI 操作里。

核心目标：

- **可观察的编辑**：每个关键 UI 修改都应该能表示为工具调用，并能作为数据审查。
- **可版本化的工作流**：UMG 可以导出为结构化 JSON，再按补丁回写。
- **编辑器原生行为**：命令通过 Unreal 编辑器系统执行，而不是模拟浏览器式点击。
- **开源协议优先**：协议、schema、测试和 bug fix 都应该保留在公开仓库。

## 快速开始

当前目标平台：**Unreal Engine 5.8 / Win64**。插件描述文件的 `EngineVersion` 是 `5.8.0`。

### 1. 安装插件

把开源插件克隆到 Unreal 项目：

```powershell
cd D:\UE5Project\YourProject
mkdir Plugins
cd Plugins
git clone https://github.com/winyunq/UnrealMotionGraphicsMCP.git UmgMcp
```

用 Unreal Engine 5.8 打开项目，必要时启用 `UmgMcp`，然后重启编辑器，让 Unreal Build Tool 编译插件。

### 2. 连接 MCP 客户端

先启动 Unreal Editor。插件会在本机监听 `127.0.0.1:55557`，Python MCP server 会把 MCP 客户端的工具调用转发到这个编辑器桥接。

把下面配置加入 MCP 客户端，并把路径替换为你项目里的插件绝对路径：

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

- `--directory` 必须指向 `Resources/Python`。
- Windows JSON 路径要转义，例如 `D:\\UE5Project\\...`。
- 使用 MCP 客户端时请保持 Unreal Editor 打开。

### 3. 验证安装

先跑静态协议检查：

```powershell
cd D:\UE5Project\YourProject\Plugins\UmgMcp\Resources\Python
uv run python APITest\Umg_Widget_Protocol_Static_Check.py
uv run python APITest\Blueprint_MCP_Schema_Check.py
uv run python APITest\Material_Protocol_Static_Check.py
uv run python APITest\Animation_Protocol_Static_Check.py
```

打开 Unreal Editor 后，再跑编辑器桥接冒烟测试：

```powershell
uv run python APITest\UE5_Editor_Imitation.py
```

如果你希望手动创建虚拟环境：

```powershell
cd D:\UE5Project\YourProject\Plugins\UmgMcp\Resources\Python
uv venv
.\.venv\Scripts\activate
uv pip install "mcp[cli]>=1.4.1" fastmcp uvicorn fastapi "pydantic>=2.6.1" requests
python -c "import mcp, fastapi, pydantic, requests; print('deps ok')"
```

## 文档

引导式安装与使用说明放在网页文档：

- [网页教程](https://winyunq.github.io/UnrealMotionGraphicsMCP/Document/index_zh.html)
- [English Web Manual](https://winyunq.github.io/UnrealMotionGraphicsMCP/)

协议文档保留在仓库内，便于和代码一起审查：

| 主题 | 文件 |
| --- | --- |
| Widget 命令和 JSON 回写 | [Document/UmgWidgetMcpProtocol.md](Document/UmgWidgetMcpProtocol.md) |
| Blueprint / BlueCode 图表操作 | [Document/BlueprintBluecodeProtocol.md](Document/BlueprintBluecodeProtocol.md) |
| Material 与 HLSL 相关操作 | [Document/MaterialMcpProtocol.md](Document/MaterialMcpProtocol.md) |
| Animation / Sequencer 操作 | [Document/AnimationMcpProtocol.md](Document/AnimationMcpProtocol.md) |

## Agent 任务示例

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
| `ConnectionRefusedError`、`WinError 10061` 或 `WinError 1225` | 先打开 Unreal Editor，启用 `UmgMcp`，重启编辑器，然后重新连接 MCP 客户端。 |
| 系统找不到 `uv` | 安装 `uv` 后重开终端，或使用上面的手动虚拟环境流程。 |
| 插件无法编译 | 确认项目使用 UE 5.8 Win64，清理该插件/项目的旧生成产物，然后让 Unreal 重新构建。 |
| Agent 改错了 Widget | 先设置当前 UMG 目标，或传完整资产路径，例如 `/Game/UI/WBP_LoginPanel.WBP_LoginPanel`。 |

## 开源与 Fab

这个仓库是 UMG MCP 协议与编辑器桥接的开源主页。Fab 包是可选的打包发行版，适合希望通过市场安装并获得更生产化封装的用户。核心协议修复应当保留在公开仓库，方便公开汇报、审查和复现。

Fab 链接：[UMG MCP on Fab Marketplace](https://www.fab.com/zh-cn/listings/f70dbcb0-11e4-46bf-b3f7-9f30ba2c9b71)

## 贡献

有效贡献包括可复现的 bug 报告、聚焦的协议测试、payload 示例、文档修复，以及行为清楚的小型编辑器侧修复。提交 Issue 时请尽量包含 Unreal Engine 版本、插件 commit、目标资产路径和失败的 MCP payload。

## 许可证

本仓库使用 [MIT License](LICENSE)。
