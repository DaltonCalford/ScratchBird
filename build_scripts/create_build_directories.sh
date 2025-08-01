#!/bin/bash
# ScratchBird Build Directory Creation Script
# Creates all necessary directories for compilation

set -e

PROJECT_ROOT="/home/dcalford/Documents/claude/GitHubRepo/ScratchBird"
cd "$PROJECT_ROOT"

GREEN='\033[0;32m'
BLUE='\033[0;34m'
NC='\033[0m'

log() {
    echo -e "${BLUE}[$(date '+%Y-%m-%d %H:%M:%S')]${NC} $1"
}

success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

create_build_directories() {
    local target=${1:-"Release"}
    
    log "Creating comprehensive build directory structure for target: $target"
    
    # Create main build directories
    mkdir -p "temp/$target"
    mkdir -p "gen/$target"
    
    # Create destination directories
    mkdir -p "gen/$target/scratchbird/bin"
    mkdir -p "gen/$target/scratchbird/lib"
    mkdir -p "gen/$target/scratchbird/etc"
    mkdir -p "gen/$target/scratchbird/include"
    mkdir -p "gen/$target/scratchbird/include/firebird"
    mkdir -p "gen/$target/scratchbird/include/firebird/impl"
    mkdir -p "gen/$target/scratchbird/include/scratchbird"
    mkdir -p "gen/$target/scratchbird/include/scratchbird/impl"
    mkdir -p "gen/$target/scratchbird/plugins"
    mkdir -p "gen/$target/scratchbird/intl"
    mkdir -p "gen/$target/scratchbird/tzdata"
    
    # Create all source-based directories in temp build area
    log "Creating source-based directory structure..."
    while IFS= read -r -d '' dir; do
        # Convert src/ path to temp/TARGET/ path
        target_dir="${dir/src\//temp/$target/}"
        mkdir -p "$target_dir"
    done < <(find src -type d -print0)
    
    # Create additional build-specific directories that are often missing
    log "Creating additional build-specific directories..."
    
    # Plugin directories
    mkdir -p "temp/$target/plugins/crypt/arc4"
    mkdir -p "temp/$target/plugins/crypt/chacha" 
    mkdir -p "temp/$target/plugins/profiler"
    mkdir -p "temp/$target/plugins/udr_engine"
    
    # Common directories that cause build failures
    mkdir -p "temp/$target/common/config"
    mkdir -p "temp/$target/common/sha2"
    mkdir -p "temp/$target/common/classes"
    mkdir -p "temp/$target/common/os/posix"
    mkdir -p "temp/$target/common/os/win32"
    
    # YValve directories
    mkdir -p "temp/$target/yvalve/config/os/posix"
    mkdir -p "temp/$target/yvalve/config/os/win32"
    
    # Remote directories
    mkdir -p "temp/$target/remote/client"
    mkdir -p "temp/$target/remote/server"
    mkdir -p "temp/$target/remote/os/posix"
    mkdir -p "temp/$target/remote/os/win32"
    
    # Authentication directories
    mkdir -p "temp/$target/auth/SecureRemotePassword/client"
    mkdir -p "temp/$target/auth/SecureRemotePassword/server"
    mkdir -p "temp/$target/auth/SecurityDatabase"
    mkdir -p "temp/$target/auth/trusted"
    
    # JRD directories
    mkdir -p "temp/$target/jrd/extds"
    mkdir -p "temp/$target/jrd/optimizer"
    mkdir -p "temp/$target/jrd/os/posix"
    mkdir -p "temp/$target/jrd/os/win32"
    mkdir -p "temp/$target/jrd/recsrc"
    mkdir -p "temp/$target/jrd/replication"
    mkdir -p "temp/$target/jrd/trace"
    
    # DSQL directories
    mkdir -p "temp/$target/dsql"
    
    # Utilities directories
    mkdir -p "temp/$target/utilities/archive"
    
    # Internationalization directories
    mkdir -p "temp/$target/intl/charsets"
    mkdir -p "temp/$target/intl/collations"
    
    # Generator directories that are needed during build
    mkdir -p "gen/$target/cloop"
    mkdir -p "gen/$target/include"
    
    # External library directories
    mkdir -p "temp/$target/extern"
    
    success "Created comprehensive build directory structure for $target"
    
    # Display summary
    local dir_count=$(find "temp/$target" -type d | wc -l)
    log "Total directories created in temp/$target: $dir_count"
    
    local gen_count=$(find "gen/$target" -type d | wc -l)
    log "Total directories created in gen/$target: $gen_count"
}

# Function to create directories for all common targets
create_all_targets() {
    local targets=("Release" "Debug" "Release-linux-x64" "Release-linux-x86" "Release-windows-x64" "Release-windows-x86")
    
    for target in "${targets[@]}"; do
        log "Creating directories for target: $target"
        create_build_directories "$target"
    done
    
    success "Created directories for all common build targets"
}

# Function to clean up and recreate directories
clean_and_create() {
    local target=${1:-"Release"}
    
    log "Cleaning and recreating directories for target: $target"
    
    # Remove existing directories
    rm -rf "temp/$target" "gen/$target" 2>/dev/null || true
    
    # Recreate them
    create_build_directories "$target"
    
    success "Cleaned and recreated directories for $target"
}

# Main function
main() {
    local action=${1:-"create"}
    local target=${2:-"Release"}
    
    case $action in
        "create")
            create_build_directories "$target"
            ;;
        "all")
            create_all_targets
            ;;
        "clean")
            clean_and_create "$target"
            ;;
        *)
            echo "Usage: $0 [create|all|clean] [target]"
            echo ""
            echo "Actions:"
            echo "  create  - Create directories for specific target (default: Release)"
            echo "  all     - Create directories for all common targets"
            echo "  clean   - Clean and recreate directories for specific target"
            echo ""
            echo "Examples:"
            echo "  $0 create Release"
            echo "  $0 all"
            echo "  $0 clean Release-linux-x64"
            exit 1
            ;;
    esac
}

# Execute main function
main "$@"