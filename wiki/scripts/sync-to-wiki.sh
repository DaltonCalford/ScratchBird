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
WIKI_REPO_URL="${GITHUB_REPO_URL:-https://github.com/DaltonCalford/ScratchBird.wiki.git}"
WIKI_CLONE_DIR="/tmp/scratchbird-wiki-$$"

# Options
DRY_RUN=false
FORCE=false

# Flattening settings for GitHub wiki (no nested directories)
FLATTEN_SEPARATOR="-"
LINK_MAP_FILE=""

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

# Build a map of content paths to flattened wiki page names
build_link_map() {
    LINK_MAP_FILE="$(mktemp -t scratchbird-wiki-map-XXXX.json)"

    python - "$CONTENT_DIR" "$LINK_MAP_FILE" "$FLATTEN_SEPARATOR" << 'PY'
import json
import os
import sys

content_dir = sys.argv[1]
map_file = sys.argv[2]
sep = sys.argv[3]

mapping = {}
reverse = {}

for dirpath, _, filenames in os.walk(content_dir):
    for name in filenames:
        if not name.endswith(".md"):
            continue
        rel = os.path.relpath(os.path.join(dirpath, name), content_dir).replace(os.sep, "/")
        rel_no_ext = rel[:-3]
        page_path = rel_no_ext[:-7] if rel_no_ext.endswith("/README") else rel_no_ext
        slug = page_path.replace("/", sep)
        if slug in reverse and reverse[slug] != page_path:
            raise SystemExit(f"Slug collision: {slug} from {page_path} and {reverse[slug]}")
        reverse[slug] = page_path
        mapping[page_path] = slug
        if rel_no_ext.endswith("/README"):
            mapping[rel_no_ext] = slug

with open(map_file, "w", encoding="utf-8") as fh:
    json.dump(mapping, fh, indent=2, sort_keys=True)
PY
}

# Rewrite internal links to flattened wiki page names
rewrite_markdown() {
    local source_file="$1"
    local target_file="$2"
    local rel_path="$3"

    python - "$source_file" "$target_file" "$rel_path" "$LINK_MAP_FILE" << 'PY'
import json
import os
import posixpath
import re
import sys

source_file = sys.argv[1]
target_file = sys.argv[2]
rel_path = sys.argv[3].replace(os.sep, "/")
map_file = sys.argv[4]

with open(map_file, "r", encoding="utf-8") as fh:
    mapping = json.load(fh)

rel_dir = posixpath.dirname(rel_path) or "."

def is_external(target: str) -> bool:
    return bool(re.match(r"^[a-zA-Z][a-zA-Z0-9+.-]*:", target))

def is_image(target: str) -> bool:
    lower = target.lower()
    return lower.endswith((".png", ".jpg", ".jpeg", ".gif", ".svg", ".webp"))

def map_target(target: str) -> str:
    if not target or target.startswith("#"):
        return target
    if is_external(target):
        return target

    if "#" in target:
        path_part, anchor = target.split("#", 1)
    else:
        path_part, anchor = target, ""

    if not path_part:
        return target

    lower_path = path_part.lower()
    if is_image(path_part) or lower_path.startswith(("images/", "./images/", "../images/")):
        normalized = re.sub(r"^(?:\.\./|\./)+", "", path_part)
        if normalized.startswith("images/"):
            return f"{normalized}#{anchor}" if anchor else normalized

        if path_part.startswith("/"):
            resolved = path_part.lstrip("/")
        else:
            resolved = posixpath.normpath(posixpath.join(rel_dir, path_part))

        if resolved.startswith("./"):
            resolved = resolved[2:]

        if resolved.startswith("images/"):
            return f"{resolved}#{anchor}" if anchor else resolved

        return target

    if path_part.endswith(".md"):
        path_part = path_part[:-3]

    if path_part.startswith("/"):
        resolved = path_part.lstrip("/")
    else:
        resolved = posixpath.normpath(posixpath.join(rel_dir, path_part))

    if resolved.startswith("./"):
        resolved = resolved[2:]

    slug = mapping.get(resolved)
    if not slug:
        return target

    return f"{slug}#{anchor}" if anchor else slug

link_re = re.compile(r"(!?\[[^\]]*\]\()([^)]+)(\))")

with open(source_file, "r", encoding="utf-8") as fh:
    content = fh.read()

def repl(match: re.Match) -> str:
    prefix, target, suffix = match.groups()
    new_target = map_target(target.strip())
    return f"{prefix}{new_target}{suffix}"

content = link_re.sub(repl, content)

with open(target_file, "w", encoding="utf-8") as fh:
    fh.write(content)
PY
}

# Sync content files
sync_content() {
    log_info "Syncing content files..."

    build_link_map

    local files_synced=0
    local files_skipped=0
    local files_created=0
    local files_updated=0

    # Find all markdown files in content directory
    while IFS= read -r -d '' file; do
        # Get relative path from content dir
        local rel_path="${file#$CONTENT_DIR/}"
        local rel_no_ext="${rel_path%.md}"
        local page_path="$rel_no_ext"
        if [[ "$page_path" == */README ]]; then
            page_path="${page_path%/README}"
        fi
        local wiki_slug="${page_path//\//${FLATTEN_SEPARATOR}}"
        local wiki_path="${wiki_slug}.md"

        local source_file="$file"
        local target_file="$WIKI_CLONE_DIR/$wiki_path"

        # Check if file needs update
        local needs_update=false

        if [ ! -f "$target_file" ]; then
            needs_update=true
            ((files_created+=1))
            log_info "New file: $wiki_path"
        elif [ "$FORCE" = true ]; then
            needs_update=true
            ((files_updated+=1))
            log_info "Force update: $wiki_path"
        elif [ "$source_file" -nt "$target_file" ]; then
            needs_update=true
            ((files_updated+=1))
            log_info "Updated file: $wiki_path"
        else
            ((files_skipped+=1))
        fi

        if [ "$needs_update" = true ]; then
            if [ "$DRY_RUN" = false ]; then
                rewrite_markdown "$source_file" "$target_file" "$rel_path"
                log_success "Synced: $wiki_path"
            else
                log_info "[DRY RUN] Would sync: $wiki_path"
            fi
            ((files_synced+=1))
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
            ((images_synced+=1))
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

    # Check if there are changes, including untracked files
    if [ -n "$(git status --porcelain)" ]; then
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
    if [ -n "${LINK_MAP_FILE:-}" ] && [ -f "$LINK_MAP_FILE" ]; then
        rm -f "$LINK_MAP_FILE"
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
