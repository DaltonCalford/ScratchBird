#!/usr/bin/env bash
# beta-requirements-tracker.sh - Track Beta requirements completion status
# Usage: ./scripts/beta-requirements-tracker.sh [--output FILE]

set -euo pipefail

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m'

# Configuration
OUTPUT_FILE="docs/specifications/beta_requirements/COMPLETION_STATUS.md"
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
            echo "  --output FILE    Output file (default: docs/specifications/beta_requirements/COMPLETION_STATUS.md)"
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

cd "$PROJECT_ROOT"

log_info "Tracking Beta requirements completion..."

BETA_ROOT="docs/specifications/beta_requirements"
TIMESTAMP=$(date -u +"%Y-%m-%d %H:%M:%S UTC")

# ============================================================================
# ANALYZE BETA REQUIREMENTS STRUCTURE
# ============================================================================

log_info "Analyzing directory structure..."

# Find all README files in beta requirements
readmes=$(find "$BETA_ROOT" -name "README.md" ! -path "*/archive/*")

# Initialize counters
declare -A category_counts
declare -A category_p0
declare -A category_p1
declare -A category_p2
declare -A category_complete
declare -A category_partial
declare -A category_pending

categories=("drivers" "integrations" "cloud" "extensions" "deployment" "monitoring" "migration" "security")

for category in "${categories[@]}"; do
    category_counts[$category]=0
    category_p0[$category]=0
    category_p1[$category]=0
    category_p2[$category]=0
    category_complete[$category]=0
    category_partial[$category]=0
    category_pending[$category]=0
done

total_items=0
total_p0=0
total_p1=0
total_p2=0
total_complete=0
total_partial=0
total_pending=0

log_info "Scanning README files for priority and status markers..."

# Scan each README file
while IFS= read -r readme; do
    # Determine category from path
    category=""
    for cat in "${categories[@]}"; do
        if [[ "$readme" == *"/$cat/"* ]]; then
            category="$cat"
            break
        fi
    done

    if [ -z "$category" ]; then
        continue
    fi

    # Extract priority
    priority=""
    if grep -q "Priority.*P0" "$readme"; then
        priority="P0"
        ((category_p0[$category]++)) || true
        ((total_p0++)) || true
    elif grep -q "Priority.*P1" "$readme"; then
        priority="P1"
        ((category_p1[$category]++)) || true
        ((total_p1++)) || true
    elif grep -q "Priority.*P2" "$readme"; then
        priority="P2"
        ((category_p2[$category]++)) || true
        ((total_p2++)) || true
    fi

    # Extract status
    status="pending"
    if grep -q "Status.*Complete" "$readme" || grep -q "Status.*✅" "$readme"; then
        status="complete"
        ((category_complete[$category]++)) || true
        ((total_complete++)) || true
    elif grep -q "Status.*Partial" "$readme" || grep -q "Status.*🚧" "$readme"; then
        status="partial"
        ((category_partial[$category]++)) || true
        ((total_partial++)) || true
    else
        ((category_pending[$category]++)) || true
        ((total_pending++)) || true
    fi

    ((category_counts[$category]++)) || true
    ((total_items++)) || true

done <<< "$readmes"

# ============================================================================
# GENERATE MARKDOWN REPORT
# ============================================================================

log_info "Generating completion status report..."

cat > "$OUTPUT_FILE" << EOF
# Beta Requirements Completion Status

**Generated:** $TIMESTAMP
**Total Requirements:** $total_items

---

## 📊 Overall Progress

| Metric | Count | Percentage |
|--------|-------|------------|
| **Total Items** | $total_items | 100% |
| ✅ **Complete** | $total_complete | $(awk "BEGIN {printf \"%.1f\", $total_complete * 100.0 / $total_items}")% |
| 🚧 **Partial** | $total_partial | $(awk "BEGIN {printf \"%.1f\", $total_partial * 100.0 / $total_items}")% |
| ⏳ **Pending** | $total_pending | $(awk "BEGIN {printf \"%.1f\", $total_pending * 100.0 / $total_items}")% |

### Progress Bar

EOF

# Generate progress bar
completed_pct=$(awk "BEGIN {printf \"%.0f\", $total_complete * 100.0 / $total_items}")
partial_pct=$(awk "BEGIN {printf \"%.0f\", $total_partial * 100.0 / $total_items}")
pending_pct=$(awk "BEGIN {printf \"%.0f\", $total_pending * 100.0 / $total_items}")

echo "\`\`\`" >> "$OUTPUT_FILE"
echo -n "[" >> "$OUTPUT_FILE"
for ((i=0; i<completed_pct/2; i++)); do echo -n "█" >> "$OUTPUT_FILE"; done
for ((i=0; i<partial_pct/2; i++)); do echo -n "▓" >> "$OUTPUT_FILE"; done
for ((i=0; i<pending_pct/2; i++)); do echo -n "░" >> "$OUTPUT_FILE"; done
echo "]" >> "$OUTPUT_FILE"
echo "\`\`\`" >> "$OUTPUT_FILE"
echo "" >> "$OUTPUT_FILE"
echo "- █ Complete ($total_complete items)" >> "$OUTPUT_FILE"
echo "- ▓ Partial ($total_partial items)" >> "$OUTPUT_FILE"
echo "- ░ Pending ($total_pending items)" >> "$OUTPUT_FILE"
echo "" >> "$OUTPUT_FILE"

