#!/bin/bash
#
# Convert all compatibility tests using parallel processing
#
# This script uses xargs -P to run conversions in parallel (8 jobs at a time)
# This should significantly speed up the conversion of ~4,000 test files
#

set -e

# Use absolute paths
COMPAT_DIR="/home/dcalford/CliWork/ScratchBird/tests/compatibility"

# Number of parallel jobs (adjust based on CPU cores)
PARALLEL_JOBS=8

echo ""
echo "╔════════════════════════════════════════════════════════════╗"
echo "║  Converting All Compatibility Tests (Parallel Mode)       ║"
echo "╚════════════════════════════════════════════════════════════╝"
echo ""
echo "Using $PARALLEL_JOBS parallel jobs"
echo ""

#
# Convert Firebird Tests
#
echo "═══════════════════════════════════════════════════════════"
echo "  Converting Firebird Tests (.fbt → .sql)"
echo "═══════════════════════════════════════════════════════════"

FIREBIRD_REPO="$COMPAT_DIR/firebird/repos/fbt-repository/tests"
FIREBIRD_OUTPUT="$COMPAT_DIR/firebird/converted"
FIREBIRD_SCRIPT="$COMPAT_DIR/firebird/scripts/convert_fbt_to_sql.py"

if [ -d "$FIREBIRD_REPO" ]; then
    # Clear previous conversions
    rm -rf "$FIREBIRD_OUTPUT"
    mkdir -p "$FIREBIRD_OUTPUT"

    echo "Scanning Firebird test repository..."
    total_fbt=$(find "$FIREBIRD_REPO" -name "*.fbt" | wc -l)
    echo "Found $total_fbt .fbt files"
    echo "Converting... (this may take several minutes)"

    # Create temporary script for parallel execution
    TEMP_SCRIPT=$(mktemp)
    cat > "$TEMP_SCRIPT" <<'SCRIPTEOF'
#!/bin/bash
fbt_file="$1"
FIREBIRD_REPO="$2"
FIREBIRD_OUTPUT="$3"
FIREBIRD_SCRIPT="$4"

# Determine category from path
rel_path="${fbt_file#$FIREBIRD_REPO/}"
category=$(dirname "$rel_path")

# Create output directory matching source structure
output_dir="$FIREBIRD_OUTPUT/$category"
mkdir -p "$output_dir"

# Convert file (suppress output for cleaner display)
python3 "$FIREBIRD_SCRIPT" "$fbt_file" "$output_dir" >/dev/null 2>&1

# Return success/failure
if [ $? -eq 0 ]; then
    echo "✓"
else
    echo "✗ $(basename "$fbt_file")" >&2
fi
SCRIPTEOF

    chmod +x "$TEMP_SCRIPT"

    # Run conversions in parallel
    find "$FIREBIRD_REPO" -name "*.fbt" | \
        xargs -P "$PARALLEL_JOBS" -I {} "$TEMP_SCRIPT" {} "$FIREBIRD_REPO" "$FIREBIRD_OUTPUT" "$FIREBIRD_SCRIPT" | \
        grep -c "✓" || firebird_count=0

    rm "$TEMP_SCRIPT"

    # Count actual converted files
    firebird_count=$(find "$FIREBIRD_OUTPUT" -name "*.sql" | wc -l)
    echo "✓ Converted $firebird_count Firebird test files"
else
    echo "⚠  Firebird repository not found"
    firebird_count=0
fi

#
# Convert MySQL Tests
#
echo ""
echo "═══════════════════════════════════════════════════════════"
echo "  Converting MySQL Tests (.test → .sql)"
echo "═══════════════════════════════════════════════════════════"

MYSQL_REPO="$COMPAT_DIR/mysql/repos/mysql-server/mysql-test"
MYSQL_OUTPUT="$COMPAT_DIR/mysql/converted"
MYSQL_SCRIPT="$COMPAT_DIR/mysql/scripts/convert_mysql_test.py"

if [ -d "$MYSQL_REPO" ]; then
    # Clear previous conversions
    rm -rf "$MYSQL_OUTPUT"
    mkdir -p "$MYSQL_OUTPUT"

    echo "Scanning MySQL test repository..."
    total_mysql=$(find "$MYSQL_REPO/t" -name "*.test" | wc -l)
    suite_count=$(find "$MYSQL_REPO/suite" -type d -name "t" -exec find {} -name "*.test" \; 2>/dev/null | wc -l)
    total_mysql=$((total_mysql + suite_count))
    echo "Found $total_mysql .test files"
    echo "Converting... (this may take several minutes)"

    # Create temporary script for parallel execution
    TEMP_SCRIPT=$(mktemp)
    cat > "$TEMP_SCRIPT" <<'SCRIPTEOF'
