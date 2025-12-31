#!/bin/bash
# ScratchBird Performance Benchmark Tests
# Measures query performance, throughput, and latency

set -euo pipefail

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
RESULTS_DIR="${ROOT_DIR}/results/performance"
BUILD_DIR="${ROOT_DIR}/../../build"

# Test binary paths
SB_ISQL="${BUILD_DIR}/sb_isql"

# Test parameters
ROWS_SMALL=1000
ROWS_MEDIUM=10000
ROWS_LARGE=100000

# Function: Print colored message
print_msg() {
    local color=$1
    shift
    echo -e "${color}$*${NC}"
}

# Function: Run timed query
run_timed_query() {
    local query="$1"
    local description="$2"

    local start_time=$(date +%s%N)
    echo "${query}" | "${SB_ISQL}" > /dev/null 2>&1
    local end_time=$(date +%s%N)

    local elapsed_ms=$(( (end_time - start_time) / 1000000 ))
    printf "%-50s %8d ms\n" "${description}" "${elapsed_ms}"

    echo "${elapsed_ms}"
}

# Function: Setup test database
setup_test_db() {
    print_msg "${BLUE}" "Setting up test database..."

    cat <<'EOF' | "${SB_ISQL}" > /dev/null 2>&1
CREATE DATABASE perf_test;
\c perf_test;

-- Create tables for testing
CREATE TABLE test_seq (
    id INTEGER PRIMARY KEY,
    data VARCHAR(100),
    value INTEGER,
    timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE test_random (
    id INTEGER PRIMARY KEY,
    random_key INTEGER,
    data VARCHAR(100)
);

CREATE TABLE test_join_a (
    id INTEGER PRIMARY KEY,
    name VARCHAR(100)
);

CREATE TABLE test_join_b (
    id INTEGER PRIMARY KEY,
    a_id INTEGER,
    value INTEGER
);
EOF

    print_msg "${GREEN}" "✓ Database created"
}

# Function: Benchmark INSERT performance
benchmark_insert() {
    print_msg "${BLUE}" "\n1. INSERT Performance"
    print_msg "${BLUE}" "========================================"

    # Sequential inserts
    local insert_query=$(cat <<EOF
\c perf_test;
INSERT INTO test_seq SELECT generate_series(1, ${ROWS_SMALL}), 'Data_' || generate_series(1, ${ROWS_SMALL}), random() * 1000;
EOF
)

    local time_ms=$(run_timed_query "${insert_query}" "Sequential INSERT (${ROWS_SMALL} rows)")
    local rows_per_sec=$(( ROWS_SMALL * 1000 / time_ms ))
    print_msg "${GREEN}" "  → ${rows_per_sec} rows/sec"

    # Batch insert
    insert_query=$(cat <<EOF
\c perf_test;
BEGIN TRANSACTION;
INSERT INTO test_random SELECT generate_series(1, ${ROWS_MEDIUM}), random() * 100000, 'Random_' || generate_series(1, ${ROWS_MEDIUM});
COMMIT;
EOF
)

    time_ms=$(run_timed_query "${insert_query}" "Batch INSERT in transaction (${ROWS_MEDIUM} rows)")
    rows_per_sec=$(( ROWS_MEDIUM * 1000 / time_ms ))
    print_msg "${GREEN}" "  → ${rows_per_sec} rows/sec"
}

# Function: Benchmark SELECT performance
benchmark_select() {
    print_msg "${BLUE}" "\n2. SELECT Performance"
    print_msg "${BLUE}" "========================================"

    # Full table scan
    local select_query=$(cat <<EOF
\c perf_test;
SELECT COUNT(*) FROM test_seq;
EOF
)
    run_timed_query "${select_query}" "Full table scan COUNT(*)"

    # WHERE clause
    select_query=$(cat <<EOF
\c perf_test;
SELECT * FROM test_seq WHERE id < 100;
EOF
)
    run_timed_query "${select_query}" "Index scan with WHERE (100 rows)"

    # ORDER BY
    select_query=$(cat <<EOF
\c perf_test;
SELECT * FROM test_seq ORDER BY value LIMIT 100;
EOF
)
    run_timed_query "${select_query}" "ORDER BY with LIMIT"

    # Aggregation
    select_query=$(cat <<EOF
\c perf_test;
SELECT value / 100 AS bucket, COUNT(*), AVG(value) FROM test_seq GROUP BY value / 100;
EOF
)
    run_timed_query "${select_query}" "GROUP BY with aggregation"
}

# Function: Benchmark UPDATE performance
benchmark_update() {
    print_msg "${BLUE}" "\n3. UPDATE Performance"
    print_msg "${BLUE}" "========================================"

    # Single row update
    local update_query=$(cat <<EOF
\c perf_test;
UPDATE test_seq SET data = 'Updated' WHERE id = 500;
EOF
)
    run_timed_query "${update_query}" "Single row UPDATE"

    # Batch update
    update_query=$(cat <<EOF
\c perf_test;
BEGIN TRANSACTION;
UPDATE test_seq SET value = value + 1 WHERE id < 100;
COMMIT;
EOF
)
    run_timed_query "${update_query}" "Batch UPDATE (100 rows)"

    # Full table update
    update_query=$(cat <<EOF
\c perf_test;
BEGIN TRANSACTION;
UPDATE test_random SET data = 'Updated_' || id;
COMMIT;
EOF
)
    run_timed_query "${update_query}" "Full table UPDATE (${ROWS_MEDIUM} rows)"
}

# Function: Benchmark DELETE performance
benchmark_delete() {
    print_msg "${BLUE}" "\n4. DELETE Performance"
    print_msg "${BLUE}" "========================================"

    # Delete with WHERE
    local delete_query=$(cat <<EOF
\c perf_test;
BEGIN TRANSACTION;
DELETE FROM test_random WHERE random_key < 10000;
COMMIT;
EOF
)
    run_timed_query "${delete_query}" "DELETE with WHERE (subset)"

    # TRUNCATE
    delete_query=$(cat <<EOF
\c perf_test;
TRUNCATE TABLE test_random;
EOF
)
    run_timed_query "${delete_query}" "TRUNCATE TABLE"
}

# Function: Benchmark INDEX performance
benchmark_index() {
    print_msg "${BLUE}" "\n5. INDEX Performance"
    print_msg "${BLUE}" "========================================"

    # Create index
    local index_query=$(cat <<EOF
\c perf_test;
CREATE INDEX idx_test_seq_value ON test_seq(value);
EOF
)
    run_timed_query "${index_query}" "CREATE INDEX (${ROWS_SMALL} rows)"

    # Query using index
    index_query=$(cat <<EOF
\c perf_test;
SELECT * FROM test_seq WHERE value BETWEEN 100 AND 200;
EOF
)
    run_timed_query "${index_query}" "Index scan with BETWEEN"

    # Drop index
    index_query=$(cat <<EOF
\c perf_test;
DROP INDEX idx_test_seq_value;
EOF
)
    run_timed_query "${index_query}" "DROP INDEX"
}

# Function: Benchmark JOIN performance
benchmark_join() {
    print_msg "${BLUE}" "\n6. JOIN Performance"
    print_msg "${BLUE}" "========================================"

    # Setup join tables
    cat <<EOF | "${SB_ISQL}" > /dev/null 2>&1
\c perf_test;
INSERT INTO test_join_a SELECT generate_series(1, 1000), 'Name_' || generate_series(1, 1000);
INSERT INTO test_join_b SELECT generate_series(1, 5000), (random() * 999 + 1)::INTEGER, random() * 10000;
CREATE INDEX idx_join_b_a_id ON test_join_b(a_id);
EOF

    # INNER JOIN
    local join_query=$(cat <<EOF
\c perf_test;
SELECT a.name, COUNT(b.id), AVG(b.value)
FROM test_join_a a
INNER JOIN test_join_b b ON a.id = b.a_id
GROUP BY a.name;
EOF
)
    run_timed_query "${join_query}" "INNER JOIN with GROUP BY"

    # LEFT JOIN
    join_query=$(cat <<EOF
\c perf_test;
SELECT a.id, a.name, b.value
FROM test_join_a a
LEFT JOIN test_join_b b ON a.id = b.a_id
WHERE b.value > 5000;
EOF
)
    run_timed_query "${join_query}" "LEFT JOIN with WHERE"
}

# Function: Benchmark TRANSACTION performance
benchmark_transaction() {
    print_msg "${BLUE}" "\n7. TRANSACTION Performance"
    print_msg "${BLUE}" "========================================"

    # Multiple small transactions
    local start_time=$(date +%s%N)
    for i in {1..100}; do
        cat <<EOF | "${SB_ISQL}" > /dev/null 2>&1
\c perf_test;
BEGIN TRANSACTION;
INSERT INTO test_seq VALUES (${ROWS_SMALL} + ${i}, 'TX_${i}', ${i});
COMMIT;
EOF
    done
    local end_time=$(date +%s%N)
    local elapsed_ms=$(( (end_time - start_time) / 1000000 ))
    printf "%-50s %8d ms\n" "100 small transactions (1 INSERT each)" "${elapsed_ms}"
    local tx_per_sec=$(( 100 * 1000 / elapsed_ms ))
    print_msg "${GREEN}" "  → ${tx_per_sec} transactions/sec"

    # Large transaction with SAVEPOINT
    local tx_query=$(cat <<EOF
\c perf_test;
BEGIN TRANSACTION;
INSERT INTO test_seq SELECT ${ROWS_SMALL} + 200 + generate_series(1, 100), 'SP_' || generate_series(1, 100), generate_series(1, 100);
SAVEPOINT sp1;
UPDATE test_seq SET value = value + 1 WHERE id > ${ROWS_SMALL} + 200;
SAVEPOINT sp2;
DELETE FROM test_seq WHERE id > ${ROWS_SMALL} + 250;
ROLLBACK TO SAVEPOINT sp2;
COMMIT;
EOF
)
    run_timed_query "${tx_query}" "Transaction with SAVEPOINT"
}

# Function: Benchmark concurrency (simplified)
benchmark_concurrency() {
    print_msg "${BLUE}" "\n8. Concurrency Simulation"
    print_msg "${BLUE}" "========================================"

    # Run 10 concurrent queries
    local start_time=$(date +%s%N)
    for i in {1..10}; do
        (echo "\\c perf_test; SELECT COUNT(*) FROM test_seq;" | "${SB_ISQL}" > /dev/null 2>&1) &
    done
    wait
    local end_time=$(date +%s%N)
    local elapsed_ms=$(( (end_time - start_time) / 1000000 ))
    printf "%-50s %8d ms\n" "10 concurrent SELECT queries" "${elapsed_ms}"
}

# Function: Cleanup
cleanup_test_db() {
    print_msg "${BLUE}" "\nCleaning up..."

    cat <<'EOF' | "${SB_ISQL}" > /dev/null 2>&1
\c perf_test;
DROP TABLE test_join_b CASCADE;
DROP TABLE test_join_a CASCADE;
DROP TABLE test_random CASCADE;
DROP TABLE test_seq CASCADE;
DROP DATABASE perf_test;
EOF

    print_msg "${GREEN}" "✓ Cleanup complete"
}

# Function: Generate summary report
generate_report() {
    local report_file="${RESULTS_DIR}/benchmark_report_$(date +%Y%m%d_%H%M%S).txt"
    mkdir -p "${RESULTS_DIR}"

    cat > "${report_file}" <<EOF
ScratchBird Performance Benchmark Report
Generated: $(date)
========================================

System Information:
- CPU: $(lscpu | grep "Model name" | cut -d: -f2 | xargs)
- Memory: $(free -h | grep Mem | awk '{print $2}')
- OS: $(uname -s) $(uname -r)

Test Configuration:
- Small dataset: ${ROWS_SMALL} rows
- Medium dataset: ${ROWS_MEDIUM} rows
- Large dataset: ${ROWS_LARGE} rows

Results saved to: ${RESULTS_DIR}/
========================================
EOF

    print_msg "${BLUE}" "\nReport saved to: ${report_file}"
}

# Main script
main() {
    print_msg "${BLUE}" "========================================"
    print_msg "${BLUE}" "ScratchBird Performance Benchmarks"
    print_msg "${BLUE}" "========================================"
    echo

    # Check prerequisites
    if [[ ! -x "${SB_ISQL}" ]]; then
        print_msg "${RED}" "ERROR: sb_isql not found at ${SB_ISQL}"
        exit 1
    fi

    mkdir -p "${RESULTS_DIR}"

    # Run benchmarks
    setup_test_db
    benchmark_insert
    benchmark_select
    benchmark_update
    benchmark_delete
    benchmark_index
    benchmark_join
    benchmark_transaction
    benchmark_concurrency

    # Cleanup and report
    cleanup_test_db
    generate_report

    echo
    print_msg "${GREEN}" "✓ All benchmarks complete!"
}

main "$@"