cat >> "$OUTPUT_FILE" << EOF
---

## 🎯 Priority Breakdown

| Priority | Items | Complete | Partial | Pending | Progress |
|----------|-------|----------|---------|---------|----------|
| **P0** (Critical) | $total_p0 | - | - | - | - |
| **P1** (High) | $total_p1 | - | - | - | - |
| **P2** (Medium) | $total_p2 | - | - | - | - |

### P0 Items (Beta Critical)
These items MUST be completed before Beta release.

**Total P0:** $total_p0 items

---

## 📁 Category Breakdown

EOF

# Generate category table
echo "| Category | Total | P0 | P1 | P2 | Complete | Partial | Pending | Progress |" >> "$OUTPUT_FILE"
echo "|----------|-------|----|----|----|---------|---------|---------|---------:|" >> "$OUTPUT_FILE"

for category in "${categories[@]}"; do
    count=${category_counts[$category]}
    if [ $count -gt 0 ]; then
        p0=${category_p0[$category]}
        p1=${category_p1[$category]}
        p2=${category_p2[$category]}
        complete=${category_complete[$category]}
        partial=${category_partial[$category]}
        pending=${category_pending[$category]}
        progress=$(awk "BEGIN {printf \"%.0f\", $complete * 100.0 / $count}")

        # Capitalize category name
        cat_name=$(echo "${category:0:1}" | tr '[:lower:]' '[:upper:]')${category:1}

        echo "| **$cat_name** | $count | $p0 | $p1 | $p2 | $complete | $partial | $pending | $progress% |" >> "$OUTPUT_FILE"
    fi
done

cat >> "$OUTPUT_FILE" << EOF

---

## 📝 Detailed Status by Category

EOF

# Generate detailed status for each category
for category in "${categories[@]}"; do
    count=${category_counts[$category]}
    if [ $count -eq 0 ]; then
        continue
    fi

    cat_name=$(echo "${category:0:1}" | tr '[:lower:]' '[:upper:]')${category:1}
    complete=${category_complete[$category]}
    partial=${category_partial[$category]}
    pending=${category_pending[$category]}
    progress=$(awk "BEGIN {printf \"%.0f\", $complete * 100.0 / $count}")

    cat >> "$OUTPUT_FILE" << EOF
### $cat_name ($progress% complete)

**Total:** $count items | ✅ $complete Complete | 🚧 $partial Partial | ⏳ $pending Pending

EOF

    # List items in this category
    category_readmes=$(find "$BETA_ROOT/$category" -name "README.md" 2>/dev/null | sort)

    if [ -n "$category_readmes" ]; then
        echo "| Item | Priority | Status |" >> "$OUTPUT_FILE"
        echo "|------|----------|--------|" >> "$OUTPUT_FILE"

        while IFS= read -r readme; do
            # Extract item name from path
            item_dir=$(dirname "$readme")
            item_name=$(basename "$item_dir")

            # Get priority
            priority="?"
            if grep -q "Priority.*P0" "$readme"; then
                priority="P0"
            elif grep -q "Priority.*P1" "$readme"; then
                priority="P1"
            elif grep -q "Priority.*P2" "$readme"; then
                priority="P2"
            fi

            # Get status
            status_icon="⏳"
            status_text="Pending"
            if grep -q "Status.*Complete" "$readme" || grep -q "Status.*✅" "$readme"; then
                status_icon="✅"
                status_text="Complete"
            elif grep -q "Status.*Partial" "$readme" || grep -q "Status.*🚧" "$readme"; then
                status_icon="🚧"
                status_text="Partial"
            fi

            # Format item name nicely
            formatted_name=$(echo "$item_name" | tr '-' ' ' | sed 's/\b\(.\)/\u\1/g')

            echo "| $formatted_name | $priority | $status_icon $status_text |" >> "$OUTPUT_FILE"

        done <<< "$category_readmes"

        echo "" >> "$OUTPUT_FILE"
    fi
done

cat >> "$OUTPUT_FILE" << EOF

---

## 🚀 Critical Path (P0 Items)

The following P0 items are critical for Beta release:

### Drivers (P0)
EOF

# List P0 drivers
p0_drivers=$(find "$BETA_ROOT/drivers" -name "README.md" -exec grep -l "Priority.*P0" {} \; 2>/dev/null | sort)

