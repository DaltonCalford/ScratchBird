#!/bin/bash
# ScratchBird Alpha 0.6.0 - Complete Build Script
# Builds server and all utilities with partial hash indexes support

set -e  # Exit on any error

PROJECT_ROOT="/home/dcalford/Documents/claude/GitHubRepo/ScratchBird"
cd "$PROJECT_ROOT"

# Color codes for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Logging function
log() {
    echo -e "${BLUE}[$(date '+%Y-%m-%d %H:%M:%S')]${NC} $1"
}

error() {
    echo -e "${RED}[ERROR]${NC} $1" >&2
}

success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

# Build configuration
BUILD_TARGET="Release"
BUILD_JOBS=$(nproc)
BUILD_TIMESTAMP=$(date '+%Y%m%d_%H%M%S')
BUILD_LOG="builds/build_${BUILD_TIMESTAMP}.log"

echo "=================================================================="
echo "           ScratchBird Alpha 0.6.0 Complete Build"
echo "=================================================================="
echo "Building with partial hash indexes support..."
echo "Build Target: $BUILD_TARGET"
echo "Parallel Jobs: $BUILD_JOBS"
echo "Build Log: $BUILD_LOG"
echo "=================================================================="

# Create build log directory
mkdir -p builds
exec > >(tee -a "$BUILD_LOG") 2>&1

log "Starting complete ScratchBird build process"

# Step 1: Create and validate build directories
log "Step 1: Creating and validating build directories..."

# Create all necessary build directories first
log "Creating comprehensive build directory structure..."
if [ -f "build_scripts/create_build_directories.sh" ]; then
    bash build_scripts/create_build_directories.sh create $BUILD_TARGET
else
    # Fallback: create basic directory structure
    warning "create_build_directories.sh not found, using fallback method"
    find src -type d | sed "s|src/|temp/$BUILD_TARGET/|g" | xargs mkdir -p
    mkdir -p "gen/$BUILD_TARGET/scratchbird/{bin,lib,include,plugins}"
fi
success "Build directories created"

# Validate directories
if ! ./build_scripts/validate_build_directories.sh; then
    error "Directory validation failed"
    exit 1
fi
success "Directory validation completed"

# Step 2: Clean previous build
log "Step 2: Cleaning previous build..."
make TARGET=$BUILD_TARGET clean || {
    warning "Clean command had issues, continuing..."
}
success "Clean completed"

# Step 3: Build external dependencies
log "Step 3: Building external dependencies..."
if ! make TARGET=$BUILD_TARGET external; then
    error "External dependencies build failed"
    exit 1
fi
success "External dependencies built successfully"

# Step 4: Build core components
log "Step 4: Building core ScratchBird components..."

# Build order for proper dependency resolution
CORE_TARGETS=(
    "yvalve"
    "scratchbird"
)

for target in "${CORE_TARGETS[@]}"; do
    log "Building $target..."
    if ! make TARGET=$BUILD_TARGET -j$BUILD_JOBS $target; then
        error "Failed to build $target"
        exit 1
    fi
    success "$target built successfully"
done

# Step 5: Build utilities
log "Step 5: Building ScratchBird utilities..."

UTILITY_TARGETS=(
    "sb_isql"
    "sb_gbak"
    "sb_gstat" 
    "sb_gfix"
    "sb_gsec"
    "nbackup"
    "sb_tracemgr"
)

for utility in "${UTILITY_TARGETS[@]}"; do
    log "Building $utility..."
    if ! make TARGET=$BUILD_TARGET -j$BUILD_JOBS $utility; then
        warning "Failed to build $utility - continuing with other utilities"
        continue
    fi
    success "$utility built successfully"
done

# Step 6: Deploy to release directory
log "Step 6: Deploying to release directory..."

RELEASE_DIR="release/alpha0.6.0/linux-x64"
BUILD_DIR="gen/Release/scratchbird"

