#!/usr/bin/env bash
# generate-all-stats.sh - Run all project statistics scripts
# Usage: ./scripts/generate-all-stats.sh

set -euo pipefail

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m'

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SCRIPT_DIR="$PROJECT_ROOT/scripts"

# Logging functions
log_header() { echo -e "\n${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"; echo -e "${CYAN}  $*${NC}"; echo -e "${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}\n"; }
log_info() { echo -e "${BLUE}ℹ${NC} $*"; }
log_success() { echo -e "${GREEN}✓${NC} $*"; }
log_warn() { echo -e "${YELLOW}⚠${NC} $*"; }
log_error() { echo -e "${RED}✗${NC} $*"; }

cd "$PROJECT_ROOT"

# ============================================================================
# HEADER
# ============================================================================

echo -e "${CYAN}"
cat << "EOF"
╔═══════════════════════════════════════════════════════════════╗
║                                                               ║
║       ScratchBird Project Statistics Generator               ║
║                                                               ║
║       Generating comprehensive project metrics...            ║
║                                                               ║
╚═══════════════════════════════════════════════════════════════╝
EOF
echo -e "${NC}"

TIMESTAMP=$(date -u +"%Y-%m-%d %H:%M:%S UTC")
log_info "Started: $TIMESTAMP"
log_info "Project: $PROJECT_ROOT"
echo ""

# ============================================================================
# RUN STATISTICS SCRIPTS
# ============================================================================

FAILED_SCRIPTS=()
SUCCESS_COUNT=0

# Script 1: Project Statistics
log_header "1/3 - Generating Project Statistics"
if [ -x "$SCRIPT_DIR/generate-project-stats.sh" ]; then
    if "$SCRIPT_DIR/generate-project-stats.sh"; then
        log_success "Project statistics generated"
        ((SUCCESS_COUNT++))
    else
        log_error "Project statistics failed"
        FAILED_SCRIPTS+=("generate-project-stats.sh")
    fi
else
    log_warn "Script not found or not executable: generate-project-stats.sh"
    FAILED_SCRIPTS+=("generate-project-stats.sh (not executable)")
fi

echo ""

# Script 2: Beta Requirements Tracker
log_header "2/3 - Analyzing Beta Requirements"
if [ -x "$SCRIPT_DIR/beta-requirements-tracker.sh" ]; then
    if "$SCRIPT_DIR/beta-requirements-tracker.sh"; then
        log_success "Beta requirements analysis complete"
        ((SUCCESS_COUNT++))
    else
        log_error "Beta requirements tracker failed"
        FAILED_SCRIPTS+=("beta-requirements-tracker.sh")
    fi
else
    log_warn "Script not found or not executable: beta-requirements-tracker.sh"
    FAILED_SCRIPTS+=("beta-requirements-tracker.sh (not executable)")
fi

echo ""

# Script 3: Documentation Coverage
log_header "3/3 - Analyzing Documentation Coverage"
if [ -x "$SCRIPT_DIR/documentation-coverage.sh" ]; then
    if "$SCRIPT_DIR/documentation-coverage.sh"; then
        log_success "Documentation coverage analysis complete"
        ((SUCCESS_COUNT++))
    else
        log_error "Documentation coverage failed"
        FAILED_SCRIPTS+=("documentation-coverage.sh")
    fi
else
    log_warn "Script not found or not executable: documentation-coverage.sh"
    FAILED_SCRIPTS+=("documentation-coverage.sh (not executable)")
fi

# ============================================================================
# SUMMARY
# ============================================================================

echo ""
log_header "Statistics Generation Complete"

