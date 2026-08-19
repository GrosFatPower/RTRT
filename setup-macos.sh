#!/bin/bash

echo "=== Setup Script for RTRT on macOS ==="

ARCH=$(uname -m)
echo "Architecture : $ARCH"

echo "Checking prerequisites..."

if ! command -v git &> /dev/null; then
    echo "Error: Git not detected. Please install it: brew install git"
    exit 1
fi

if ! command -v cmake &> /dev/null; then
    echo "Error: CMake not detected. Please install it: brew install cmake"
    exit 1
fi

if ! command -v pkg-config &> /dev/null; then
    echo "Error: pkg-config not detected. Please install it: brew install pkg-config"
    exit 1
fi

if ! pkg-config --exists glew; then
    echo "Error: GLEW not detected. Please install it: brew install glew"
    exit 1
fi

echo "All prerequisites are installed"

echo ""
echo "=== Git submodules ==="
git submodule update --init --recursive

if [ $? -ne 0 ]; then
    echo "Error while updating git submodules"
    exit 1
fi

if [ ! -d "build" ]; then
    mkdir build
fi

cd build

echo ""
echo "=== Initial project configuration ==="

if [[ "$ARCH" == "arm64" ]]; then
    cmake .. \
        -DCMAKE_BUILD_TYPE=Debug \
        -DCMAKE_VERBOSE_MAKEFILE=OFF \
        -DCMAKE_OSX_ARCHITECTURES=arm64
else
    cmake .. \
        -DCMAKE_BUILD_TYPE=Debug \
        -DCMAKE_VERBOSE_MAKEFILE=OFF
fi

if [ $? -ne 0 ]; then
    echo "Error while configurating CMake"
    exit 1
fi

echo ""
echo "Setup succeeded!"
echo "You can now run ./build-debug-macos.sh or ./build-release-macos.sh"
