#!/bin/bash

echo "🔍 ScratchBird Build Verification"
echo "================================="

# Check if CMake is available
if command -v cmake >/dev/null 2>&1; then
    echo "✅ CMake found: $(cmake --version | head -1)"
else
    echo "❌ CMake not found - install with: sudo apt install cmake"
fi

# Check if Make is available  
if command -v make >/dev/null 2>&1; then
    echo "✅ Make found: $(make --version | head -1)"
else
    echo "❌ Make not found - install with: sudo apt install build-essential"
fi

# Check if GCC is available
if command -v gcc >/dev/null 2>&1; then
    echo "✅ GCC found: $(gcc --version | head -1)"
else
    echo "❌ GCC not found - install with: sudo apt install gcc g++"
fi

# Check required libraries
echo
echo "📦 Checking required libraries..."

# Check for zlib
if ldconfig -p | grep -q libz.so; then
    echo "✅ zlib found"
else
    echo "❌ zlib not found - install with: sudo apt install zlib1g-dev"
fi

# Check for ICU
if ldconfig -p | grep -q libicu; then
    echo "✅ ICU libraries found"
else
    echo "❌ ICU not found - install with: sudo apt install libicu-dev"
fi

echo
echo "🏗️  Build Commands:"
echo "  mkdir -p build && cd build"
echo "  cmake .."
echo "  make -j\$(nproc)"
echo
echo "📦 Installation:"
echo "  sudo make install"
echo "  ./builds/install/install_scratchbird.sh"