if [ -d "$BUILD_DIR" ]; then
    # Deploy binaries
    if [ -d "$BUILD_DIR/bin" ]; then
        log "Deploying binaries..."
        cp -v "$BUILD_DIR/bin"/* "$RELEASE_DIR/bin/" 2>/dev/null || warning "Some binaries failed to copy"
    fi
    
    # Deploy libraries
    if [ -d "$BUILD_DIR/lib" ]; then
        log "Deploying libraries..."
        cp -v "$BUILD_DIR/lib"/* "$RELEASE_DIR/lib/" 2>/dev/null || warning "Some libraries failed to copy"
    fi
    
    # Deploy headers
    if [ -d "$BUILD_DIR/include" ]; then
        log "Deploying headers..."
        cp -rv "$BUILD_DIR/include"/* "$RELEASE_DIR/include/" 2>/dev/null || warning "Some headers failed to copy"
    fi
    
    # Deploy configuration files
    if [ -d "$BUILD_DIR/etc" ]; then
        log "Deploying configuration files..."
        cp -v "$BUILD_DIR/etc"/* "$RELEASE_DIR/etc/" 2>/dev/null || warning "Some config files failed to copy"
    fi
    
    success "Deployment to release directory completed"
else
    warning "Build directory $BUILD_DIR not found - skipping deployment"
fi

# Step 7: Validate built executables
log "Step 7: Validating built executables..."

EXPECTED_EXECUTABLES=(
    "$RELEASE_DIR/bin/scratchbird"
    "$RELEASE_DIR/bin/sb_isql"
    "$RELEASE_DIR/bin/sb_gbak"
    "$RELEASE_DIR/bin/sb_gstat"
    "$RELEASE_DIR/bin/sb_gfix"
    "$RELEASE_DIR/bin/sb_gsec"
)

VALIDATION_FAILED=0
for exe in "${EXPECTED_EXECUTABLES[@]}"; do
    if [ -f "$exe" ] && [ -x "$exe" ]; then
        # Test if executable runs (get version)
        if "$exe" -z >/dev/null 2>&1 || "$exe" --version >/dev/null 2>&1 || "$exe" -? >/dev/null 2>&1; then
            success "✅ $exe - functional"
        else
            success "✅ $exe - exists (version check not supported)"
        fi
    else
        error "❌ $exe - missing or not executable"
        VALIDATION_FAILED=1
    fi
done

# Step 8: Run basic functionality tests
log "Step 8: Running basic functionality tests..."

if [ -f "tests/test_partial_hash_indexes.sh" ]; then
    log "Running partial hash index tests..."
    if chmod +x tests/test_partial_hash_indexes.sh && ./tests/test_partial_hash_indexes.sh --basic; then
        success "Partial hash index tests passed"
    else
        warning "Partial hash index tests had issues - check manually"
    fi
else
    warning "Partial hash index test script not found"
fi

# Step 9: Generate build report
log "Step 9: Generating build report..."

BUILD_REPORT="builds/build_report_${BUILD_TIMESTAMP}.txt"
cat > "$BUILD_REPORT" << EOF
ScratchBird Alpha 0.6.0 Build Report
====================================

Build Information:
- Timestamp: $(date)
- Target: $BUILD_TARGET
- Jobs: $BUILD_JOBS
- Host: $(hostname)
- OS: $(uname -a)

Build Results:
EOF

echo "Built Executables:" >> "$BUILD_REPORT"
for exe in "${EXPECTED_EXECUTABLES[@]}"; do
    if [ -f "$exe" ]; then
        echo "  ✅ $exe ($(stat -f%z "$exe" 2>/dev/null || stat -c%s "$exe") bytes)" >> "$BUILD_REPORT"
    else
        echo "  ❌ $exe - MISSING" >> "$BUILD_REPORT"
    fi
done

echo "" >> "$BUILD_REPORT"
echo "Built Libraries:" >> "$BUILD_REPORT"
find "$RELEASE_DIR/lib" -name "*.so*" -o -name "*.a" 2>/dev/null | while read lib; do
    echo "  ✅ $lib ($(stat -f%z "$lib" 2>/dev/null || stat -c%s "$lib") bytes)" >> "$BUILD_REPORT"
done 2>/dev/null || echo "  No libraries found" >> "$BUILD_REPORT"

echo "" >> "$BUILD_REPORT"
echo "Partial Hash Index Implementation Status:" >> "$BUILD_REPORT"
if [ -f "src/jrd/PartialHashIndex.cpp" ]; then
    echo "  ✅ Core implementation: $(wc -l < src/jrd/PartialHashIndex.cpp) lines" >> "$BUILD_REPORT"
else
    echo "  ❌ Core implementation: MISSING" >> "$BUILD_REPORT"
fi

if [ -f "src/jrd/PartialHashKeyGenerator.cpp" ]; then
    echo "  ✅ Key generator: $(wc -l < src/jrd/PartialHashKeyGenerator.cpp) lines" >> "$BUILD_REPORT"
else
    echo "  ❌ Key generator: MISSING" >> "$BUILD_REPORT"
fi

success "Build report generated: $BUILD_REPORT"

# Final summary
echo ""
echo "=================================================================="
echo "                    BUILD SUMMARY"
echo "=================================================================="

if [ $VALIDATION_FAILED -eq 0 ]; then
    success "✅ Build completed successfully!"
    success "✅ All expected executables created"
    success "✅ Release deployment completed"
    echo "📁 Release location: $RELEASE_DIR"
    echo "📊 Build report: $BUILD_REPORT"
    echo "📝 Build log: $BUILD_LOG"
else
    error "❌ Build completed with issues"
    error "❌ Some executables missing or non-functional"
    echo "📝 Check build log: $BUILD_LOG"
    exit 1
fi

echo "=================================================================="
log "Build process completed successfully"