#!/bin/bash
# Build script for VHS RF Viewer
# This script builds the Qt-based RF viewer application

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}=== VHS RF Viewer Build Script ===${NC}"

# Check for Qt6
if ! command -v qmake6 &> /dev/null && ! command -v qmake &> /dev/null; then
    echo -e "${RED}Error: Qt6 not found${NC}"
    echo "Please install Qt6 development packages:"
    echo "  Ubuntu/Debian: sudo apt-get install qt6-base-dev libqt6opengl6-dev qt6-base-dev-tools"
    echo "  Fedora: sudo dnf install qt6-qtbase-devel"
    echo "  Arch: sudo pacman -S qt6-base"
    exit 1
fi

# Check for FFTW3
if ! pkg-config --exists fftw3f; then
    echo -e "${YELLOW}Warning: FFTW3 (float) not found${NC}"
    echo "Install FFTW3 for better performance:"
    echo "  Ubuntu/Debian: sudo apt-get install libfftw3-dev"
    echo "  Fedora: sudo dnf install fftw-devel"
    echo "  Arch: sudo pacman -S fftw"
    echo ""
fi

# Create build directory
BUILD_DIR="build"
if [ ! -d "$BUILD_DIR" ]; then
    mkdir -p "$BUILD_DIR"
fi

cd "$BUILD_DIR"

# Configure with CMake
echo -e "${GREEN}Configuring with CMake...${NC}"
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_VIEWER=ON \
    -DUSE_FFTW3=ON \
    -DBUILD_TESTS=OFF \
    -DBUILD_TOOLS=OFF

# Build
echo -e "${GREEN}Building...${NC}"
cmake --build . --parallel $(nproc)

# Check if build succeeded
if [ -f "viewer/vhs-rf-viewer" ]; then
    echo -e "${GREEN}=== Build successful! ===${NC}"
    echo "Executable: $BUILD_DIR/viewer/vhs-rf-viewer"
    echo ""
    echo "To run:"
    echo "  cd $BUILD_DIR"
    echo "  ./viewer/vhs-rf-viewer"
else
    echo -e "${RED}Build failed!${NC}"
    exit 1
fi
