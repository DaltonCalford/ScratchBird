#!/bin/bash
# Build ScratchBird Production Binaries
# 
# This script builds ONLY the server binaries needed for deployment.
# It does NOT build tests.
#
# Usage: ./build-production.sh [install]

set -e

INSTALL_PREFIX="${INSTALL_PREFIX:-/opt/scratchbird}"
BUILD_DIR="build"
JOBS=$(nproc 2>/dev/null || echo 4)

echo "═══════════════════════════════════════════════════════════════"
echo "  ScratchBird Production Build"
echo "═══════════════════════════════════════════════════════════════"
echo ""
echo "Build Configuration:"
echo "  Type: Release"
echo "  Jobs: $JOBS"
echo "  Install: $INSTALL_PREFIX"
echo ""

# Check dependencies
echo "Checking dependencies..."
for cmd in cmake g++ make; do
    if ! command -v $cmd &> /dev/null; then
        echo "❌ Missing: $cmd"
        exit 1
    fi
done
echo "✅ All dependencies found"
echo ""

# Create build directory
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Configure - only essential components
echo "🔧 Configuring build..."
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$INSTALL_PREFIX" \
    -DSCRATCHBIRD_BUILD_TESTS=OFF \
    -DSCRATCHBIRD_ENABLE_TLS=ON

echo ""
echo "🏗️  Building production binaries..."
echo ""

# Build only server components
cmake --build . --target sb_server sb_isql sb_admin sb_backup sb_security -j$JOBS

echo ""
echo "✅ Build complete!"
echo ""

# Show binaries
echo "Built binaries:"
ls -lh bin/sb_* 2>/dev/null || echo "  (check bin/ directory)"
echo ""

# Optional install
if [ "$1" == "install" ]; then
    echo "📦 Installing to $INSTALL_PREFIX..."
    
    # Create install directories
    sudo mkdir -p "$INSTALL_PREFIX/bin"
    sudo mkdir -p "$INSTALL_PREFIX/lib"
    
    # Install binaries
    sudo cp bin/sb_* "$INSTALL_PREFIX/bin/" 2>/dev/null || true
    sudo cp lib/*.so "$INSTALL_PREFIX/lib/" 2>/dev/null || true
    
    # Set permissions
    sudo chmod +x "$INSTALL_PREFIX/bin/"*
    
    echo "✅ Installed to $INSTALL_PREFIX"
    echo ""
    echo "Add to PATH:"
    echo "  export PATH=$INSTALL_PREFIX/bin:\$PATH"
fi

echo ""
echo "═══════════════════════════════════════════════════════════════"
echo "Next steps:"
echo "  1. Deploy test server:"
echo "     sudo ./scripts/test-server-deploy.sh setup"
echo "     sudo ./scripts/test-server-deploy.sh start"
echo ""
echo "  2. Or run directly:"
echo "     sudo ./build/bin/sb_server --database=/var/scratchbird/testdb/testdb.sdb \\"
echo "         --port=13092 --bind=127.0.0.1"
echo "═══════════════════════════════════════════════════════════════"
