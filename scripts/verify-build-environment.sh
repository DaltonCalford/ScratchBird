#!/bin/bash
# ScratchBird Build Environment Verification
# Version: 1.0

echo "========================================================================"
echo "ScratchBird Build Environment Verification"
echo "========================================================================"
echo ""

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

PASS_COUNT=0
FAIL_COUNT=0
INFO_COUNT=0

# Function to check command
check_cmd() {
    local cmd=$1
    local description=$2
    if command -v $cmd &> /dev/null; then
        version=$($cmd --version 2>&1 | head -n1)
        echo -e "${GREEN}✓${NC} $description: $version"
        ((PASS_COUNT++))
    else
        echo -e "${RED}✗${NC} $description: NOT FOUND"
        ((FAIL_COUNT++))
    fi
}

# Function to check optional command
check_cmd_optional() {
    local cmd=$1
    local description=$2
    if command -v $cmd &> /dev/null; then
        version=$($cmd --version 2>&1 | head -n1)
        echo -e "${GREEN}✓${NC} $description: $version"
        ((PASS_COUNT++))
    else
        echo -e "${YELLOW}ℹ${NC} $description: Not installed (optional)"
        ((INFO_COUNT++))
    fi
}

# Function to check file
check_file() {
    local file=$1
    local description=$2
    if [ -f "$file" ]; then
        echo -e "${GREEN}✓${NC} $description: EXISTS"
        ((PASS_COUNT++))
    else
        echo -e "${RED}✗${NC} $description: NOT FOUND"
        ((FAIL_COUNT++))
    fi
}

# Function to check package
check_package() {
    local package=$1
    local description=$2
    if dpkg -l | grep -q "^ii  $package"; then
        version=$(dpkg -l | grep "^ii  $package" | awk '{print $3}')
        echo -e "${GREEN}✓${NC} $description: $version"
        ((PASS_COUNT++))
    else
        echo -e "${RED}✗${NC} $description: NOT INSTALLED"
        ((FAIL_COUNT++))
    fi
}

echo "=== Compilers ==="
check_cmd gcc "GCC"
check_cmd g++ "G++"
check_cmd gcc-11 "GCC 11"
check_cmd gcc-12 "GCC 12"
check_cmd clang "Clang"
check_cmd clang++ "Clang++"
check_cmd clang-14 "Clang 14"
check_cmd clang-15 "Clang 15"

echo ""
echo "=== Build Systems ==="
check_cmd cmake "CMake"
check_cmd ninja "Ninja"
check_cmd make "Make"
check_cmd ccache "ccache"

echo ""
echo "=== Cross-Compilation - Windows ==="
check_cmd x86_64-w64-mingw32-gcc "MinGW-w64 GCC"
check_cmd x86_64-w64-mingw32-g++ "MinGW-w64 G++"
check_cmd x86_64-w64-mingw32-ar "MinGW-w64 AR"
check_cmd wine64 "Wine64"

echo ""
echo "=== Cross-Compilation - macOS ==="
check_cmd_optional x86_64-apple-darwin23-clang "OSXCross x86_64 Clang"
check_cmd_optional arm64-apple-darwin23-clang "OSXCross ARM64 Clang"

echo ""
echo "=== Core Dependencies ==="
check_package libspdlog-dev "spdlog (development)"
check_package libgtest-dev "GoogleTest (development)"
check_package libssl-dev "OpenSSL (development)"
check_package liblz4-dev "LZ4 (development)"
check_package zlib1g-dev "zlib (development)"

echo ""
echo "=== Optional Dependencies ==="
check_package libgeos-dev "GEOS (development)"
check_package libproj-dev "PROJ (development)"
check_package libxml2-dev "libxml2 (development)"

echo ""
echo "=== Container Tools ==="
check_cmd docker "Docker"
check_cmd podman "Podman"
check_cmd buildah "Buildah"

if command -v docker &> /dev/null; then
    if docker ps &> /dev/null; then
        echo -e "${GREEN}✓${NC} Docker daemon: Running"
        ((PASS_COUNT++))
    else
        echo -e "${YELLOW}ℹ${NC} Docker daemon: Not running or permission denied"
        ((INFO_COUNT++))
    fi
fi

