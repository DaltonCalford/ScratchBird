#!/usr/bin/env bash
# sync-to-wiki.sh - Sync wiki content to GitHub wiki repository
#
# Usage:
#   ./sync-to-wiki.sh [options]
#
# Options:
#   --dry-run    Show what would be synced without actually syncing
#   --force      Force overwrite even if wiki has newer content
#   --help       Show this help message

set -euo pipefail

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WIKI_DIR="$(dirname "$SCRIPT_DIR")"
REPO_ROOT="$(dirname "$WIKI_DIR")"
CONTENT_DIR="$WIKI_DIR/content"
IMAGES_DIR="$WIKI_DIR/images"

# Wiki repository (will be cloned)
WIKI_REPO_URL="${GITHUB_REPO_URL:-https://github.com/scratchbird/scratchbird.wiki.git}"
WIKI_CLONE_DIR="/tmp/scratchbird-wiki-$$"

# Options
DRY_RUN=false
FORCE=false

# Logging functions
log_info() {
    echo -e "${BLUE}ℹ${NC} $*"
}

log_success() {
    echo -e "${GREEN}✓${NC} $*"
}

log_warning() {
    echo -e "${YELLOW}⚠${NC} $*"
}

log_error() {
    echo -e "${RED}✗${NC} $*" >&2
}

# Parse command line arguments
parse_args() {
    while [[ $# -gt 0 ]]; do
        case $1 in
            --dry-run)
                DRY_RUN=true
                log_info "Dry run mode enabled - no changes will be made"
                shift
                ;;
            --force)
                FORCE=true
                log_warning "Force mode enabled - will overwrite wiki content"
                shift
                ;;
            --help)
                cat << EOF
Usage: $0 [options]

Sync wiki content from repository to GitHub wiki.

Options:
  --dry-run    Show what would be synced without actually syncing
  --force      Force overwrite even if wiki has newer content
  --help       Show this help message

Environment Variables:
  GITHUB_REPO_URL    GitHub wiki repository URL (default: auto-detected)

Examples:
  $0                    # Normal sync
  $0 --dry-run          # Preview changes
  $0 --force            # Force overwrite

EOF
                exit 0
                ;;
            *)
                log_error "Unknown option: $1"
                echo "Use --help for usage information"
                exit 1
                ;;
        esac
    done
}

# Check prerequisites
check_prerequisites() {
    log_info "Checking prerequisites..."

    # Check if git is installed
    if ! command -v git &> /dev/null; then
        log_error "git is not installed"
        exit 1
    fi

    # Check if content directory exists
    if [ ! -d "$CONTENT_DIR" ]; then
        log_error "Content directory not found: $CONTENT_DIR"
        exit 1
    fi

    # Check if we're in a git repository
    if ! git -C "$REPO_ROOT" rev-parse --git-dir > /dev/null 2>&1; then
        log_error "Not in a git repository"
        exit 1
    fi

    log_success "Prerequisites check passed"
}

# Clone wiki repository
clone_wiki() {
    log_info "Cloning wiki repository..."

    if [ -d "$WIKI_CLONE_DIR" ]; then
        log_warning "Removing existing clone directory"
        rm -rf "$WIKI_CLONE_DIR"
    fi

    if ! git clone "$WIKI_REPO_URL" "$WIKI_CLONE_DIR" 2>&1; then
        log_error "Failed to clone wiki repository"
        log_error "Make sure the wiki exists and you have access"
        log_error "URL: $WIKI_REPO_URL"
        exit 1
    fi

    log_success "Wiki repository cloned to $WIKI_CLONE_DIR"
}

# Sync content files
sync_content() {
    log_info "Syncing content files..."

    local files_synced=0
    local files_skipped=0
    local files_created=0
    local files_updated=0

    # Find all markdown files in content directory
    while IFS= read -r -d '' file; do
        # Get relative path from content dir
        local rel_path="${file#$CONTENT_DIR/}"

        # Convert directory structure to wiki format
        # content/drivers/Python.md -> drivers-Python.md (or keep structure)
        local wiki_path="$rel_path"

        # For GitHub wiki, we might want to flatten structure or keep it
        # Adjust based on preference:
        # Option 1: Keep directory structure (content/foo/bar.md -> foo/bar.md)
        # Option 2: Flatten (content/foo/bar.md -> foo-bar.md)

        local source_file="$file"
        local target_file="$WIKI_CLONE_DIR/$wiki_path"
        local target_dir="$(dirname "$target_file")"

        # Create target directory if needed
        if [ ! -d "$target_dir" ]; then
            if [ "$DRY_RUN" = false ]; then
                mkdir -p "$target_dir"
            fi
            log_info "Created directory: $target_dir"
        fi

        # Check if file needs update
        local needs_update=false

        if [ ! -f "$target_file" ]; then
            needs_update=true
            ((files_created++))
            log_info "New file: $wiki_path"
        elif [ "$FORCE" = true ]; then
            needs_update=true
            ((files_updated++))
            log_info "Force update: $wiki_path"
        elif [ "$source_file" -nt "$target_file" ]; then
            needs_update=true
            ((files_updated++))
            log_info "Updated file: $wiki_path"
        else
            ((files_skipped++))
        fi

        if [ "$needs_update" = true ]; then
            if [ "$DRY_RUN" = false ]; then
                cp "$source_file" "$target_file"
                log_success "Synced: $wiki_path"
            else
                log_info "[DRY RUN] Would sync: $wiki_path"
            fi
            ((files_synced++))
        fi

    done < <(find "$CONTENT_DIR" -type f -name "*.md" -print0)

    log_success "Content sync complete:"
    echo "  - Files synced: $files_synced"
    echo "  - Files created: $files_created"
    echo "  - Files updated: $files_updated"
    echo "  - Files skipped: $files_skipped"
}

