#!/bin/bash
# ScratchBird Alpha 0.6.0 - Build Directory Validation Script
# Ensures all required directories exist before building

set -e

PROJECT_ROOT="/home/dcalford/Documents/claude/GitHubRepo/ScratchBird"
cd "$PROJECT_ROOT"

echo "=== ScratchBird Build Directory Validation ==="
echo "Checking and creating required directories..."

# Required temporary build directories
REQUIRED_TEMP_DIRS=(
    "temp/Release/jrd"
    "temp/Release/dsql" 
    "temp/Release/msgs"
    "temp/Release/common"
    "temp/Release/remote"
    "temp/Release/auth"
    "temp/Release/plugins"
    "temp/Release/utilities"
    "temp/Release/isql"
    "temp/Release/burp"
    "temp/Release/alice"
    "temp/Release/gpre"
    "temp/Release/intl"
    "temp/Release/extlib"
    "temp/Release/yvalve"
    "temp/Release/lock"
    "temp/Release/config"
)

# Required release directories for alpha 0.6.0
REQUIRED_RELEASE_DIRS=(
    "release/alpha0.6.0/linux-x64/bin"
    "release/alpha0.6.0/linux-x64/lib"
    "release/alpha0.6.0/linux-x64/include"
    "release/alpha0.6.0/linux-x64/doc"
    "release/alpha0.6.0/linux-x64/examples"
    "release/alpha0.6.0/linux-x64/etc"
    "release/alpha0.6.0/linux-x86/bin"
    "release/alpha0.6.0/linux-x86/lib"
    "release/alpha0.6.0/linux-x86/include"
    "release/alpha0.6.0/linux-x86/doc"
    "release/alpha0.6.0/linux-x86/examples"
    "release/alpha0.6.0/linux-x86/etc"
    "release/alpha0.6.0/windows-x64/bin"
    "release/alpha0.6.0/windows-x64/lib"
    "release/alpha0.6.0/windows-x64/include"
    "release/alpha0.6.0/windows-x64/doc"
    "release/alpha0.6.0/windows-x64/examples"
    "release/alpha0.6.0/windows-x64/etc"
    "release/alpha0.6.0/darwin-x64/bin"
    "release/alpha0.6.0/darwin-x64/lib"
    "release/alpha0.6.0/darwin-x64/include"
    "release/alpha0.6.0/darwin-x64/doc"
    "release/alpha0.6.0/darwin-x64/examples"
    "release/alpha0.6.0/darwin-x64/etc"
)

# Required build working directories
REQUIRED_BUILD_DIRS=(
    "builds/temp"
    "builds/Release"
    "builds/Debug"
    "gen/temp"
)

# Create temporary build directories
echo "Creating temporary build directories..."
for dir in "${REQUIRED_TEMP_DIRS[@]}"; do
    if [ ! -d "$dir" ]; then
        echo "  Creating: $dir"
        mkdir -p "$dir"
    else
        echo "  Exists: $dir"
    fi
done

# Create release directories
echo "Creating release directories for alpha0.6.0..."
for dir in "${REQUIRED_RELEASE_DIRS[@]}"; do
    if [ ! -d "$dir" ]; then
        echo "  Creating: $dir"
        mkdir -p "$dir"
    else
        echo "  Exists: $dir"
    fi
done

# Create build working directories
echo "Creating build working directories..."
for dir in "${REQUIRED_BUILD_DIRS[@]}"; do
    if [ ! -d "$dir" ]; then
        echo "  Creating: $dir"
        mkdir -p "$dir"
    else
        echo "  Exists: $dir"
    fi
done

# Create special directories for partial hash indexes
PARTIAL_HASH_DIRS=(
    "temp/Release/jrd/partial_hash"
    "gen/temp/partial_hash_tests"
)

echo "Creating partial hash index directories..."
for dir in "${PARTIAL_HASH_DIRS[@]}"; do
    if [ ! -d "$dir" ]; then
        echo "  Creating: $dir"
        mkdir -p "$dir"
    else
        echo "  Exists: $dir"
    fi
done

# Validate critical files exist
CRITICAL_FILES=(
    "src/jrd/PartialHashIndex.cpp"
    "src/jrd/PartialHashIndex.h"
    "src/jrd/PartialHashKeyGenerator.cpp"
    "src/jrd/PartialHashKeyGenerator.h"
    "tests/test_partial_hash_indexes.sh"
    "Makefile"
    "gen/Makefile"
)

echo "Validating critical files exist..."
MISSING_FILES=0
for file in "${CRITICAL_FILES[@]}"; do
    if [ ! -f "$file" ]; then
        echo "  ❌ MISSING: $file"
        MISSING_FILES=$((MISSING_FILES + 1))
    else
        echo "  ✅ Found: $file"
    fi
done

# Set proper permissions on build scripts
echo "Setting executable permissions on build scripts..."
chmod +x build_scripts/*.sh 2>/dev/null || echo "  No build scripts found to set permissions"
chmod +x tests/*.sh 2>/dev/null || echo "  No test scripts found to set permissions"

# Validate external dependencies
echo "Checking external dependencies..."
EXTERNAL_DEPS=(
    "extern/btyacc/btyacc"
    "extern/decNumber/libdecFloat.a"
    "extern/editline/src/libedit.a"
)

MISSING_DEPS=0
for dep in "${EXTERNAL_DEPS[@]}"; do
    if [ ! -f "$dep" ]; then
        echo "  ⚠️  Missing external dependency: $dep"
        MISSING_DEPS=$((MISSING_DEPS + 1))
    else
        echo "  ✅ External dependency: $dep"
    fi
done

# Summary
echo ""
echo "=== Directory Validation Summary ==="
echo "Temporary directories: ${#REQUIRED_TEMP_DIRS[@]} created/validated"
echo "Release directories: ${#REQUIRED_RELEASE_DIRS[@]} created/validated"
echo "Build directories: ${#REQUIRED_BUILD_DIRS[@]} created/validated"
echo "Partial hash directories: ${#PARTIAL_HASH_DIRS[@]} created/validated"

if [ $MISSING_FILES -gt 0 ]; then
    echo "❌ Warning: $MISSING_FILES critical files missing"
    exit 1
fi

if [ $MISSING_DEPS -gt 0 ]; then
    echo "⚠️  Warning: $MISSING_DEPS external dependencies missing"
    echo "   Run 'make TARGET=Release external' to build dependencies"
fi

echo "✅ Directory validation complete - ready for build"
echo ""