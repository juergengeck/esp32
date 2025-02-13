#!/bin/bash

# Exit on error
set -e

# Create build directory
mkdir -p build
cd build

# Check for mbedtls
if ! brew list mbedtls &>/dev/null; then
    echo "Installing mbedtls..."
    brew install mbedtls
fi

# Configure with CMake
cmake ..

# Get number of CPU cores on macOS
CORES=$(sysctl -n hw.ncpu)

# Build
make -j${CORES}

echo "Build complete! The tool is available at build/mac_tool"
echo
echo "Example usage:"
echo "  ./mac_tool generate                    # Generate new keys"
echo "  ./mac_tool export-keys owner.json      # Export keys"
echo "  ./mac_tool create-credential vc.json   # Create credential" 