echo "Results:"
echo "  ✓ Successful: $SUCCESS_COUNT/3"
if [ ${#FAILED_SCRIPTS[@]} -gt 0 ]; then
    echo "  ✗ Failed: ${#FAILED_SCRIPTS[@]}/3"
    echo ""
    log_warn "Failed scripts:"
    for script in "${FAILED_SCRIPTS[@]}"; do
        echo "    - $script"
    done
fi

echo ""
log_info "Generated Reports:"

# Check which files were created
reports_found=0

if [ -f "PROJECT_STATS.md" ]; then
    size=$(wc -l < "PROJECT_STATS.md")
    echo "  📊 PROJECT_STATS.md ($size lines)"
    ((reports_found++))
fi

if [ -f "docs/specifications/beta_requirements/COMPLETION_STATUS.md" ]; then
    size=$(wc -l < "docs/specifications/beta_requirements/COMPLETION_STATUS.md")
    echo "  🎯 docs/specifications/beta_requirements/COMPLETION_STATUS.md ($size lines)"
    ((reports_found++))
fi

if [ -f "docs/DOCUMENTATION_COVERAGE.md" ]; then
    size=$(wc -l < "docs/DOCUMENTATION_COVERAGE.md")
    echo "  📚 docs/DOCUMENTATION_COVERAGE.md ($size lines)"
    ((reports_found++))
fi

if [ $reports_found -eq 0 ]; then
    log_warn "No report files found"
fi

echo ""

# ============================================================================
# QUICK SUMMARY FROM GENERATED FILES
# ============================================================================

log_header "Quick Project Overview"

# Extract key metrics from PROJECT_STATS.md if it exists
if [ -f "PROJECT_STATS.md" ]; then
    log_info "Code Statistics:"

    # Extract metrics using grep
    total_files=$(grep "Total Files" PROJECT_STATS.md | head -1 | awk -F'|' '{print $3}' | tr -d ' ' || echo "N/A")
    cpp_files=$(grep "C++ Source Files" PROJECT_STATS.md | head -1 | awk -F'|' '{print $3}' | tr -d ' ' || echo "N/A")
    test_files=$(grep "Test Files" PROJECT_STATS.md | head -1 | awk -F'|' '{print $3}' | tr -d ' ' || echo "N/A")
    md_files=$(grep "Documentation Files" PROJECT_STATS.md | head -1 | awk -F'|' '{print $3}' | tr -d ' ' || echo "N/A")

    echo "  Total Files:       $total_files"
    echo "  C++ Source:        $cpp_files"
    echo "  Tests:             $test_files"
    echo "  Documentation:     $md_files"
fi

# Extract wiki progress
if [ -f "docs/DOCUMENTATION_COVERAGE.md" ]; then
    log_info "Documentation:"

    wiki_progress=$(grep "Created:" docs/DOCUMENTATION_COVERAGE.md | head -1 | awk '{print $2}' || echo "N/A")
    echo "  Wiki Pages:        $wiki_progress / 67 planned"
fi

# Extract beta requirements
if [ -f "docs/specifications/beta_requirements/COMPLETION_STATUS.md" ]; then
    log_info "Beta Requirements:"

    p0_count=$(grep "P0.*Critical" docs/specifications/beta_requirements/COMPLETION_STATUS.md | head -1 | awk -F'|' '{print $3}' | tr -d ' ' || echo "N/A")
    echo "  P0 Items:          $p0_count (Critical for Beta)"
fi

echo ""

# ============================================================================
# RECOMMENDATIONS
# ============================================================================

log_header "Recommendations"

# Check for common issues
issues_found=false

# Check if cloc is installed
if ! command -v cloc &> /dev/null; then
    log_warn "Install 'cloc' for detailed code statistics"
    echo "       sudo apt install cloc    # Debian/Ubuntu"
    echo "       brew install cloc        # macOS"
    issues_found=true
fi

# Check if Doxygen is installed
if ! command -v doxygen &> /dev/null; then
    log_warn "Install 'doxygen' for API documentation generation"
    echo "       sudo apt install doxygen  # Debian/Ubuntu"
    echo "       brew install doxygen      # macOS"
    issues_found=true
fi

# Check wiki progress
if [ -f "docs/DOCUMENTATION_COVERAGE.md" ]; then
    wiki_created=$(find wiki/content/ -type f -name "*.md" 2>/dev/null | wc -l || echo 0)
    if [ $wiki_created -lt 10 ]; then
        log_warn "Wiki documentation is under 15% complete"
        echo "       Consider creating critical pages first:"
        echo "       - Getting-Started.md"
        echo "       - installation/Docker.md"
        echo "       - drivers/Python.md"
        issues_found=true
    fi
fi

if [ "$issues_found" = false ]; then
    log_success "No issues detected!"
fi

echo ""

# ============================================================================
# FOOTER
# ============================================================================

log_header "Next Steps"

cat << EOF
View the generated reports:

  📊 Overall Project Statistics:
     cat PROJECT_STATS.md

  🎯 Beta Requirements Progress:
     cat docs/specifications/beta_requirements/COMPLETION_STATUS.md

  📚 Documentation Coverage:
     cat docs/DOCUMENTATION_COVERAGE.md

Run individual scripts:
  ./scripts/generate-project-stats.sh
  ./scripts/beta-requirements-tracker.sh
  ./scripts/documentation-coverage.sh

Schedule automatic generation:
  # Add to cron for weekly stats
  0 9 * * 1 cd /path/to/ScratchBird && ./scripts/generate-all-stats.sh

  # Or add to GitHub Actions workflow
  See: .github/workflows/stats.yml (create if needed)

EOF

END_TIMESTAMP=$(date -u +"%Y-%m-%d %H:%M:%S UTC")
log_success "Finished: $END_TIMESTAMP"

# Exit with error if any scripts failed
if [ ${#FAILED_SCRIPTS[@]} -gt 0 ]; then
    exit 1
fi

exit 0
