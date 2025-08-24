#!/bin/bash

echo "=== Build Script for RTRT on macOS (Apple Silicon) ==="

ARCH=$(uname -m)
echo "Architecture : $ARCH"

if [[ "$ARCH" != "arm64" ]]; then
    echo "Warning: This script is optimised for Apple Silicon (M1/M2/M3/M4)"
    echo "Current architecture ($ARCH) will use optimisations for x64"
fi

echo "Checking prerequisites..."

if ! command -v cmake &> /dev/null; then
    echo "❌ CMake not detected. Please install it: brew install cmake"
    exit 1
fi

if ! command -v git &> /dev/null; then
    echo "❌ Git no detected. Please install it: brew install git"
    exit 1
fi

echo "✅ All prerequistes are installed"

if [ ! -d "build" ]; then
    mkdir build
fi

cd build

echo ""
echo "=== Project configuration ==="
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_VERBOSE_MAKEFILE=ON \
    -DCMAKE_OSX_ARCHITECTURES=arm64

if [ $? -ne 0 ]; then
    echo "❌ Error while configurating CMake"
    exit 1
fi

echo ""
echo "=== Compilation ==="
cmake --build . --config Release -j$(sysctl -n hw.ncpu)

if [ $? -ne 0 ]; then
    echo "❌ Compilation failed"
    exit 1
fi

echo ""
echo "✅ Compilation succeeded!"
echo ""

if [ -f "bin/Release/RT_Renderer" ]; then
    echo "✅ Executable found: bin/Release/RT_Renderer"
    
    echo ""
    echo "=== Informations sur l'exécutable ==="
    file bin/Release/RT_Renderer
    
    echo ""
    echo "=== Dynamic dependencies ==="
    otool -L bin/Release/RTRT   
else
    echo "❌ Executable non found in bin/Release"
    exit 1
fi

echo "=== Build success ! ==="
