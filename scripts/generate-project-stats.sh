#!/usr/bin/env bash
# generate-project-stats.sh - Generate comprehensive project statistics
# Usage: ./scripts/generate-project-stats.sh [--output FILE] [--format md|json]

set -euo pipefail

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Default configuration
OUTPUT_FILE="PROJECT_STATS.md"
OUTPUT_FORMAT="md"
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --output)
            OUTPUT_FILE="$2"
            shift 2
            ;;
        --format)
            OUTPUT_FORMAT="$2"
            shift 2
            ;;
        --help)
            echo "Usage: $0 [--output FILE] [--format md|json]"
            echo "  --output FILE    Output file (default: PROJECT_STATS.md)"
            echo "  --format FORMAT  Output format: md or json (default: md)"
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            exit 1
            ;;
    esac
done

# Logging functions
log_info() { echo -e "${BLUE}ℹ${NC} $*"; }
log_success() { echo -e "${GREEN}✓${NC} $*"; }
log_warn() { echo -e "${YELLOW}⚠${NC} $*"; }
log_error() { echo -e "${RED}✗${NC} $*"; }

cd "$PROJECT_ROOT"

log_info "Generating project statistics..."
log_info "Project root: $PROJECT_ROOT"
log_info "Output: $OUTPUT_FILE ($OUTPUT_FORMAT)"

# ============================================================================
# DATA COLLECTION
# ============================================================================

log_info "Collecting code statistics..."

# Code statistics
TOTAL_FILES=$(find . -type f \( -name "*.cpp" -o -name "*.h" -o -name "*.py" -o -name "*.js" -o -name "*.md" \) ! -path "*/build/*" ! -path "*/.git/*" ! -path "*/node_modules/*" | wc -l)
CPP_FILES=$(find . -type f -name "*.cpp" ! -path "*/build/*" ! -path "*/.git/*" | wc -l)
HEADER_FILES=$(find . -type f -name "*.h" ! -path "*/build/*" ! -path "*/.git/*" | wc -l)
TEST_FILES=$(find tests/ -type f \( -name "*.cpp" -o -name "*.h" \) 2>/dev/null | wc -l || echo 0)
MD_FILES=$(find . -type f -name "*.md" ! -path "*/.git/*" ! -path "*/node_modules/*" | wc -l)

# Lines of code (if cloc is available)
if command -v cloc &> /dev/null; then
    log_info "Running cloc for detailed statistics..."
    CLOC_OUTPUT=$(cloc . --exclude-dir=build,.git,node_modules --quiet --md 2>/dev/null || echo "")
    HAS_CLOC=true
else
    log_warn "cloc not installed - skipping detailed line counts"
    HAS_CLOC=false
    # Fallback: basic line counts
    CPP_LINES=$(find . -type f -name "*.cpp" ! -path "*/build/*" ! -path "*/.git/*" -exec wc -l {} + 2>/dev/null | tail -1 | awk '{print $1}' || echo 0)
    HEADER_LINES=$(find . -type f -name "*.h" ! -path "*/build/*" ! -path "*/.git/*" -exec wc -l {} + 2>/dev/null | tail -1 | awk '{print $1}' || echo 0)
fi

log_info "Collecting documentation statistics..."

# Documentation statistics
WIKI_PAGES=$(find wiki/content/ -type f -name "*.md" 2>/dev/null | wc -l || echo 0)
SPEC_FILES=$(find docs/specifications/ -type f -name "*.md" 2>/dev/null | wc -l || echo 0)
PLANNING_FILES=$(find docs/planning/ -type f -name "*.md" 2>/dev/null | wc -l || echo 0)

log_info "Collecting Beta requirements statistics..."

# Beta requirements statistics
BETA_ROOT="docs/specifications/beta_requirements"
BETA_DIRS=$(find "$BETA_ROOT" -mindepth 1 -type d ! -path "*/archive/*" 2>/dev/null | wc -l || echo 0)

P0_COUNT=0
P1_COUNT=0
P2_COUNT=0
UNSPEC_COUNT=0
TOTAL_BETA_ITEMS=0
DRIVER_TOTAL=0
P0_DRIVER_COUNT=0

while IFS= read -r -d '' readme; do
    if [ "$readme" = "$BETA_ROOT/README.md" ]; then
        continue
    fi
    ((TOTAL_BETA_ITEMS++)) || true
    if grep -q "Priority.*P0" "$readme"; then
        ((P0_COUNT++)) || true
    elif grep -q "Priority.*P1" "$readme"; then
        ((P1_COUNT++)) || true
    elif grep -q "Priority.*P2" "$readme"; then
        ((P2_COUNT++)) || true
    else
        ((UNSPEC_COUNT++)) || true
    fi
done < <(find "$BETA_ROOT" -name "README.md" ! -path "*/archive/*" -print0 2>/dev/null)

