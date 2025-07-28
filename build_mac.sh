#!/bin/bash

# Build script for mTroll on macOS

set -e

echo "Building mTroll for macOS..."

# Set up Qt5 environment
export PATH="/opt/homebrew/opt/qt@5/bin:$PATH"
export Qt5_DIR="/opt/homebrew/opt/qt@5/lib/cmake/Qt5"

# Clean and create build directory
rm -rf build
mkdir -p build
cd build

echo "Configuring with CMake..."
# Configure with CMake
cmake .. -DCMAKE_BUILD_TYPE=Release

echo "Building..."
# Build
make -j$(sysctl -n hw.ncpu)

echo ""
echo "✅ Build complete!"
echo "📁 Binary location: $(pwd)/bin/mTroll"
echo ""
ls -la bin/