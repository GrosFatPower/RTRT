@echo off
setlocal

echo === Build Script for RTRT on Windows x64 ===
echo.
echo Checking prerequisites...

where cmake >nul 2>nul
if errorlevel 1 (
    echo Error: CMake not detected. Please install CMake and add it to PATH.
    exit /b 1
)

where git >nul 2>nul
if errorlevel 1 (
    echo Error: Git not detected. Please install Git and add it to PATH.
    exit /b 1
)

echo All prerequisites are installed

if not exist build (
    mkdir build
)

cd build
if errorlevel 1 (
    echo Error: Unable to enter build directory
    exit /b 1
)

echo.
echo === Project configuration ===
cmake .. ^
    -A x64 ^
    -DCMAKE_BUILD_TYPE=Debug ^
    -DCMAKE_VERBOSE_MAKEFILE=OFF

if errorlevel 1 (
    echo Error while configurating CMake
    exit /b 1
)

echo.
echo === Compilation ===
cmake --build . --config Debug --parallel

if errorlevel 1 (
    echo Compilation failed
    exit /b 1
)

echo.
echo Compilation succeeded!
echo.

set EXECUTABLE_PATH=Debug\RenderLab.exe

if exist "%EXECUTABLE_PATH%" (
    echo Executable found: %EXECUTABLE_PATH%
) else (
    echo Error: Executable not found in Debug
    exit /b 1
)

echo === Build success ! ===
endlocal
