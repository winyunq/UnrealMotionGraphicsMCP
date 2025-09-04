@echo off
setlocal

set "PLUGIN_NAME=UmgMcp"
set "CURRENT_PLUGIN_DIR=%~dp0"

rem --- Get Engine Path ---
if "%~1" == "" (
    :GET_ENGINE_PATH
    set "ENGINE_PATH="
    set /p ENGINE_PATH="Enter your Unreal Engine installation path (e.g., C:\Program Files\Epic Games\UE_5.3): "
) else (
    set "ENGINE_PATH=%~1"
)

if not exist "%ENGINE_PATH%\Engine\Plugins\Developer\" (
    echo Error: The specified Unreal Engine path does not seem to be valid or the Developer plugins directory does not exist.
    echo Please ensure you provide the root path to your UE installation.
    if "%~1" == "" (
        goto GET_ENGINE_PATH
    ) else (
        echo Installation failed. Exiting.
        exit /b 1
    )
)

set "TARGET_PLUGIN_DIR=%ENGINE_PATH%\Engine\Plugins\Developer\%PLUGIN_NAME%"

rem Remove existing plugin directory if it exists
if exist "%TARGET_PLUGIN_DIR%" (
    echo Removing existing plugin directory: "%TARGET_PLUGIN_DIR%"
    rmdir /s /q "%TARGET_PLUGIN_DIR%"
)

echo Copying %PLUGIN_NAME% to "%TARGET_PLUGIN_DIR%"
xcopy "%CURRENT_PLUGIN_DIR%" "%TARGET_PLUGIN_DIR%" /E /I /Y

if %ERRORLEVEL% equ 0 (
    echo %PLUGIN_NAME% installed successfully!
    echo You may need to rebuild your project or generate Visual Studio project files if you have C++ code.
) else (
    echo Failed to install %PLUGIN_NAME%.
)

pause
endlocal