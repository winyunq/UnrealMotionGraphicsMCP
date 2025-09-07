@echo off
setlocal

set "PLUGIN_NAME=UmgMcp"
set "CURRENT_PLUGIN_DIR=%~dp0"

rem --- Parse Arguments ---
set "ENGINE_PATH_ARG=%~1"
set "PORT_ARG=%~2"

rem --- Get Engine Path ---
if "%ENGINE_PATH_ARG%" == "" (
    :GET_ENGINE_PATH
    set "ENGINE_PATH="
    set /p ENGINE_PATH="Enter your Unreal Engine installation path (e.g., C:\Program Files\Epic Games\UE_5.3): "
) else (
    set "ENGINE_PATH=%ENGINE_PATH_ARG%"
)

if not exist "%ENGINE_PATH%\Engine\Plugins\Developer\" (
    echo Error: The specified Unreal Engine path does not seem to be valid or the Developer plugins directory does not exist.
    echo Please ensure you provide the root path to your UE installation.
    if "%ENGINE_PATH_ARG%" == "" (
        goto GET_ENGINE_PATH
    ) else (
        echo Installation failed. Exiting.
        exit /b 1
    )
)

set "TARGET_PLUGIN_DIR=%ENGINE_PATH%\Engine\Plugins\Developer\%PLUGIN_NAME%"

rem Remove existing plugin directory if it exists
if exist "%TARGET_PLUGIN_DIR%\" (
    echo Removing existing plugin directory: "%TARGET_PLUGIN_DIR%"
    rmdir /s /q "%TARGET_PLUGIN_DIR%"
)

echo Copying %PLUGIN_NAME% to "%TARGET_PLUGIN_DIR%"
xcopy "%CURRENT_PLUGIN_DIR%\" "%TARGET_PLUGIN_DIR%\" /E /I /Y

if %ERRORLEVEL% neq 0 (
    echo Failed to copy %PLUGIN_NAME%. Installation failed. Exiting.
    exit /b 1
)

rem --- Configure Port ---
set "DEFAULT_PORT=54517"
set "PORT=%DEFAULT_PORT%"
if not "%PORT_ARG%" == "" (
    set "PORT=%PORT_ARG%"
)

echo Configuring MCP server port to %PORT%...

set "UMG_MCP_CONFIG_H_PATH=%TARGET_PLUGIN_DIR%\Source\UmgMcp\Public\UmgMcpConfig.h"
set "MCP_CONFIG_PY_PATH=%TARGET_PLUGIN_DIR%\Resources\Python\mcp_config.py"

rem Generate UmgMcpConfig.h
(
echo #pragma once
echo.
echo #include "CoreMinimal.h"
echo.
echo // Configuration for the UMG-MCP plugin
echo namespace UmgMcpConfig
echo {
echo     const int32 DEFAULT_MCP_PORT = %PORT%; // Default port for the MCP server
echo }
) > "%UMG_MCP_CONFIG_H_PATH%"

rem Generate mcp_config.py
(
echo # mcp_config.py
echo # Configuration for the FastMCP server
echo.
echo MCP_HOST = "127.0.0.1"
echo MCP_PORT = %PORT%
) > "%MCP_CONFIG_PY_PATH%"

echo %PLUGIN_NAME% installed and configured successfully!
echo You may need to rebuild your project or generate Visual Studio project files if you have C++ code.

pause
endlocal
