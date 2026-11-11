@echo off
setlocal
cd /d "%~dp0.."
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0PackageTetris.ps1" %*
if %ERRORLEVEL% neq 0 (
    echo.
    echo [ERROR] Packaging failed with exit code %ERRORLEVEL%.
    pause
    exit /b %ERRORLEVEL%
)
endlocal