if [ -d "$BETA_ROOT/drivers" ]; then
    DRIVER_TOTAL=$(find "$BETA_ROOT/drivers" -name "README.md" ! -path "*/archive/*" ! -path "$BETA_ROOT/drivers/README.md" 2>/dev/null | wc -l || echo 0)
    P0_DRIVER_COUNT=$(find "$BETA_ROOT/drivers" -name "README.md" ! -path "*/archive/*" ! -path "$BETA_ROOT/drivers/README.md" -exec grep -l "Priority.*P0" {} \; 2>/dev/null | wc -l || echo 0)
fi

log_info "Collecting test statistics..."

# Test statistics
if [ -f "tests/CMakeLists.txt" ]; then
    UNIT_TESTS=$(grep -c "add_executable.*test" tests/CMakeLists.txt 2>/dev/null || echo 0)
else
    UNIT_TESTS=$(find tests/unit/ -type f -name "*.cpp" 2>/dev/null | wc -l || echo 0)
fi

INTEGRATION_TESTS=$(find tests/integration/ -type f -name "*.cpp" 2>/dev/null | wc -l || echo 0)
BENCHMARK_TESTS=$(find tests/benchmark/ -type f -name "*.cpp" 2>/dev/null | wc -l || echo 0)

log_info "Collecting git statistics..."

# Git statistics
if command -v git &> /dev/null && [ -d .git ]; then
    TOTAL_COMMITS=$(git rev-list --all --count 2>/dev/null || echo 0)
    CONTRIBUTORS=$(git log --format='%an' | sort -u | wc -l || echo 0)
    BRANCHES=$(git branch -a | wc -l || echo 0)
    RECENT_COMMIT=$(git log -1 --format="%h - %s (%ar)" 2>/dev/null || echo "N/A")

    # Commit activity (last 30 days)
    COMMITS_30D=$(git log --since="30 days ago" --oneline | wc -l || echo 0)

    # Repository size
    REPO_SIZE=$(du -sh .git 2>/dev/null | cut -f1 || echo "N/A")
else
    TOTAL_COMMITS=0
    CONTRIBUTORS=0
    BRANCHES=0
    RECENT_COMMIT="N/A"
    COMMITS_30D=0
    REPO_SIZE="N/A"
fi

# Timestamp
TIMESTAMP=$(date -u +"%Y-%m-%d %H:%M:%S UTC")

# ============================================================================
# MARKDOWN OUTPUT
# ============================================================================

if [ "$OUTPUT_FORMAT" = "md" ]; then
    log_info "Generating markdown output..."

    cat > "$OUTPUT_FILE" << EOF
# ScratchBird Project Statistics

**Generated:** $TIMESTAMP
**Project Root:** $PROJECT_ROOT

---

## 📊 Overview

| Metric | Count |
|--------|-------|
| **Total Files** | $TOTAL_FILES |
| **C++ Source Files** | $CPP_FILES |
| **Header Files** | $HEADER_FILES |
| **Test Files** | $TEST_FILES |
| **Documentation Files** | $MD_FILES |

---

## 💻 Code Statistics

### Source Code
| Language | Files | Lines |
|----------|-------|-------|
EOF

    if [ "$HAS_CLOC" = true ]; then
        echo "$CLOC_OUTPUT" >> "$OUTPUT_FILE"
    else
        cat >> "$OUTPUT_FILE" << EOF
| C++ Source | $CPP_FILES | $CPP_LINES |
| C++ Headers | $HEADER_FILES | $HEADER_LINES |
| **Total C++** | **$((CPP_FILES + HEADER_FILES))** | **$((CPP_LINES + HEADER_LINES))** |

*Note: Install 'cloc' for detailed statistics*
EOF
    fi

    cat >> "$OUTPUT_FILE" << EOF

### Code Organization
| Category | Count |
|----------|-------|
| Source Files (src/) | $CPP_FILES |
| Header Files (include/) | $HEADER_FILES |
| Test Files (tests/) | $TEST_FILES |

---

## 🧪 Test Suite

| Test Type | Count |
|-----------|-------|
| **Unit Tests** | $UNIT_TESTS |
| **Integration Tests** | $INTEGRATION_TESTS |
| **Benchmark Tests** | $BENCHMARK_TESTS |
| **Total Tests** | $((UNIT_TESTS + INTEGRATION_TESTS + BENCHMARK_TESTS)) |

EOF

    # Add test coverage if available
    if [ -f "build/coverage.txt" ]; then
        COVERAGE=$(grep -oP '\d+\.\d+%' build/coverage.txt | head -1 || echo "N/A")
        cat >> "$OUTPUT_FILE" << EOF
**Test Coverage:** $COVERAGE

EOF
    fi

    cat >> "$OUTPUT_FILE" << EOF
---

## 📚 Documentation

