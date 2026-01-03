#!/usr/bin/env bash
# documentation-coverage.sh - Analyze documentation coverage for the project
# Usage: ./scripts/documentation-coverage.sh [--output FILE]

set -euo pipefail

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m'

# Configuration
OUTPUT_FILE="docs/DOCUMENTATION_COVERAGE.md"
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --output)
            OUTPUT_FILE="$2"
            shift 2
            ;;
        --help)
            echo "Usage: $0 [--output FILE]"
            echo "  --output FILE    Output file (default: docs/DOCUMENTATION_COVERAGE.md)"
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            exit 1
            ;;
    esac
done

log_info() { echo -e "${BLUE}ℹ${NC} $*"; }
log_success() { echo -e "${GREEN}✓${NC} $*"; }
log_warn() { echo -e "${YELLOW}⚠${NC} $*"; }

cd "$PROJECT_ROOT"

log_info "Analyzing documentation coverage..."

TIMESTAMP=$(date -u +"%Y-%m-%d %H:%M:%S UTC")

# ============================================================================
# COLLECT STATISTICS
# ============================================================================

log_info "Scanning source files..."

# Count source files
total_cpp=$(find src/ -type f -name "*.cpp" 2>/dev/null | wc -l || echo 0)
total_headers=$(find include/ -type f -name "*.h" 2>/dev/null | wc -l || echo 0)

# Count files with documentation comments (/** or ///)
cpp_with_docs=$(find src/ -type f -name "*.cpp" -exec grep -l "^\s*/\*\*\|^\s*///" {} \; 2>/dev/null | wc -l || echo 0)
headers_with_docs=$(find include/ -type f -name "*.h" -exec grep -l "^\s*/\*\*\|^\s*///" {} \; 2>/dev/null | wc -l || echo 0)

log_info "Analyzing header documentation..."

# Analyze public API headers (classes and functions)
total_classes=$(grep -rh "^class\|^struct" include/ --include="*.h" 2>/dev/null | grep -v "^//" | wc -l || echo 0)
documented_classes=$(grep -B3 "^class\|^struct" include/**/*.h 2>/dev/null | grep -c "/\*\*\|///" || echo 0)

# Count public functions (rough estimate)
total_functions=$(grep -rh "^\s*[a-zA-Z_].*(" include/ --include="*.h" 2>/dev/null | grep -v "^//" | grep -v "^#" | wc -l || echo 0)

log_info "Scanning markdown documentation..."

# Wiki documentation
wiki_total_planned=67
wiki_created=$(find wiki/content/ -type f -name "*.md" 2>/dev/null | wc -l || echo 0)
wiki_installation=$(find wiki/content/installation/ -type f -name "*.md" 2>/dev/null | wc -l || echo 0)
wiki_drivers=$(find wiki/content/drivers/ -type f -name "*.md" 2>/dev/null | wc -l || echo 0)
wiki_migration=$(find wiki/content/migration/ -type f -name "*.md" 2>/dev/null | wc -l || echo 0)
wiki_tutorials=$(find wiki/content/tutorials/ -type f -name "*.md" 2>/dev/null | wc -l || echo 0)
wiki_user_guides=$(find wiki/content/user-guides/ -type f -name "*.md" 2>/dev/null | wc -l || echo 0)
wiki_reference=$(find wiki/content/reference/ -type f -name "*.md" 2>/dev/null | wc -l || echo 0)
wiki_troubleshooting=$(find wiki/content/troubleshooting/ -type f -name "*.md" 2>/dev/null | wc -l || echo 0)

# Specifications
specs_total=$(find docs/specifications/ -type f -name "*.md" 2>/dev/null | wc -l || echo 0)

# Planning documents
planning_total=$(find docs/planning/ -type f -name "*.md" 2>/dev/null | wc -l || echo 0)

log_info "Checking for README files..."

