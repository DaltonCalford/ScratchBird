#!/bin/bash
#
# Convert all compatibility tests from original repositories to SQL format
#
# This script processes tests from:
#   - Firebird fbt-repository (2,283 .fbt files)
#   - MySQL mysql-server (1,512 .test files)
#   - PostgreSQL postgres (238 .sql files)
#
# Total: ~4,033 test files
#

set -e

# Use absolute paths
SCRIPT_DIR="/home/dcalford/CliWork/ScratchBird/tests/compatibility/scripts"
COMPAT_DIR="/home/dcalford/CliWork/ScratchBird/tests/compatibility"

# Color output
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m' # No Color

echo ""
echo "╔════════════════════════════════════════════════════════════╗"
echo "║  Converting All Compatibility Tests to SQL Format         ║"
echo "╚════════════════════════════════════════════════════════════╝"
echo ""

# Track statistics
total_converted=0
total_failed=0

#
# Convert Firebird Tests
#
echo ""
echo "═══════════════════════════════════════════════════════════"
echo "  Converting Firebird Tests (.fbt → .sql)"
echo "═══════════════════════════════════════════════════════════"
echo ""

FIREBIRD_REPO="$COMPAT_DIR/firebird/repos/fbt-repository/tests"
FIREBIRD_OUTPUT="$COMPAT_DIR/firebird/converted"
FIREBIRD_SCRIPT="$COMPAT_DIR/firebird/scripts/convert_fbt_to_sql.py"