| Category | Count | Status |
|----------|-------|--------|
| **Wiki Pages** | $WIKI_PAGES | 🚧 In Progress |
| **Specifications** | $SPEC_FILES | ✅ Active |
| **Planning Docs** | $PLANNING_FILES | ✅ Active |
| **Total Markdown** | $MD_FILES | - |

### Wiki Infrastructure
- **Content Pages:** $WIKI_PAGES / 67 planned ($(( WIKI_PAGES * 100 / 67 ))%)
- **Templates:** 2 complete, 3 pending
- **Automation:** Sync script + GitHub Actions ✅

### Documentation Coverage
EOF

    # Calculate documentation categories
    WIKI_INSTALLATION=$(find wiki/content/installation/ -type f -name "*.md" 2>/dev/null | wc -l || echo 0)
    WIKI_DRIVERS=$(find wiki/content/drivers/ -type f -name "*.md" 2>/dev/null | wc -l || echo 0)
    WIKI_MIGRATION=$(find wiki/content/migration/ -type f -name "*.md" 2>/dev/null | wc -l || echo 0)
    WIKI_TUTORIALS=$(find wiki/content/tutorials/ -type f -name "*.md" 2>/dev/null | wc -l || echo 0)

    cat >> "$OUTPUT_FILE" << EOF
| Category | Created | Planned | Progress |
|----------|---------|---------|----------|
| Installation Guides | $WIKI_INSTALLATION | 8 | $(( WIKI_INSTALLATION * 100 / 8 ))% |
| Driver Documentation | $WIKI_DRIVERS | 9 | $(( WIKI_DRIVERS * 100 / 9 ))% |
| Migration Guides | $WIKI_MIGRATION | 6 | $(( WIKI_MIGRATION * 100 / 6 ))% |
| Tutorials | $WIKI_TUTORIALS | 7 | $(( WIKI_TUTORIALS * 100 / 7 ))% |

---

## 🎯 Beta Requirements Progress

| Category | Count |
|----------|-------|
| **Total Items** | $TOTAL_BETA_ITEMS items |
| **Categories** | $BETA_DIRS directories |
| **P0 (Critical)** | $P0_COUNT items |
| **P1 (High)** | $P1_COUNT items |
| **P2 (Medium)** | $P2_COUNT items |
| **Unspecified** | $UNSPEC_COUNT items |

### Priority Breakdown
EOF

    TOTAL_REQUIREMENTS=$TOTAL_BETA_ITEMS

    if [ $TOTAL_REQUIREMENTS -gt 0 ]; then
        cat >> "$OUTPUT_FILE" << EOF
| Priority | Items | Percentage |
|----------|-------|------------|
| P0 (Critical - Beta Required) | $P0_COUNT | $(( P0_COUNT * 100 / TOTAL_REQUIREMENTS ))% |
| P1 (High - Post-Beta) | $P1_COUNT | $(( P1_COUNT * 100 / TOTAL_REQUIREMENTS ))% |
| P2 (Medium - Future) | $P2_COUNT | $(( P2_COUNT * 100 / TOTAL_REQUIREMENTS ))% |
| Unspecified | $UNSPEC_COUNT | $(( UNSPEC_COUNT * 100 / TOTAL_REQUIREMENTS ))% |
| **Total** | **$TOTAL_REQUIREMENTS** | **100%** |