echo ""
echo "=== Packaging Tools ==="
check_cmd debuild "Debuild (DEB packaging)"
check_cmd dpkg-buildpackage "dpkg-buildpackage"
check_cmd rpmbuild "rpmbuild (RPM packaging)"
check_cmd linuxdeploy "linuxdeploy (AppImage)"
check_cmd appimagetool "appimagetool (AppImage)"
check_cmd fuse "FUSE"

echo ""
echo "=== Development Tools ==="
check_cmd clang-format "clang-format"
check_cmd clang-tidy "clang-tidy"
check_cmd cppcheck "cppcheck"
check_cmd valgrind "Valgrind"
check_cmd gdb "GDB"
check_cmd strace "strace"

echo ""
echo "=== Version Control ==="
check_cmd git "Git"
check_cmd gh "GitHub CLI"

echo ""
echo "=== Python Tools ==="
check_cmd python3 "Python 3"
check_cmd pip3 "pip3"

if command -v pip3 &> /dev/null; then
    if pip3 list --user | grep -q conan; then
        echo -e "${GREEN}✓${NC} Conan: Installed"
        ((PASS_COUNT++))
    else
        echo -e "${YELLOW}ℹ${NC} Conan: Not installed (optional)"
        ((INFO_COUNT++))
    fi
fi

echo ""
echo "=== Other Tools ==="
check_cmd jq "jq"
check_cmd doxygen "Doxygen"
check_cmd pandoc "Pandoc"

echo ""
echo "=== User Groups ==="
if groups | grep -q docker; then
    echo -e "${GREEN}✓${NC} docker group: Member"
    ((PASS_COUNT++))
else
    echo -e "${YELLOW}⚠${NC} docker group: NOT A MEMBER (logout/login required)"
    ((INFO_COUNT++))
fi

if groups | grep -q sudo; then
    echo -e "${GREEN}✓${NC} sudo group: Member"
    ((PASS_COUNT++))
fi

echo ""
echo "=== System Resources ==="
echo -e "${GREEN}ℹ${NC} CPU cores: $(nproc)"
echo -e "${GREEN}ℹ${NC} Total RAM: $(free -h | awk '/^Mem:/ {print $2}')"
echo -e "${GREEN}ℹ${NC} Available RAM: $(free -h | awk '/^Mem:/ {print $7}')"

echo ""
echo "=== Disk Space ==="
df -h / | awk 'NR==1{print $0} NR==2{
    used_pct=$5;
    if (used_pct+0 > 90)
        printf "\033[0;31m%s\033[0m\n", $0;
    else if (used_pct+0 > 70)
        printf "\033[1;33m%s\033[0m\n", $0;
    else
        printf "\033[0;32m%s\033[0m\n", $0
}'

echo ""
echo "========================================================================"
echo "Summary"
echo "========================================================================"
echo -e "${GREEN}✓ Passed:${NC} $PASS_COUNT"
echo -e "${RED}✗ Failed:${NC} $FAIL_COUNT"
echo -e "${YELLOW}ℹ Info:${NC} $INFO_COUNT"
echo ""

if [ $FAIL_COUNT -eq 0 ]; then
    echo -e "${GREEN}All required components are installed!${NC}"
    echo ""
    echo "You can now build ScratchBird for multiple platforms:"
    echo "  - Native Linux (x86_64, ARM64)"
    echo "  - Windows via MinGW-w64"
    if command -v x86_64-apple-darwin23-clang &> /dev/null; then
        echo "  - macOS via OSXCross"
    fi
    echo "  - Docker containers (multi-arch)"
    echo "  - AppImage packages"
    echo "  - DEB packages"
    if command -v rpmbuild &> /dev/null; then
        echo "  - RPM packages"
    fi
    echo ""
    echo "Next: Try building ScratchBird:"
    echo "  cd build"
    echo "  cmake -G Ninja -DCMAKE_BUILD_TYPE=Release .."
    echo "  ninja"
    exit 0
else
    echo -e "${RED}Some required components are missing.${NC}"
    echo ""
    echo "To fix:"
    echo "  1. Review the failed items above"
    echo "  2. Re-run: ./install-build-environment.sh"
    echo "  3. Check documentation in:"
    echo "     docs/specifications/beta_requirements/builds/"
    exit 1
fi
