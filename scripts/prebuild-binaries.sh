#!/bin/bash
# Pre-build ScratchBird binaries for test server setup
# This script builds only the essential binaries needed for the test server
# Usage: ./scripts/prebuild-binaries.sh

set -e

echo "═══════════════════════════════════════════════════════════════"
echo "  ScratchBird Binary Pre-Build"
echo "═══════════════════════════════════════════════════════════════"
echo ""

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Number of parallel jobs
JOBS=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

echo "📋 Build Configuration:"
echo "  Build Type: Release"
echo "  Parallel Jobs: $JOBS"
echo "  Install Prefix: /usr/local"
echo ""

# Check for required tools
echo "🔍 Checking build dependencies..."
MISSING_DEPS=""

for tool in cmake g++ make; do
    if ! command -v $tool &> /dev/null; then
        MISSING_DEPS="$MISSING_DEPS $tool"
    fi
done

if [ -n "$MISSING_DEPS" ]; then
    echo -e "${RED}❌ Missing required tools:$MISSING_DEPS${NC}"
    echo ""
    echo "Install with:"
    echo "  Ubuntu/Debian: sudo apt-get install build-essential cmake"
    echo "  CentOS/RHEL:   sudo yum install gcc-c++ cmake make"
    exit 1
fi

echo -e "${GREEN}✅ All build tools found${NC}"
echo ""

# Create build directory
echo "🔨 Configuring build..."
mkdir -p build
cd build

# Configure with minimal options (faster build)
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr/local \
    -DSCRATCHBIRD_ENABLE_TLS=ON \
    -DSCRATCHBIRD_BUILD_TESTS=OFF \
    2>&1 | tee cmake.log | tail -20

if [ $? -ne 0 ]; then
    echo -e "${RED}❌ CMake configuration failed${NC}"
    echo "See build/cmake.log for details"
    exit 1
fi

echo ""
echo -e "${GREEN}✅ CMake configuration successful${NC}"
echo ""

# Build essential targets only
echo "🏗️  Building essential binaries..."
echo "  Targets: sb_server, sb_isql, sb_admin, sb_createdb"
echo ""

# Build with progress output
cmake --build . --target sb_server sb_isql sb_admin -j$JOBS 2>&1 | \
    tee build.log | \
    grep -E "(Building|Linking|error:|warning:|\[.*%\])" || true

if [ $? -ne 0 ]; then
    echo -e "${RED}❌ Build failed${NC}"
    echo "See build/build.log for details"
    exit 1
fi

echo ""
echo -e "${GREEN}✅ Build completed${NC}"
echo ""

# Verify binaries
echo "🔍 Verifying binaries..."
BINARIES=("sb_server" "sb_isql" "sb_admin")
MISSING_BINARIES=""

for binary in "${BINARIES[@]}"; do
    if [ -f "bin/$binary" ]; then
        echo -e "${GREEN}✅ $binary${NC}"
    else
        echo -e "${RED}❌ $binary - MISSING${NC}"
        MISSING_BINARIES="$MISSING_BINARIES $binary"
    fi
done

if [ -n "$MISSING_BINARIES" ]; then
    echo ""
    echo -e "${RED}❌ Some binaries are missing:$MISSING_BINARIES${NC}"
    echo "Build may have failed. Check build/build.log"
    exit 1
fi

echo ""
echo "📊 Binary Sizes:"
ls -lh bin/sb_server bin/sb_isql bin/sb_admin 2>/dev/null | awk '{print "  " $9 ": " $5}'

echo ""
echo "═══════════════════════════════════════════════════════════════"
echo -e "${GREEN}  ✅ BINARIES READY${NC}"
echo "═══════════════════════════════════════════════════════════════"
echo ""
echo "Binaries location: $(pwd)/bin/"
echo ""
echo "Next steps:"
echo "  1. Run test server setup:"
echo "     sudo ./scripts/setup-test-server.sh"
echo ""
echo "  Or run directly:"
echo "     sudo ./bin/sb_server --database=/var/scratchbird/testdb/testdb.sdb \\"
echo "         --port=13092 --bind=127.0.0.1"
echo ""