if [ -d "$FIREBIRD_REPO" ]; then
    # Clear previous conversions
    rm -rf "$FIREBIRD_OUTPUT"
    mkdir -p "$FIREBIRD_OUTPUT"

    firebird_count=0
    firebird_failed=0

    echo "Scanning Firebird test repository..."
    mapfile -t fbt_files < <(find "$FIREBIRD_REPO" -name "*.fbt" | sort)
    total_fbt=${#fbt_files[@]}
    echo "Found $total_fbt .fbt files"
    echo ""

    for fbt_file in "${fbt_files[@]}"; do
        # Determine category from path
        rel_path="${fbt_file#$FIREBIRD_REPO/}"
        category=$(dirname "$rel_path")

        # Create output directory matching source structure
        output_dir="$FIREBIRD_OUTPUT/$category"
        mkdir -p "$output_dir"

        # Convert file
        if python3 "$FIREBIRD_SCRIPT" "$fbt_file" "$output_dir" >/dev/null 2>&1; then
            ((firebird_count++))
        else
            ((firebird_failed++))
            echo -e "${RED}✗${NC} Failed: $(basename "$fbt_file")"
        fi

        # Progress indicator (every 100 files)
        if (( firebird_count % 100 == 0 )); then
            echo -ne "\rProgress: $firebird_count / $total_fbt files converted..."
        fi
    done

    echo -ne "\r"
    echo -e "${GREEN}✓${NC} Converted $firebird_count Firebird tests"
    if (( firebird_failed > 0 )); then
        echo -e "${YELLOW}⚠${NC}  Failed: $firebird_failed tests"
    fi

    total_converted=$((total_converted + firebird_count))
    total_failed=$((total_failed + firebird_failed))
else
    echo -e "${YELLOW}⚠${NC}  Firebird repository not found at: $FIREBIRD_REPO"
    echo "  Run: git submodule update --init"
fi

#
# Convert MySQL Tests
#
echo ""
echo "═══════════════════════════════════════════════════════════"
echo "  Converting MySQL Tests (.test → .sql)"
echo "═══════════════════════════════════════════════════════════"
echo ""

MYSQL_REPO="$COMPAT_DIR/mysql/repos/mysql-server/mysql-test"
MYSQL_OUTPUT="$COMPAT_DIR/mysql/converted"
MYSQL_SCRIPT="$COMPAT_DIR/mysql/scripts/convert_mysql_test.py"

if [ -d "$MYSQL_REPO" ]; then
    # Clear previous conversions
    rm -rf "$MYSQL_OUTPUT"
    mkdir -p "$MYSQL_OUTPUT"

    mysql_count=0
    mysql_failed=0

    echo "Scanning MySQL test repository..."

    # Find all test suites
    # Main tests are in mysql-test/t/
    # Suite tests are in mysql-test/suite/*/t/
    mapfile -t test_files < <(find "$MYSQL_REPO/t" -name "*.test" | sort)
    mapfile -t suite_files < <(find "$MYSQL_REPO/suite" -type d -name "t" -exec find {} -name "*.test" \; | sort)

    all_test_files=("${test_files[@]}" "${suite_files[@]}")
    total_mysql=${#all_test_files[@]}
    echo "Found $total_mysql .test files"
    echo ""

    for test_file in "${all_test_files[@]}"; do
        # Determine suite/category
        if [[ "$test_file" == *"/suite/"* ]]; then
            # Suite test: extract suite name
            suite=$(echo "$test_file" | sed -n 's|.*/suite/\([^/]*\)/.*|\1|p')
            output_dir="$MYSQL_OUTPUT/$suite"
        else
            # Main test
            output_dir="$MYSQL_OUTPUT/main"
        fi

        mkdir -p "$output_dir"

        # Convert file
        if python3 "$MYSQL_SCRIPT" "$test_file" "$output_dir" >/dev/null 2>&1; then
            ((mysql_count++))
        else
            ((mysql_failed++))
            echo -e "${RED}✗${NC} Failed: $(basename "$test_file")"
        fi

        # Progress indicator (every 100 files)
        if (( mysql_count % 100 == 0 )); then
            echo -ne "\rProgress: $mysql_count / $total_mysql files converted..."
        fi
    done

    echo -ne "\r"
    echo -e "${GREEN}✓${NC} Converted $mysql_count MySQL tests"
    if (( mysql_failed > 0 )); then
        echo -e "${YELLOW}⚠${NC}  Failed: $mysql_failed tests"
    fi

    total_converted=$((total_converted + mysql_count))
    total_failed=$((total_failed + mysql_failed))
else
    echo -e "${YELLOW}⚠${NC}  MySQL repository not found at: $MYSQL_REPO"
    echo "  Run: git submodule update --init"
fi

#
# Convert PostgreSQL Tests
#
echo ""
echo "═══════════════════════════════════════════════════════════"
echo "  Converting PostgreSQL Tests (.sql → standardized .sql)"
echo "═══════════════════════════════════════════════════════════"
echo ""

POSTGRES_REPO="$COMPAT_DIR/postgresql/repos/postgres/src/test/regress/sql"
POSTGRES_OUTPUT="$COMPAT_DIR/postgresql/converted"
POSTGRES_SCRIPT="$COMPAT_DIR/postgresql/scripts/convert_pg_test.py"

if [ -d "$POSTGRES_REPO" ]; then
    # Clear previous conversions
    rm -rf "$POSTGRES_OUTPUT"
    mkdir -p "$POSTGRES_OUTPUT/core"

    postgres_count=0
    postgres_failed=0

    echo "Scanning PostgreSQL test repository..."
    mapfile -t sql_files < <(find "$POSTGRES_REPO" -name "*.sql" | sort)
    total_postgres=${#sql_files[@]}
    echo "Found $total_postgres .sql files"
    echo ""

    for sql_file in "${sql_files[@]}"; do
        # Convert file
        if python3 "$POSTGRES_SCRIPT" "$sql_file" "$POSTGRES_OUTPUT/core" 2>/dev/null; then
            ((postgres_count++))
        else
            ((postgres_failed++))
            echo -e "${RED}✗${NC} Failed: $(basename "$sql_file")"
        fi

        # Progress indicator (every 50 files)
        if (( postgres_count % 50 == 0 )); then
            echo -ne "\rProgress: $postgres_count / $total_postgres files converted..."
        fi
    done

    echo -ne "\r"
    echo -e "${GREEN}✓${NC} Converted $postgres_count PostgreSQL tests"
    if (( postgres_failed > 0 )); then
        echo -e "${YELLOW}⚠${NC}  Failed: $postgres_failed tests"
    fi

    total_converted=$((total_converted + postgres_count))
    total_failed=$((total_failed + postgres_failed))
else
    echo -e "${YELLOW}⚠${NC}  PostgreSQL repository not found at: $POSTGRES_REPO"
    echo "  Run: git submodule update --init"
fi

#
# Summary
#
echo ""
echo "═══════════════════════════════════════════════════════════"
echo "  Conversion Summary"
echo "═══════════════════════════════════════════════════════════"
echo ""
echo "Total Tests Converted: $total_converted"
if (( total_failed > 0 )); then
    echo -e "${YELLOW}Total Failed:          $total_failed${NC}"
fi
echo ""

# Generate conversion report
REPORT_FILE="$COMPAT_DIR/results/conversion_report_$(date +%Y%m%d_%H%M%S).txt"
mkdir -p "$COMPAT_DIR/results"

cat > "$REPORT_FILE" <<EOF
Compatibility Test Conversion Report
Generated: $(date)

═══════════════════════════════════════════════════════════

Firebird Tests:
  Converted: $firebird_count
  Failed:    $firebird_failed
  Output:    $FIREBIRD_OUTPUT/

MySQL Tests:
  Converted: $mysql_count
  Failed:    $mysql_failed
  Output:    $MYSQL_OUTPUT/

PostgreSQL Tests:
  Converted: $postgres_count
  Failed:    $postgres_failed
  Output:    $POSTGRES_OUTPUT/

═══════════════════════════════════════════════════════════

Total Converted: $total_converted
Total Failed:    $total_failed

Conversion Rate: $(awk "BEGIN {printf \"%.1f\", 100*$total_converted/($total_converted+$total_failed)}")%

═══════════════════════════════════════════════════════════
EOF

echo "Report saved to: $REPORT_FILE"
echo ""

if (( total_failed == 0 )); then
    echo -e "${GREEN}✓ All tests converted successfully!${NC}"
    exit 0
else
    echo -e "${YELLOW}⚠ Conversion completed with some failures${NC}"
    exit 1
fi