if [ -n "$p0_drivers" ]; then
    echo "" >> "$OUTPUT_FILE"
    while IFS= read -r readme; do
        item_dir=$(dirname "$readme")
        item_name=$(basename "$item_dir")
        formatted_name=$(echo "$item_name" | tr '-' ' ' | sed 's/\b\(.\)/\u\1/g')

        # Check status
        if grep -q "Status.*Complete" "$readme"; then
            status="✅ Complete"
        elif grep -q "Status.*Partial" "$readme"; then
            status="🚧 In Progress"
        else
            status="⏳ Pending"
        fi

        echo "- **$formatted_name** - $status" >> "$OUTPUT_FILE"
    done <<< "$p0_drivers"
else
    echo "" >> "$OUTPUT_FILE"
    echo "*No P0 drivers found*" >> "$OUTPUT_FILE"
fi

cat >> "$OUTPUT_FILE" << EOF

### Other P0 Items
EOF

# List other P0 items
other_p0=$(find "$BETA_ROOT" -name "README.md" ! -path "*/drivers/*" -exec grep -l "Priority.*P0" {} \; 2>/dev/null | sort)

if [ -n "$other_p0" ]; then
    echo "" >> "$OUTPUT_FILE"
    while IFS= read -r readme; do
        item_dir=$(dirname "$readme")
        category=$(basename "$(dirname "$item_dir")")
        item_name=$(basename "$item_dir")

        cat_formatted=$(echo "${category:0:1}" | tr '[:lower:]' '[:upper:]')${category:1}
        item_formatted=$(echo "$item_name" | tr '-' ' ' | sed 's/\b\(.\)/\u\1/g')

        # Check status
        if grep -q "Status.*Complete" "$readme"; then
            status="✅ Complete"
        elif grep -q "Status.*Partial" "$readme"; then
            status="🚧 In Progress"
        else
            status="⏳ Pending"
        fi

        echo "- **$cat_formatted / $item_formatted** - $status" >> "$OUTPUT_FILE"
    done <<< "$other_p0"
else
    echo "" >> "$OUTPUT_FILE"
    echo "*No other P0 items found*" >> "$OUTPUT_FILE"
fi

cat >> "$OUTPUT_FILE" << EOF

---

## 📈 Completion Metrics

### By Priority
| Priority | Total | Complete | % Complete |
|----------|-------|----------|-----------|
| P0 (Critical) | $total_p0 | - | - |
| P1 (High) | $total_p1 | - | - |
| P2 (Medium) | $total_p2 | - | - |

### By Category
EOF

# Sort categories by completion percentage
echo "| Category | Completion | Status |" >> "$OUTPUT_FILE"
echo "|----------|-----------|--------|" >> "$OUTPUT_FILE"

for category in "${categories[@]}"; do
    count=${category_counts[$category]}
    if [ $count -gt 0 ]; then
        complete=${category_complete[$category]}
        progress=$(awk "BEGIN {printf \"%.0f\", $complete * 100.0 / $count}")
        cat_name=$(echo "${category:0:1}" | tr '[:lower:]' '[:upper:]')${category:1}

        # Status icon based on progress
        if [ "$progress" -ge 80 ]; then
            status_icon="✅"
        elif [ "$progress" -ge 40 ]; then
            status_icon="🚧"
        else
            status_icon="⏳"
        fi

        echo "| $cat_name | $progress% ($complete/$count) | $status_icon |" >> "$OUTPUT_FILE"
    fi
done

cat >> "$OUTPUT_FILE" << EOF

---

## 🎯 Next Steps

### Immediate Priorities
1. Complete all P0 driver specifications
2. Complete P0 deployment requirements
3. Complete P0 security requirements

### This Week
- Focus on P0 items with "Pending" status
- Update partial items to complete
- Add missing specifications

### Before Beta Release
- All P0 items must be **Complete** ✅
- All P1 items should be at least **Partial** 🚧
- P2 items are post-Beta

---

## 📊 Historical Progress

Track progress over time:

| Date | Total Complete | P0 Complete | Overall % |
|------|---------------|-------------|-----------|
| $TIMESTAMP | $total_complete | - | $(awk "BEGIN {printf \"%.1f\", $total_complete * 100.0 / $total_items}")% |

*Add new rows each week to track progress*

---

**Generated by:** \`scripts/beta-requirements-tracker.sh\`
**Last Updated:** $TIMESTAMP

To regenerate: \`./scripts/beta-requirements-tracker.sh\`
EOF

log_success "Beta requirements status written to $OUTPUT_FILE"

# Console summary
echo ""
log_info "Beta Requirements Summary:"
echo "  Total Items:  $total_items"
echo "  P0 (Critical): $total_p0"
echo "  P1 (High):     $total_p1"
echo "  P2 (Medium):   $total_p2"
echo ""
echo "  ✅ Complete:   $total_complete ($(awk "BEGIN {printf \"%.1f\", $total_complete * 100.0 / $total_items}")%)"
echo "  🚧 Partial:    $total_partial ($(awk "BEGIN {printf \"%.1f\", $total_partial * 100.0 / $total_items}")%)"
echo "  ⏳ Pending:    $total_pending ($(awk "BEGIN {printf \"%.1f\", $total_pending * 100.0 / $total_items}")%)"
echo ""
log_success "Report generated: $OUTPUT_FILE"