### P0 Drivers (Beta Critical)
See \`docs/specifications/beta_requirements/COMPLETION_STATUS.md\` for the authoritative driver list.

EOF
    fi

    cat >> "$OUTPUT_FILE" << EOF
---

## 🔧 Repository

| Metric | Value |
|--------|-------|
| **Total Commits** | $TOTAL_COMMITS |
| **Contributors** | $CONTRIBUTORS |
| **Branches** | $BRANCHES |
| **Commits (30 days)** | $COMMITS_30D |
| **Repository Size** | $REPO_SIZE |

**Most Recent Commit:**
\`\`\`
$RECENT_COMMIT
\`\`\`

---

## 📈 Project Trends

### Activity Metrics
- **Commits per day (30d avg):** $(( COMMITS_30D / 30 )) commits/day
- **Active development:** $([ $COMMITS_30D -gt 50 ] && echo "✅ Very Active" || echo "🔄 Active")

### Documentation Ratio
- **Docs to Code Ratio:** $(awk "BEGIN {printf \"%.2f\", $MD_FILES / ($CPP_FILES + $HEADER_FILES)}")
- **Test Coverage Ratio:** $(awk "BEGIN {printf \"%.2f\", $TEST_FILES / ($CPP_FILES + $HEADER_FILES)}")

---

## 🎨 Project Structure

\`\`\`
ScratchBird/
├── src/                    # Source code ($CPP_FILES files)
├── include/                # Headers ($HEADER_FILES files)
├── tests/                  # Test suite ($TEST_FILES files)
│   ├── unit/              # Unit tests ($UNIT_TESTS files)
│   ├── integration/       # Integration tests ($INTEGRATION_TESTS files)
│   └── benchmark/         # Benchmarks ($BENCHMARK_TESTS files)
├── docs/                  # Documentation ($((SPEC_FILES + PLANNING_FILES)) files)
│   ├── specifications/    # Technical specs ($SPEC_FILES files)
│   └── planning/          # Planning docs ($PLANNING_FILES files)
├── wiki/                  # Wiki content ($WIKI_PAGES files)
└── scripts/               # Automation scripts
\`\`\`

---

## 🚀 Quick Stats

**Code Maturity:**
- Source files: **$CPP_FILES** C++ files
- Test coverage: **$TEST_FILES** test files
- Documentation: **$MD_FILES** markdown files

**Beta Readiness:**
- P0 Requirements: **$P0_COUNT** items identified
- Unspecified Requirements: **$UNSPEC_COUNT** items
- Driver Specs: **$P0_DRIVER_COUNT/$DRIVER_TOTAL** P0 drivers specified ✅
- Wiki Infrastructure: **100%** complete ✅

**Development Velocity:**
- Total commits: **$TOTAL_COMMITS**
- Recent activity: **$COMMITS_30D** commits (30 days)
- Contributors: **$CONTRIBUTORS** developers

---

**Last Updated:** $TIMESTAMP
**Generated by:** \`scripts/generate-project-stats.sh\`

To regenerate: \`./scripts/generate-project-stats.sh\`
EOF

    log_success "Statistics written to $OUTPUT_FILE"

# ============================================================================
# JSON OUTPUT
# ============================================================================

elif [ "$OUTPUT_FORMAT" = "json" ]; then
    log_info "Generating JSON output..."

    cat > "$OUTPUT_FILE" << EOF
{
  "generated_at": "$TIMESTAMP",
  "project_root": "$PROJECT_ROOT",
  "code": {
    "total_files": $TOTAL_FILES,
    "cpp_files": $CPP_FILES,
    "header_files": $HEADER_FILES,
    "test_files": $TEST_FILES,
    "markdown_files": $MD_FILES,
    "cpp_lines": ${CPP_LINES:-0},
    "header_lines": ${HEADER_LINES:-0}
  },
  "tests": {
    "unit_tests": $UNIT_TESTS,
    "integration_tests": $INTEGRATION_TESTS,
    "benchmark_tests": $BENCHMARK_TESTS,
    "total_tests": $((UNIT_TESTS + INTEGRATION_TESTS + BENCHMARK_TESTS))
  },
  "documentation": {
    "wiki_pages": $WIKI_PAGES,
    "wiki_planned": 67,
    "wiki_progress_percent": $(( WIKI_PAGES * 100 / 67 )),
    "specifications": $SPEC_FILES,
    "planning_docs": $PLANNING_FILES,
    "total_markdown": $MD_FILES
  },
  "beta_requirements": {
    "total_directories": $BETA_DIRS,
    "total_items": $TOTAL_BETA_ITEMS,
    "p0_items": $P0_COUNT,
    "p1_items": $P1_COUNT,
    "p2_items": $P2_COUNT,
    "unspecified_items": $UNSPEC_COUNT,
    "total_requirements": $TOTAL_BETA_ITEMS,
    "driver_total": $DRIVER_TOTAL,
    "p0_driver_items": $P0_DRIVER_COUNT
  },
  "repository": {
    "total_commits": $TOTAL_COMMITS,
    "contributors": $CONTRIBUTORS,
    "branches": $BRANCHES,
    "commits_30_days": $COMMITS_30D,
    "repository_size": "$REPO_SIZE",
    "recent_commit": "$RECENT_COMMIT"
  }
}
EOF

    log_success "Statistics written to $OUTPUT_FILE (JSON format)"
fi

log_success "Project statistics generation complete!"
echo ""
echo "Output: $OUTPUT_FILE"
echo ""

# Display quick summary to console
if [ "$OUTPUT_FORMAT" = "md" ]; then
    log_info "Quick Summary:"
    echo "  Code:         $CPP_FILES C++ files, $HEADER_FILES headers"
    echo "  Tests:        $((UNIT_TESTS + INTEGRATION_TESTS + BENCHMARK_TESTS)) total tests"
    echo "  Docs:         $MD_FILES markdown files"
    echo "  Wiki:         $WIKI_PAGES / 67 pages ($(( WIKI_PAGES * 100 / 67 ))%)"
    echo "  Beta Reqs:    $P0_COUNT P0, $P1_COUNT P1, $P2_COUNT P2, $UNSPEC_COUNT unspecified"
    echo "  Commits:      $TOTAL_COMMITS total, $COMMITS_30D (30d)"
fi