# Count README files in key directories
readme_src=$([ -f "src/README.md" ] && echo 1 || echo 0)
readme_include=$([ -f "include/README.md" ] && echo 1 || echo 0)
readme_tests=$([ -f "tests/README.md" ] && echo 1 || echo 0)
readme_docs=$([ -f "docs/README.md" ] && echo 1 || echo 0)
readme_wiki=$([ -f "wiki/README.md" ] && echo 1 || echo 0)

# ============================================================================
# GENERATE REPORT
# ============================================================================

log_info "Generating documentation coverage report..."

cat > "$OUTPUT_FILE" << EOF
# Documentation Coverage Report

**Generated:** $TIMESTAMP

---

## 📊 Executive Summary

| Category | Coverage | Status |
|----------|----------|--------|
| **Code Documentation** | $(awk "BEGIN {printf \"%.1f\", ($cpp_with_docs + $headers_with_docs) * 100.0 / ($total_cpp + $total_headers)}")% | $([ $(awk "BEGIN {print (($cpp_with_docs + $headers_with_docs) * 100.0 / ($total_cpp + $total_headers) >= 50)}") -eq 1 ] && echo "✅ Good" || echo "⚠️ Needs Improvement") |
| **Wiki Documentation** | $(awk "BEGIN {printf \"%.1f\", $wiki_created * 100.0 / $wiki_total_planned}")% | 🚧 In Progress |
| **Specifications** | $specs_total docs | ✅ Active |
| **Planning Docs** | $planning_total docs | ✅ Active |

### Overall Assessment
$(
if [ $(awk "BEGIN {print ($wiki_created * 100 / $wiki_total_planned >= 50)}") -eq 1 ]; then
    echo "✅ **Good:** Wiki documentation is progressing well"
elif [ $wiki_created -ge 10 ]; then
    echo "🚧 **Fair:** Wiki documentation is started, continue building"
else
    echo "⚠️ **Needs Attention:** Wiki documentation needs development"
fi
)

---

## 💻 Code Documentation

### Source Files

| Metric | Count | With Docs | Coverage |
|--------|-------|-----------|----------|
| **C++ Source (.cpp)** | $total_cpp | $cpp_with_docs | $(awk "BEGIN {printf \"%.1f\", $cpp_with_docs * 100.0 / $total_cpp}")% |
| **Headers (.h)** | $total_headers | $headers_with_docs | $(awk "BEGIN {printf \"%.1f\", $headers_with_docs * 100.0 / $total_headers}")% |
| **Total** | $(($total_cpp + $total_headers)) | $(($cpp_with_docs + $headers_with_docs)) | $(awk "BEGIN {printf \"%.1f\", ($cpp_with_docs + $headers_with_docs) * 100.0 / ($total_cpp + $total_headers)}")% |

### API Documentation

| Component | Total | Documented | Coverage |
|-----------|-------|------------|----------|
| **Classes/Structs** | $total_classes | $documented_classes | $(awk "BEGIN {printf \"%.1f\", $documented_classes * 100.0 / ($total_classes > 0 ? $total_classes : 1)}")% |
| **Public Functions** | $total_functions | - | - |

**Note:** Function documentation requires manual review or Doxygen analysis.

### Documentation Comment Styles

