#!/usr/bin/env bash
set -euo pipefail

echo "=== Mortar: Installing system prerequisites ==="

# System packages
sudo apt-get update
sudo apt-get install -y \
    cmake \
    build-essential \
    pkg-config \
    libvulkan-dev \
    vulkan-tools \
    vulkan-validationlayers \
    libwayland-dev \
    libxkbcommon-dev \
    xorg-dev \
    glslc \
    curl \
    zip \
    unzip \
    tar \
    git \
    ninja-build

echo "=== Bootstrapping vcpkg ==="

VCPKG_DIR="/home/chris/workspace/vcpkg"

if [ ! -d "$VCPKG_DIR" ]; then
    git clone https://github.com/microsoft/vcpkg.git "$VCPKG_DIR"
fi

if [ ! -f "$VCPKG_DIR/vcpkg" ]; then
    "$VCPKG_DIR/bootstrap-vcpkg.sh" -disableMetrics
fi

echo "=== Verifying installations ==="
cmake --version
glslc --version
"$VCPKG_DIR/vcpkg" version

echo ""
echo "=== Setup complete ==="
echo "Run: export VCPKG_ROOT=$VCPKG_DIR"
echo "Then: cmake --preset default && cmake --build --preset default"