#!/bin/bash
test_file="$1"
MYSQL_OUTPUT="$2"
MYSQL_SCRIPT="$3"

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

# Convert file (suppress output for cleaner display)
python3 "$MYSQL_SCRIPT" "$test_file" "$output_dir" >/dev/null 2>&1

# Return success/failure
if [ $? -eq 0 ]; then
    echo "✓"
else
    echo "✗ $(basename "$test_file")" >&2
fi
SCRIPTEOF

    chmod +x "$TEMP_SCRIPT"

    # Run conversions in parallel
    { find "$MYSQL_REPO/t" -name "*.test"; \
      find "$MYSQL_REPO/suite" -type d -name "t" -exec find {} -name "*.test" \; 2>/dev/null; } | \
        xargs -P "$PARALLEL_JOBS" -I {} "$TEMP_SCRIPT" {} "$MYSQL_OUTPUT" "$MYSQL_SCRIPT" | \
        grep -c "✓" || mysql_count=0

    rm "$TEMP_SCRIPT"

    # Count actual converted files
    mysql_count=$(find "$MYSQL_OUTPUT" -name "*.sql" | wc -l)
    echo "✓ Converted $mysql_count MySQL test files"
else
    echo "⚠  MySQL repository not found"
    mysql_count=0
fi

#
# Convert PostgreSQL Tests
#
echo ""
echo "═══════════════════════════════════════════════════════════"
echo "  Converting PostgreSQL Tests (.sql → standardized .sql)"
echo "═══════════════════════════════════════════════════════════"

POSTGRES_REPO="$COMPAT_DIR/postgresql/repos/postgres/src/test/regress/sql"
POSTGRES_OUTPUT="$COMPAT_DIR/postgresql/converted"
POSTGRES_SCRIPT="$COMPAT_DIR/postgresql/scripts/convert_pg_test.py"

if [ -d "$POSTGRES_REPO" ]; then
    # Clear previous conversions
    rm -rf "$POSTGRES_OUTPUT"
    mkdir -p "$POSTGRES_OUTPUT/core"

    echo "Scanning PostgreSQL test repository..."
    total_postgres=$(find "$POSTGRES_REPO" -name "*.sql" | wc -l)
    echo "Found $total_postgres .sql files"
    echo "Converting..."

    # Run conversions in parallel
    find "$POSTGRES_REPO" -name "*.sql" | \
        xargs -P "$PARALLEL_JOBS" -I {} python3 "$POSTGRES_SCRIPT" {} "$POSTGRES_OUTPUT/core" >/dev/null 2>&1

    # Count actual converted files
    postgres_count=$(find "$POSTGRES_OUTPUT" -name "*.sql" | wc -l)
    echo "✓ Converted $postgres_count PostgreSQL test files"
else
    echo "⚠  PostgreSQL repository not found"
    postgres_count=0
fi

#
# Summary
#
total_converted=$((firebird_count + mysql_count + postgres_count))

echo ""
echo "═══════════════════════════════════════════════════════════"
echo "  Conversion Summary"
echo "═══════════════════════════════════════════════════════════"
echo ""
echo "Firebird:    $firebird_count tests"
echo "MySQL:       $mysql_count tests"
echo "PostgreSQL:  $postgres_count tests"
echo ""
echo "Total:       $total_converted tests converted"
echo ""

# Generate conversion report
REPORT_FILE="$COMPAT_DIR/results/conversion_report_$(date +%Y%m%d_%H%M%S).txt"
mkdir -p "$COMPAT_DIR/results"

cat > "$REPORT_FILE" <<EOF
Compatibility Test Conversion Report
Generated: $(date)

═══════════════════════════════════════════════════════════

Firebird Tests:    $firebird_count files
MySQL Tests:       $mysql_count files
PostgreSQL Tests:  $postgres_count files

Total Converted:   $total_converted files

Parallel Jobs:     $PARALLEL_JOBS

═══════════════════════════════════════════════════════════

Output Directories:
  Firebird:    $COMPAT_DIR/firebird/converted/
  MySQL:       $COMPAT_DIR/mysql/converted/
  PostgreSQL:  $COMPAT_DIR/postgresql/converted/

═══════════════════════════════════════════════════════════
EOF

echo "Report saved to: $REPORT_FILE"
echo ""
echo "✓ Conversion complete!"
