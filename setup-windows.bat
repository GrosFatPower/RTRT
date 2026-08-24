@echo off
setlocal

echo === Setup Script for RTRT on Windows x64 ===
echo.
echo Checking prerequisites...

where git >nul 2>nul
if errorlevel 1 (
    echo Error: Git not detected. Please install Git and add it to PATH.
    exit /b 1
)

where cmake >nul 2>nul
if errorlevel 1 (
    echo Error: CMake not detected. Please install CMake and add it to PATH.
    exit /b 1
)

echo All prerequisites are installed

echo.
echo === Git submodules ===
git submodule update --init --recursive

if errorlevel 1 (
    echo Error while updating git submodules
    exit /b 1
)

if not exist build (
    mkdir build
)

cd build
if errorlevel 1 (
    echo Error: Unable to enter build directory
    exit /b 1
)

echo.
echo === Initial project configuration ===
cmake .. ^
    -A x64 ^
    -DCMAKE_BUILD_TYPE=Debug ^
    -DCMAKE_VERBOSE_MAKEFILE=OFF

if errorlevel 1 (
    echo Error while configurating CMake
    exit /b 1
)

echo.
echo Setup succeeded!
echo You can now run ..\build-debug-windows.bat or ..\build-release-windows.bat
endlocal
