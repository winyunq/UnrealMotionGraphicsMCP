# Gemini.md - UMG Agent Development Log

## 项目目标

开发一个针对 Unreal Engine 5 (UE5) UMG (Unreal Motion Graphics) 界面的 AI Agent，使其能够感知和操作 UMG UI。

## 插件信息

*   **插件名称:** `UmgMcp`
*   **插件类型:** C++ 引擎/项目插件 (目前作为项目插件安装在 `D:\UE5Project\Test\Plugins\UmgMcp\`)
*   **核心功能:**
    *   提供与 UE5 UMG 交互的 C++ 接口。
    *   通过 HTTP 服务器 (未来实现) 暴露这些功能，供外部 Python 客户端 (`umg_mcp_client.py`) 调用。

## 关键文件

*   `UmgMcp.uplugin`: 插件描述文件。
*   `Source/UmgMcp/UmgMcp.Build.cs`: 模块构建文件，定义依赖项。
*   `Source/UmgMcp/Public/UmgMcp.h`: 模块头文件，定义 `FUmgMcpModule` 类。
*   `Source/UmgMcp/Public/UmgMcpConfig.h`: 配置文件头文件，定义硬编码的默认端口。
*   `Source/UmgMcp/Private/UmgMcp.cpp`: 模块实现文件，包含 `StartupModule()` 和 `ShutdownModule()`，并在启动时打印“UMG agent Start!”和从 `UmgMcpConfig.h` 读取的端口。
*   `install_to_engine.bat`: 用于将插件安装到引擎或项目 `Plugins` 目录的批处理脚本。
*   `umg_mcp_client.py`: 外部 Python 客户端库，用于与插件通信。

## 当前状态与进展

1.  **插件架构搭建完成:** 按照标准 UE 插件结构创建了所有必要的目录和文件。
2.  **C++ 模块骨架实现:** `UmgMcp.h` 和 `UmgMcp.cpp` 已创建，实现了基本的模块启动和关闭功能，并在启动时打印日志。
3.  **端口硬编码:** 端口号已硬编码到 `UmgMcpConfig.h` 中，并在 `UmgMcp.cpp` 中读取并打印。
4.  **安装脚本:** `install_to_engine.bat` 已创建，支持将插件复制到引擎或项目目录，并支持参数传递。
5.  **客户端端口同步:** `umg_mcp_client.py` 中的默认端口已更新为 `54517`。
6.  **编译问题排查中:**
    *   最初的编译错误 (`FUmgMcpModule` 未定义) 已修复。
    *   当前的编译错误 (`LNK2019` 无法解析外部符号 `IPluginManager::Get()`) 已通过添加 `Projects` 模块依赖项修复。
    *   **当前面临的问题:** 插件编译成功后，在 UE5 中加载时仍然提示“Incompatible or missing module”错误，这表明存在引擎版本不一致的问题。

## 下一步行动

*   **用户操作:**
    1.  **验证项目关联:** 确保 `Test.uproject` 与正确的 UE5.5 版本关联。
    2.  **清理并重新构建:** 删除项目 `Binaries`、`Intermediate`、`Saved` 文件夹，然后重新生成 Visual Studio 项目文件，并在 Visual Studio 中执行“Clean”和“Build”操作。
    3.  在 Unreal Editor 中打开项目，并检查输出日志。

*   **AI 准备:** 等待用户在 `Test` 目录下重新启动。