Expected formats:
\`\`\`cpp
/**
 * @brief Brief description
 * @param name Parameter description
 * @return Return value description
 */

/// Single-line documentation comment
\`\`\`

### Top Undocumented Files

EOF

# Find files without documentation comments
log_info "Finding undocumented files..."

undoc_headers=$(find include/ -type f -name "*.h" -exec sh -c 'grep -q "^\s*/\*\*\|^\s*///" "$1" || echo "$1"' _ {} \; 2>/dev/null | head -10)

if [ -n "$undoc_headers" ]; then
    echo "**Headers without documentation:**" >> "$OUTPUT_FILE"
    echo "\`\`\`" >> "$OUTPUT_FILE"
    echo "$undoc_headers" | while read -r file; do
        if [ -n "$file" ]; then
            echo "- $file" >> "$OUTPUT_FILE"
        fi
    done
    echo "\`\`\`" >> "$OUTPUT_FILE"
else
    echo "*All header files have some documentation* ✅" >> "$OUTPUT_FILE"
fi

cat >> "$OUTPUT_FILE" << EOF

---

## 📚 Wiki Documentation

### Overall Progress

**Total Planned:** $wiki_total_planned pages
**Created:** $wiki_created pages
**Completion:** $(awk "BEGIN {printf \"%.1f\", $wiki_created * 100.0 / $wiki_total_planned}")%

### Progress Bar

\`\`\`
EOF

# Progress bar
completed=$(($wiki_created * 50 / $wiki_total_planned))
echo -n "[" >> "$OUTPUT_FILE"
for ((i=0; i<completed; i++)); do echo -n "█" >> "$OUTPUT_FILE"; done
for ((i=completed; i<50; i++)); do echo -n "░" >> "$OUTPUT_FILE"; done
echo "] $(awk "BEGIN {printf \"%.0f\", $wiki_created * 100.0 / $wiki_total_planned}")%" >> "$OUTPUT_FILE"

cat >> "$OUTPUT_FILE" << EOF
\`\`\`

### Category Breakdown

| Category | Created | Planned | Progress | Status |
|----------|---------|---------|----------|--------|
| **Installation** | $wiki_installation | 8 | $(awk "BEGIN {printf \"%.0f\", $wiki_installation * 100 / 8}")% | $([ $wiki_installation -ge 4 ] && echo "🚧" || echo "⏳") |
| **Drivers** | $wiki_drivers | 9 | $(awk "BEGIN {printf \"%.0f\", $wiki_drivers * 100 / 9}")% | $([ $wiki_drivers -ge 5 ] && echo "🚧" || echo "⏳") |
| **Migration** | $wiki_migration | 6 | $(awk "BEGIN {printf \"%.0f\", $wiki_migration * 100 / 6}")% | $([ $wiki_migration -ge 3 ] && echo "🚧" || echo "⏳") |
| **Tutorials** | $wiki_tutorials | 7 | $(awk "BEGIN {printf \"%.0f\", $wiki_tutorials * 100 / 7}")% | $([ $wiki_tutorials -ge 4 ] && echo "🚧" || echo "⏳") |
| **User Guides** | $wiki_user_guides | 8 | $(awk "BEGIN {printf \"%.0f\", $wiki_user_guides * 100 / 8}")% | $([ $wiki_user_guides -ge 4 ] && echo "🚧" || echo "⏳") |
| **Reference** | $wiki_reference | 7 | $(awk "BEGIN {printf \"%.0f\", $wiki_reference * 100 / 7}")% | $([ $wiki_reference -ge 4 ] && echo "🚧" || echo "⏳") |
| **Troubleshooting** | $wiki_troubleshooting | 5 | $(awk "BEGIN {printf \"%.0f\", $wiki_troubleshooting * 100 / 5}")% | $([ $wiki_troubleshooting -ge 3 ] && echo "🚧" || echo "⏳") |

### Priority Pages Status

EOF

# Check for critical pages
critical_pages=("Home.md" "Getting-Started.md" "FAQ.md" "Contributing.md" "_Sidebar.md" "_Footer.md")
echo "| Page | Status |" >> "$OUTPUT_FILE"
echo "|------|--------|" >> "$OUTPUT_FILE"

for page in "${critical_pages[@]}"; do
    if [ -f "wiki/content/$page" ]; then
        echo "| $page | ✅ Created |" >> "$OUTPUT_FILE"
    else
        echo "| $page | ⏳ Pending |" >> "$OUTPUT_FILE"
    fi
done

cat >> "$OUTPUT_FILE" << EOF

### Wiki Infrastructure

| Component | Status |
|-----------|--------|
| Directory Structure | $([ -d "wiki/content" ] && echo "✅ Complete" || echo "⏳ Pending") |
| Templates | $([ -f "wiki/templates/user-guide-template.md" ] && echo "✅ Available (2)" || echo "⏳ Pending") |
| Sync Script | $([ -x "wiki/scripts/sync-to-wiki.sh" ] && echo "✅ Executable" || echo "⏳ Pending") |
| GitHub Actions | $([ -f ".github/workflows/wiki-sync.yml" ] && echo "✅ Configured" || echo "⏳ Pending") |
| Logo Integration | $([ -f "wiki/images/logos/TransparentScratchBirdLogoHeader.png" ] && echo "✅ Complete" || echo "⏳ Pending") |

---

## 📖 Technical Documentation

### Specifications

**Total Specification Files:** $specs_total

EOF

# List major specifications
major_specs=(
    "docs/specifications/POSTGRESQL_PARSER_IMPLEMENTATION.md"
    "docs/specifications/FIREBIRD_TRANSACTION_MODEL_SPEC.md"
    "docs/specifications/INDEX_ARCHITECTURE.md"
    "docs/specifications/HEAP_TOAST_INTEGRATION.md"
    "docs/specifications/SBLR_DOMAIN_PAYLOADS.md"
)

echo "**Key Specifications:**" >> "$OUTPUT_FILE"
echo "" >> "$OUTPUT_FILE"

for spec in "${major_specs[@]}"; do
    basename_spec=$(basename "$spec")
    if [ -f "$spec" ]; then
        size=$(wc -l < "$spec")
        echo "- ✅ **$basename_spec** ($size lines)" >> "$OUTPUT_FILE"
    else
        echo "- ⏳ **$basename_spec** (not found)" >> "$OUTPUT_FILE"
    fi
done

cat >> "$OUTPUT_FILE" << EOF

### Planning Documents

**Total Planning Files:** $planning_total

EOF

# List planning documents
if [ -d "docs/planning" ]; then
    echo "**Recent Planning Documents:**" >> "$OUTPUT_FILE"
    echo "" >> "$OUTPUT_FILE"
    find docs/planning/ -type f -name "*.md" -printf "%T@ %p\n" 2>/dev/null | sort -rn | head -5 | while read -r timestamp file; do
        basename_file=$(basename "$file")
        echo "- $basename_file" >> "$OUTPUT_FILE"
    done
fi

cat >> "$OUTPUT_FILE" << EOF

---

## 📂 README Files

Directory README files provide navigation and context:

| Directory | README | Status |
|-----------|--------|--------|
| **src/** | $([ $readme_src -eq 1 ] && echo "README.md" || echo "-") | $([ $readme_src -eq 1 ] && echo "✅" || echo "⏳ Needed") |
| **include/** | $([ $readme_include -eq 1 ] && echo "README.md" || echo "-") | $([ $readme_include -eq 1 ] && echo "✅" || echo "⏳ Needed") |
| **tests/** | $([ $readme_tests -eq 1 ] && echo "README.md" || echo "-") | $([ $readme_tests -eq 1 ] && echo "✅" || echo "⏳ Needed") |
| **docs/** | $([ $readme_docs -eq 1 ] && echo "README.md" || echo "-") | $([ $readme_docs -eq 1 ] && echo "✅" || echo "⏳ Needed") |
| **wiki/** | $([ $readme_wiki -eq 1 ] && echo "README.md" || echo "-") | $([ $readme_wiki -eq 1 ] && echo "✅" || echo "⏳ Created") |

---

## 🎯 Documentation Goals

### Short Term (This Week)
- [ ] Increase code documentation to 60%+ coverage
- [ ] Create 5 critical wiki pages (Getting Started, Docker install, etc.)
- [ ] Add README files to src/ and include/ directories

### Medium Term (This Month)
- [ ] Complete all P0 driver documentation (7 drivers)
- [ ] Reach 30% wiki completion (20+ pages)
- [ ] Document all public APIs with Doxygen comments

### Long Term (Beta Release)
- [ ] 80%+ code documentation coverage
- [ ] 80%+ wiki completion (54+ pages)
- [ ] Complete API reference (auto-generated)
- [ ] Complete migration guides (top 3 databases)

---

## 📈 Coverage Metrics

### Current State
\`\`\`
Code Documentation:     $(awk "BEGIN {printf \"%.1f\", ($cpp_with_docs + $headers_with_docs) * 100.0 / ($total_cpp + $total_headers)}")%  $([ $(awk "BEGIN {print (($cpp_with_docs + $headers_with_docs) * 100.0 / ($total_cpp + $total_headers) >= 50)}") -eq 1 ] && echo "[███████████░░░░░░░░░]" || echo "[█████░░░░░░░░░░░░░░░]")
Wiki Documentation:     $(awk "BEGIN {printf \"%.1f\", $wiki_created * 100.0 / $wiki_total_planned}")%  $([ $(awk "BEGIN {print ($wiki_created * 100 / $wiki_total_planned >= 20)}") -eq 1 ] && echo "[████░░░░░░░░░░░░░░░░]" || echo "[██░░░░░░░░░░░░░░░░░░]")
Specifications:         Complete ✅  [████████████████████]
\`\`\`

### Target for Beta
\`\`\`
Code Documentation:     80%       [████████████████░░░░]
Wiki Documentation:     80%       [████████████████░░░░]
API Reference:          100%      [████████████████████]
\`\`\`

---

## 🔍 Analysis Tools

### Check Code Documentation
\`\`\`bash
# Find files without doc comments
find include/ -name "*.h" -exec sh -c 'grep -q "/\*\*\|///" "\$1" || echo "\$1"' _ {} \\;

# Count documented vs undocumented
grep -r "/\*\*" include/ | wc -l
\`\`\`

### Generate API Documentation
\`\`\`bash
# Using Doxygen (if configured)
doxygen Doxyfile

# View output
open docs/api/html/index.html
\`\`\`

### Check Wiki Progress
\`\`\`bash
# List created pages
find wiki/content/ -name "*.md" | sort

# List missing pages
diff <(cat wiki/DIRECTORY_STRUCTURE.md | grep "\.md" | cut -d' ' -f1) <(find wiki/content/ -name "*.md")
\`\`\`

---

## 📝 Recommendations

### Priority 1: Critical Gaps
$(
if [ $(awk "BEGIN {print ($wiki_created < 10)}") -eq 1 ]; then
    echo "1. **Create essential wiki pages** (Getting Started, Docker installation)"
fi
if [ $readme_src -eq 0 ]; then
    echo "2. **Add src/README.md** to explain source code organization"
fi
if [ $readme_include -eq 0 ]; then
    echo "3. **Add include/README.md** to document public API structure"
fi
)

### Priority 2: Improve Coverage
1. **Document public APIs** - Focus on include/ headers first
2. **Add function documentation** - Use Doxygen format
3. **Create driver docs** - 7 P0 drivers need wiki pages

### Priority 3: Maintain Quality
1. **Keep docs updated** - Tag docs with version/date
2. **Review quarterly** - Ensure accuracy
3. **Link validation** - Run link checker regularly

---

**Generated by:** \`scripts/documentation-coverage.sh\`
**Last Updated:** $TIMESTAMP

To regenerate: \`./scripts/documentation-coverage.sh\`
EOF

log_success "Documentation coverage report written to $OUTPUT_FILE"

# Console summary
echo ""
log_info "Documentation Coverage Summary:"
echo "  Code:  $(awk "BEGIN {printf \"%.1f\", ($cpp_with_docs + $headers_with_docs) * 100.0 / ($total_cpp + $total_headers)}")% ($(($cpp_with_docs + $headers_with_docs))/$(($total_cpp + $total_headers)) files)"
echo "  Wiki:  $(awk "BEGIN {printf \"%.1f\", $wiki_created * 100.0 / $wiki_total_planned}")% ($wiki_created/$wiki_total_planned pages)"
echo "  Specs: $specs_total files"
echo ""
log_success "Report generated: $OUTPUT_FILE"
