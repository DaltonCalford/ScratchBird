#!/bin/bash

# ScratchBird Build System Adaptation Script
# Adapts Firebird build system for ScratchBird with tool renaming

set -e  # Exit on any error

TARGET_DIR="/home/dcalford/Documents/claude/GitHubRepo/ScratchBird"

echo "🔧 ScratchBird Build System Adaptation"
echo "Target: $TARGET_DIR"
echo

# Verify target directory exists
if [[ ! -d "$TARGET_DIR" ]]; then
    echo "❌ Error: Target directory not found: $TARGET_DIR"
    echo "Please run migrate_to_scratchbird.sh first"
    exit 1
fi

cd "$TARGET_DIR"

echo "📝 Adapting CMakeLists.txt files..."

# Function to update CMakeLists.txt for tool renaming
update_cmake_files() {
    find . -name "CMakeLists.txt" -type f | while read -r cmake_file; do
        if [[ -f "$cmake_file" ]]; then
            echo "  Updating: $cmake_file"
            
            # Update project names
            sed -i 's/project(Firebird)/project(ScratchBird)/g' "$cmake_file"
            sed -i 's/project(firebird)/project(scratchbird)/g' "$cmake_file"
            
            # Update tool names
            sed -i 's/set_target_properties(isql/set_target_properties(sb_isql/g' "$cmake_file"
            sed -i 's/set_target_properties(gbak/set_target_properties(sb_gbak/g' "$cmake_file"
            sed -i 's/set_target_properties(gfix/set_target_properties(sb_gfix/g' "$cmake_file"
            sed -i 's/set_target_properties(gsec/set_target_properties(sb_gsec/g' "$cmake_file"
            sed -i 's/set_target_properties(gstat/set_target_properties(sb_gstat/g' "$cmake_file"
            sed -i 's/set_target_properties(nbackup/set_target_properties(sb_nbackup/g' "$cmake_file"
            sed -i 's/set_target_properties(fbsvcmgr/set_target_properties(sb_svcmgr/g' "$cmake_file"
            sed -i 's/set_target_properties(fbtracemgr/set_target_properties(sb_tracemgr/g' "$cmake_file"
            
            # Update add_executable calls
            sed -i 's/add_executable(isql/add_executable(sb_isql/g' "$cmake_file"
            sed -i 's/add_executable(gbak/add_executable(sb_gbak/g' "$cmake_file"
            sed -i 's/add_executable(gfix/add_executable(sb_gfix/g' "$cmake_file"
            sed -i 's/add_executable(gsec/add_executable(sb_gsec/g' "$cmake_file"
            sed -i 's/add_executable(gstat/add_executable(sb_gstat/g' "$cmake_file"
            sed -i 's/add_executable(nbackup/add_executable(sb_nbackup/g' "$cmake_file"
            sed -i 's/add_executable(fbsvcmgr/add_executable(sb_svcmgr/g' "$cmake_file"
            sed -i 's/add_executable(fbtracemgr/add_executable(sb_tracemgr/g' "$cmake_file"
            
            # Update target_link_libraries references
            sed -i 's/target_link_libraries(isql/target_link_libraries(sb_isql/g' "$cmake_file"
            sed -i 's/target_link_libraries(gbak/target_link_libraries(sb_gbak/g' "$cmake_file"
            sed -i 's/target_link_libraries(gfix/target_link_libraries(sb_gfix/g' "$cmake_file"
            sed -i 's/target_link_libraries(gsec/target_link_libraries(sb_gsec/g' "$cmake_file"
            sed -i 's/target_link_libraries(gstat/target_link_libraries(sb_gstat/g' "$cmake_file"
            sed -i 's/target_link_libraries(nbackup/target_link_libraries(sb_nbackup/g' "$cmake_file"
            sed -i 's/target_link_libraries(fbsvcmgr/target_link_libraries(sb_svcmgr/g' "$cmake_file"
            sed -i 's/target_link_libraries(fbtracemgr/target_link_libraries(sb_tracemgr/g' "$cmake_file"
            
            # Update library names
            sed -i 's/libfirebird/libscratchbird/g' "$cmake_file"
            sed -i 's/libfb/libsb/g' "$cmake_file"
            
            # Update install targets
            sed -i 's/install(TARGETS isql/install(TARGETS sb_isql/g' "$cmake_file"
            sed -i 's/install(TARGETS gbak/install(TARGETS sb_gbak/g' "$cmake_file"
            sed -i 's/install(TARGETS gfix/install(TARGETS sb_gfix/g' "$cmake_file"
            sed -i 's/install(TARGETS gsec/install(TARGETS sb_gsec/g' "$cmake_file"
            sed -i 's/install(TARGETS gstat/install(TARGETS sb_gstat/g' "$cmake_file"
            
            # Update version information
            sed -i 's/VERSION 6\.0\.0/VERSION 0.6.0/g' "$cmake_file"
            sed -i 's/SOVERSION 6/SOVERSION 0/g' "$cmake_file"
        fi
    done
}

echo "🔧 Updating Makefile configurations..."

