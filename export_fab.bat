@echo off
setlocal

set "SCRIPT_DIR=%~dp0"
python "%SCRIPT_DIR%export_fab.py" %*
set "EXIT_CODE=%ERRORLEVEL%"

if not "%EXIT_CODE%"=="0" (
  echo [ERROR] export_fab.py failed with exit code %EXIT_CODE%.
  exit /b %EXIT_CODE%
)

echo [OK] export_fab.py completed.
exit /b 0