# Sync images
sync_images() {
    log_info "Syncing images..."

    if [ ! -d "$IMAGES_DIR" ]; then
        log_warning "Images directory not found, skipping"
        return
    fi

    local target_images_dir="$WIKI_CLONE_DIR/images"

    if [ ! -d "$target_images_dir" ] && [ "$DRY_RUN" = false ]; then
        mkdir -p "$target_images_dir"
    fi

    local images_synced=0

    while IFS= read -r -d '' image; do
        local rel_path="${image#$IMAGES_DIR/}"
        local target_file="$target_images_dir/$rel_path"
        local target_dir="$(dirname "$target_file")"

        if [ ! -d "$target_dir" ] && [ "$DRY_RUN" = false ]; then
            mkdir -p "$target_dir"
        fi

        if [ ! -f "$target_file" ] || [ "$image" -nt "$target_file" ] || [ "$FORCE" = true ]; then
            if [ "$DRY_RUN" = false ]; then
                cp "$image" "$target_file"
                log_success "Synced image: $rel_path"
            else
                log_info "[DRY RUN] Would sync image: $rel_path"
            fi
            ((images_synced++))
        fi

    done < <(find "$IMAGES_DIR" -type f \( -name "*.png" -o -name "*.jpg" -o -name "*.jpeg" -o -name "*.gif" -o -name "*.svg" \) -print0)

    log_success "Synced $images_synced images"
}

# Update timestamps in synced files
update_timestamps() {
    log_info "Updating sync timestamps..."

    local timestamp=$(date -u +"%Y-%m-%d %H:%M:%S UTC")

    # Update SYNC_STATUS.md if it exists
    if [ -f "$WIKI_DIR/SYNC_STATUS.md" ]; then
        if [ "$DRY_RUN" = false ]; then
            echo "Last Sync: $timestamp" > "$WIKI_DIR/SYNC_STATUS.md.tmp"
            echo "Commit: $(git -C "$REPO_ROOT" rev-parse --short HEAD)" >> "$WIKI_DIR/SYNC_STATUS.md.tmp"
            echo "By: $(git config user.name)" >> "$WIKI_DIR/SYNC_STATUS.md.tmp"
            mv "$WIKI_DIR/SYNC_STATUS.md.tmp" "$WIKI_DIR/SYNC_STATUS.md"
        fi
    fi

    log_success "Timestamps updated"
}

# Commit and push to wiki
commit_and_push() {
    if [ "$DRY_RUN" = true ]; then
        log_info "[DRY RUN] Would commit and push changes"
        return
    fi

    log_info "Committing changes to wiki..."

    cd "$WIKI_CLONE_DIR"

    # Check if there are changes
    if ! git diff --quiet || ! git diff --cached --quiet; then
        git add -A

        local commit_msg="Sync from main repository

Synced at: $(date -u +"%Y-%m-%d %H:%M:%S UTC")
Source commit: $(git -C "$REPO_ROOT" rev-parse --short HEAD)
Synced by: $(git config user.name)"

        git commit -m "$commit_msg"

        log_info "Pushing to wiki repository..."
        if git push origin master; then
            log_success "Changes pushed successfully"
        else
            log_error "Failed to push changes"
            exit 1
        fi
    else
        log_info "No changes to commit"
    fi
}

# Cleanup
cleanup() {
    if [ -d "$WIKI_CLONE_DIR" ]; then
        log_info "Cleaning up temporary directory..."
        rm -rf "$WIKI_CLONE_DIR"
        log_success "Cleanup complete"
    fi
}

# Main execution
main() {
    log_info "Starting wiki sync..."
    log_info "Content source: $CONTENT_DIR"
    log_info "Wiki repository: $WIKI_REPO_URL"

    parse_args "$@"
    check_prerequisites
    clone_wiki
    sync_content
    sync_images
    update_timestamps
    commit_and_push

    log_success "Wiki sync completed successfully!"

    if [ "$DRY_RUN" = true ]; then
        log_info "This was a dry run - no changes were made"
        log_info "Run without --dry-run to actually sync"
    fi
}

# Trap cleanup on exit
trap cleanup EXIT

# Run main
main "$@"