# Function to update Makefiles
update_makefiles() {
    find . -name "Makefile*" -type f | while read -r makefile; do
        if [[ -f "$makefile" && ! "$makefile" =~ CMakeFiles ]]; then
            echo "  Updating: $makefile"
            
            # Update project names
            sed -i 's/FIREBIRD/SCRATCHBIRD/g' "$makefile"
            sed -i 's/Firebird/ScratchBird/g' "$makefile"
            sed -i 's/firebird/scratchbird/g' "$makefile"
            
            # Update tool targets
            sed -i 's/isql:/sb_isql:/g' "$makefile"
            sed -i 's/gbak:/sb_gbak:/g' "$makefile"
            sed -i 's/gfix:/sb_gfix:/g' "$makefile"
            sed -i 's/gsec:/sb_gsec:/g' "$makefile"
            sed -i 's/gstat:/sb_gstat:/g' "$makefile"
            
            # Update library names
            sed -i 's/libfirebird/libscratchbird/g' "$makefile"
            sed -i 's/libfb/libsb/g' "$makefile"
            
            # Update version
            sed -i 's/VERSION=6\.0\.0/VERSION=0.6.0/g' "$makefile"
        fi
    done
}

echo "📝 Updating source code references..."

# Function to update source code files
update_source_references() {
    # Update main function names and branding in source files
    find src/ -name "*.cpp" -o -name "*.c" -o -name "*.h" | while read -r source_file; do
        if [[ -f "$source_file" ]]; then
            # Update version strings and branding
            sed -i 's/"Firebird"/"ScratchBird"/g' "$source_file"
            sed -i 's/Firebird/ScratchBird/g' "$source_file" 2>/dev/null || true
            
            # Update executable names in help text
            sed -i 's/"isql"/"sb_isql"/g' "$source_file"
            sed -i 's/"gbak"/"sb_gbak"/g' "$source_file"
            sed -i 's/"gfix"/"sb_gfix"/g' "$source_file"
            sed -i 's/"gsec"/"sb_gsec"/g' "$source_file"
            sed -i 's/"gstat"/"sb_gstat"/g' "$source_file"
            
            # Update library references
            sed -i 's/libfirebird/libscratchbird/g' "$source_file"
        fi
    done
}

echo "🔧 Updating build configuration files..."

# Update configure.ac if it exists
if [[ -f "configure.ac" ]]; then
    echo "  Updating configure.ac"
    sed -i 's/AC_INIT(\[Firebird\]/AC_INIT([ScratchBird]/g' configure.ac
    sed -i 's/firebird/scratchbird/g' configure.ac
    sed -i 's/6\.0\.0/0.6.0/g' configure.ac
fi

# Update version files
if [[ -f "builds/posix/make.defaults" ]]; then
    echo "  Updating make.defaults"
    sed -i 's/FIREBIRD/SCRATCHBIRD/g' builds/posix/make.defaults
    sed -i 's/6\.0\.0/0.6.0/g' builds/posix/make.defaults
fi

# Execute updates
echo "🚀 Executing build system updates..."
update_cmake_files
update_makefiles
update_source_references

echo "📝 Creating ScratchBird version header..."

# Create version header file
cat > src/include/sb_version.h << 'EOF'
/*
 * ScratchBird Database Version Information
 * Based on Firebird 6.0.0.929 with SQL Dialect 4 enhancements
 */

#ifndef SB_VERSION_H
#define SB_VERSION_H

#define SCRATCHBIRD_MAJOR_VER    0
#define SCRATCHBIRD_MINOR_VER    6
#define SCRATCHBIRD_REV_NO       0
#define SCRATCHBIRD_BUILD_NO     1

#define SCRATCHBIRD_VERSION      "0.6.0"
#define SCRATCHBIRD_BUILD        "ScratchBird-0.6.0.1"

// SQL Dialect 4 support
#define SCRATCHBIRD_SQL_DIALECT_MAX  4
#define SCRATCHBIRD_HIERARCHICAL_SCHEMAS  1
#define SCRATCHBIRD_DATABASE_LINKS        1
#define SCRATCHBIRD_SYNONYMS              1

// Tool names
#define SCRATCHBIRD_ISQL         "sb_isql"
#define SCRATCHBIRD_GBAK         "sb_gbak" 
#define SCRATCHBIRD_GFIX         "sb_gfix"
#define SCRATCHBIRD_GSEC         "sb_gsec"
#define SCRATCHBIRD_GSTAT        "sb_gstat"
#define SCRATCHBIRD_NBACKUP      "sb_nbackup"
#define SCRATCHBIRD_SVCMGR       "sb_svcmgr"
#define SCRATCHBIRD_TRACEMGR     "sb_tracemgr"

// Company/Product information
#define SCRATCHBIRD_COMPANY      "ScratchBird Project"
#define SCRATCHBIRD_PRODUCT      "ScratchBird Database"
#define SCRATCHBIRD_COPYRIGHT    "Copyright (C) 2024 ScratchBird Project"

#endif /* SB_VERSION_H */
EOF

echo "📝 Creating build verification script..."

# Create build verification script
cat > verify_build.sh << 'EOF'
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
EOF

chmod +x verify_build.sh

echo "✅ Build system adaptation complete!"
echo
echo "📋 Summary of Changes:"
echo "• Updated CMakeLists.txt files for tool renaming"
echo "• Modified Makefiles for ScratchBird branding"
echo "• Updated source code references and version strings"
echo "• Created ScratchBird version header (src/include/sb_version.h)"
echo "• Created build verification script (verify_build.sh)"
echo
echo "🚀 Next Steps:"
echo "1. Run: ./verify_build.sh"
echo "2. Build ScratchBird: mkdir build && cd build && cmake .. && make"
echo "3. Test tools: ./src/isql/sb_isql"
echo
echo "🔥 ScratchBird build system ready!"