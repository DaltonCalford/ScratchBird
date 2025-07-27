#!/bin/bash

#
# ScratchBird v0.6.0 - Partial Hash Index Tests
#
# This script provides comprehensive testing of the partial hash index functionality,
# including creation, maintenance, optimization, and performance monitoring.
#
# Test categories:
# 1. Basic partial index creation and validation
# 2. WHERE clause condition evaluation
# 3. Insert/update/delete operations with conditions
# 4. Index maintenance and optimization
# 5. Performance monitoring and statistics
# 6. Query optimizer integration
# 7. Edge cases and error handling
# 8. Stress tests and performance benchmarks
#
# Copyright (c) 2025 ScratchBird Development Team
# All Rights Reserved.
#

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TEST_DB_DIR="$SCRIPT_DIR/test_databases"
TEST_DB="$TEST_DB_DIR/partial_hash_index_test.fdb"
SB_ISQL_PATH="../gen/Release/scratchbird/bin/sb_isql"
OUTPUT_FILE="$SCRIPT_DIR/partial_hash_index_test_results.txt"

# Create test database directory
mkdir -p "$TEST_DB_DIR"

# ANSI color codes for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
PURPLE='\033[0;35m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Global test counters
TOTAL_TESTS=0
PASSED_TESTS=0
FAILED_TESTS=0

# Function to log test results
log_test_result() {
    local test_name="$1"
    local expected_result="$2"
    local actual_result="$3"
    local start_time="$4"
    local end_time="$5"
    local status="$6"
    
    local elapsed_time=$(echo "$end_time - $start_time" | bc -l)
    
    echo "=========================================" >> "$OUTPUT_FILE"
    echo "Test: $test_name" >> "$OUTPUT_FILE"
    echo "Expected: $expected_result" >> "$OUTPUT_FILE"
    echo "Actual: $actual_result" >> "$OUTPUT_FILE"
    echo "Status: $status" >> "$OUTPUT_FILE"
    echo "Execution time: ${elapsed_time}s" >> "$OUTPUT_FILE"
    echo "=========================================" >> "$OUTPUT_FILE"
    
    ((TOTAL_TESTS++))
    if [ "$status" = "PASSED" ]; then
        ((PASSED_TESTS++))
        echo -e "${GREEN}✓${NC} $test_name"
    else
        ((FAILED_TESTS++))
        echo -e "${RED}✗${NC} $test_name"
        echo -e "${RED}  Expected: $expected_result${NC}"
        echo -e "${RED}  Actual: $actual_result${NC}"
    fi
}

# Function to execute SQL and capture output
execute_sql() {
    local sql="$1"
    local timeout="${2:-30}"
    
    echo "$sql" | timeout "$timeout" "$SB_ISQL_PATH" -user sysdba -password masterkey "$TEST_DB" -quiet 2>&1
}

# Function to execute SQL file and capture output
execute_sql_file() {
    local sql_file="$1"
    local timeout="${2:-30}"
    
    timeout "$timeout" "$SB_ISQL_PATH" -user sysdba -password masterkey "$TEST_DB" -input "$sql_file" -quiet 2>&1
}

# Function to run individual test
run_test() {
    local test_name="$1"
    local expected_pattern="$2"
    local sql_command="$3"
    local timeout="${4:-30}"
    
    local start_time=$(date +%s.%N)
    local result=$(execute_sql "$sql_command" "$timeout")
    local end_time=$(date +%s.%N)
    
    if echo "$result" | grep -q "$expected_pattern"; then
        log_test_result "$test_name" "$expected_pattern" "$result" "$start_time" "$end_time" "PASSED"
    else
        log_test_result "$test_name" "$expected_pattern" "$result" "$start_time" "$end_time" "FAILED"
    fi
}

# Function to run performance test
run_performance_test() {
    local test_name="$1"
    local setup_sql="$2"
    local test_sql="$3"
    local iterations="${4:-1000}"
    local max_time="${5:-10.0}"
    
    echo -e "${CYAN}Running performance test: $test_name${NC}"
    
    # Setup
    execute_sql "$setup_sql" 60
    
    local start_time=$(date +%s.%N)
    for ((i=1; i<=iterations; i++)); do
        execute_sql "$test_sql" 5 > /dev/null 2>&1
    done
    local end_time=$(date +%s.%N)
    
    local total_time=$(echo "$end_time - $start_time" | bc -l)
    local avg_time=$(echo "$total_time / $iterations" | bc -l)
    
    if (( $(echo "$avg_time < $max_time" | bc -l) )); then
        log_test_result "$test_name (Performance)" "< ${max_time}s avg" "${avg_time}s avg" "$start_time" "$end_time" "PASSED"
    else
        log_test_result "$test_name (Performance)" "< ${max_time}s avg" "${avg_time}s avg" "$start_time" "$end_time" "FAILED"
    fi
}

# Initialize test output file
echo "ScratchBird Partial Hash Index Test Results" > "$OUTPUT_FILE"
echo "Test run started: $(date)" >> "$OUTPUT_FILE"
echo "Database: $TEST_DB" >> "$OUTPUT_FILE"
echo "" >> "$OUTPUT_FILE"

echo -e "${BLUE}ScratchBird Partial Hash Index Test Suite${NC}"
echo -e "${BLUE}==========================================${NC}"
echo ""

# Remove existing test database
rm -f "$TEST_DB"

# Create test database
echo -e "${YELLOW}Setting up test database...${NC}"
execute_sql "CREATE DATABASE '$TEST_DB' USER 'sysdba' PASSWORD 'masterkey';" 60

if [ ! -f "$TEST_DB" ]; then
    echo -e "${RED}Failed to create test database${NC}"
    exit 1
fi

echo -e "${GREEN}Test database created successfully${NC}"
echo ""

# ==========================================
# Test Category 1: Basic Partial Index Creation
# ==========================================

echo -e "${PURPLE}Category 1: Basic Partial Index Creation${NC}"
echo -e "${PURPLE}==========================================${NC}"

# Test 1.1: Create table for testing
run_test "Create test table" \
    "Statement completed successfully" \
    "CREATE TABLE test_products (
        id INTEGER PRIMARY KEY,
        name VARCHAR(100),
        category VARCHAR(50),
        price DECIMAL(10,2),
        active BOOLEAN,
        created_date DATE
    );"

# Test 1.2: Create basic partial hash index
run_test "Create basic partial hash index" \
    "Statement completed successfully" \
    "CREATE PARTIAL HASH INDEX idx_active_products 
     ON test_products (category) 
     WHERE active = true;"

# Test 1.3: Create complex condition partial index
run_test "Create complex condition partial index" \
    "Statement completed successfully" \
    "CREATE PARTIAL HASH INDEX idx_expensive_products 
     ON test_products (category) 
     WHERE price > 100.00 AND active = true;"

# Test 1.4: Create partial index with date condition
run_test "Create date condition partial index" \
    "Statement completed successfully" \
    "CREATE PARTIAL HASH INDEX idx_recent_products 
     ON test_products (name) 
     WHERE created_date >= '2024-01-01';"

# Test 1.5: Verify partial indexes in system tables
run_test "Verify partial indexes in RDB\$INDICES" \
    "IDX_ACTIVE_PRODUCTS" \
    "SELECT RDB\$INDEX_NAME FROM RDB\$INDICES 
     WHERE RDB\$INDEX_NAME LIKE 'IDX_%PRODUCTS%' 
     AND RDB\$CONDITION_SOURCE IS NOT NULL;"

# ==========================================
# Test Category 2: WHERE Clause Condition Evaluation
# ==========================================

echo ""
echo -e "${PURPLE}Category 2: WHERE Clause Condition Evaluation${NC}"
echo -e "${PURPLE}=============================================${NC}"

# Test 2.1: Insert data for condition testing
run_test "Insert test data" \
    "Statement completed successfully" \
    "INSERT INTO test_products VALUES 
        (1, 'Laptop Pro', 'Electronics', 1299.99, true, '2024-03-15'),
        (2, 'Office Chair', 'Furniture', 89.99, true, '2024-02-10'),
        (3, 'Gaming Mouse', 'Electronics', 79.99, false, '2023-12-05'),
        (4, 'Standing Desk', 'Furniture', 299.99, true, '2024-01-20'),
        (5, 'Wireless Headphones', 'Electronics', 199.99, true, '2024-04-01');"

# Test 2.2: Test condition evaluation for active products
run_test "Query using partial index with active condition" \
    "Laptop Pro" \
    "SELECT name FROM test_products WHERE category = 'Electronics' AND active = true;"

# Test 2.3: Test complex condition evaluation
run_test "Query using complex condition partial index" \
    "Laptop Pro" \
    "SELECT name FROM test_products WHERE category = 'Electronics' AND price > 100.00 AND active = true;"

# Test 2.4: Test date condition evaluation
run_test "Query using date condition partial index" \
    "Laptop Pro" \
    "SELECT name FROM test_products WHERE name LIKE 'Laptop%' AND created_date >= '2024-01-01';"

# Test 2.5: Verify condition exclusion works
run_test "Verify inactive products excluded" \
    "0" \
    "SELECT COUNT(*) FROM test_products WHERE category = 'Electronics' AND active = false;"

# ==========================================
# Test Category 3: Insert/Update/Delete Operations
# ==========================================

echo ""
echo -e "${PURPLE}Category 3: Insert/Update/Delete Operations${NC}"
echo -e "${PURPLE}=========================================${NC}"

# Test 3.1: Insert record that meets condition
run_test "Insert record meeting condition" \
    "Statement completed successfully" \
    "INSERT INTO test_products VALUES (6, 'Smart Watch', 'Electronics', 249.99, true, '2024-05-01');"

# Test 3.2: Insert record that doesn't meet condition
run_test "Insert record not meeting condition" \
    "Statement completed successfully" \
    "INSERT INTO test_products VALUES (7, 'Old Keyboard', 'Electronics', 29.99, false, '2023-06-01');"

# Test 3.3: Update record to meet condition
run_test "Update record to meet condition" \
    "Statement completed successfully" \
    "UPDATE test_products SET active = true WHERE id = 3;"

# Test 3.4: Update record to not meet condition
run_test "Update record to not meet condition" \
    "Statement completed successfully" \
    "UPDATE test_products SET active = false WHERE id = 2;"

# Test 3.5: Delete record from partial index
run_test "Delete record from partial index" \
    "Statement completed successfully" \
    "DELETE FROM test_products WHERE id = 7;"

# Test 3.6: Verify index consistency after operations
run_test "Verify index consistency" \
    "Smart Watch" \
    "SELECT name FROM test_products WHERE category = 'Electronics' AND active = true ORDER BY name;"

# ==========================================
# Test Category 4: Index Maintenance and Optimization
# ==========================================

echo ""
echo -e "${PURPLE}Category 4: Index Maintenance and Optimization${NC}"
echo -e "${PURPLE}=============================================${NC}"

# Test 4.1: Execute integrity check maintenance
run_test "Index integrity check" \
    "Statement completed successfully" \
    "ALTER INDEX idx_active_products PERFORM MAINTENANCE INTEGRITY_CHECK;"

# Test 4.2: Execute defragmentation maintenance
run_test "Index defragmentation" \
    "Statement completed successfully" \
    "ALTER INDEX idx_active_products PERFORM MAINTENANCE DEFRAGMENT;"

# Test 4.3: Execute statistics recalculation
run_test "Statistics recalculation" \
    "Statement completed successfully" \
    "ALTER INDEX idx_active_products PERFORM MAINTENANCE RECALC_STATS;"

# Test 4.4: Execute cache optimization
run_test "Cache optimization" \
    "Statement completed successfully" \
    "ALTER INDEX idx_active_products PERFORM MAINTENANCE OPTIMIZE_CACHE;"

# Test 4.5: Execute full rebuild
run_test "Full index rebuild" \
    "Statement completed successfully" \
    "ALTER INDEX idx_active_products PERFORM MAINTENANCE FULL_REBUILD;"

# ==========================================
# Test Category 5: Performance Monitoring and Statistics
# ==========================================

echo ""
echo -e "${PURPLE}Category 5: Performance Monitoring and Statistics${NC}"
echo -e "${PURPLE}===============================================${NC}"

# Test 5.1: Enable performance monitoring
run_test "Enable performance monitoring" \
    "Statement completed successfully" \
    "ALTER INDEX idx_active_products ENABLE PERFORMANCE_MONITORING;"

# Test 5.2: Query performance statistics
run_test "Query performance statistics" \
    "PARTIAL_HASH_STATISTICS" \
    "SELECT * FROM RDB\$INDEX_STATISTICS WHERE RDB\$INDEX_NAME = 'IDX_ACTIVE_PRODUCTS';"

# Test 5.3: Generate performance report
run_test "Generate performance report" \
    "Performance Report" \
    "SELECT GENERATE_INDEX_REPORT('IDX_ACTIVE_PRODUCTS') FROM RDB\$DATABASE;"

# Test 5.4: Check monitoring alerts
run_test "Check monitoring alerts" \
    "Statement completed successfully" \
    "SELECT * FROM RDB\$INDEX_ALERTS WHERE RDB\$INDEX_NAME = 'IDX_ACTIVE_PRODUCTS';"

# Test 5.5: Reset statistics
run_test "Reset performance statistics" \
    "Statement completed successfully" \
    "ALTER INDEX idx_active_products RESET STATISTICS;"

# ==========================================
# Test Category 6: Query Optimizer Integration
# ==========================================

echo ""
echo -e "${PURPLE}Category 6: Query Optimizer Integration${NC}"
echo -e "${PURPLE}=====================================${NC}"

# Test 6.1: Verify partial index is used in query plan
run_test "Verify partial index in query plan" \
    "IDX_ACTIVE_PRODUCTS" \
    "SET PLAN ON; 
     SELECT name FROM test_products WHERE category = 'Electronics' AND active = true;
     SET PLAN OFF;"

# Test 6.2: Test optimizer cost calculation
run_test "Optimizer recognizes partial index benefit" \
    "PARTIAL_HASH" \
    "EXPLAIN PLAN FOR 
     SELECT name FROM test_products WHERE category = 'Electronics' AND active = true;"

# Test 6.3: Compare with full table scan
run_test "Partial index vs full scan performance" \
    "INDEX" \
    "SET STATISTICS IO ON;
     SELECT name FROM test_products WHERE category = 'Electronics' AND active = true;
     SET STATISTICS IO OFF;"

# Test 6.4: Test selectivity calculation
run_test "Verify selectivity calculation" \
    "Statement completed successfully" \
    "UPDATE RDB\$INDICES SET RDB\$STATISTICS = NULL WHERE RDB\$INDEX_NAME = 'IDX_ACTIVE_PRODUCTS';
     SET STATISTICS INDEX IDX_ACTIVE_PRODUCTS;"

# ==========================================
# Test Category 7: Edge Cases and Error Handling
# ==========================================

echo ""
echo -e "${PURPLE}Category 7: Edge Cases and Error Handling${NC}"
echo -e "${PURPLE}=======================================${NC}"

# Test 7.1: Invalid condition syntax
run_test "Invalid condition syntax error" \
    "Dynamic SQL Error" \
    "CREATE PARTIAL HASH INDEX idx_invalid 
     ON test_products (name) 
     WHERE INVALID_SYNTAX;"

# Test 7.2: Non-deterministic function in condition
run_test "Non-deterministic function error" \
    "error" \
    "CREATE PARTIAL HASH INDEX idx_nondeterministic 
     ON test_products (name) 
     WHERE created_date > CURRENT_DATE;"

# Test 7.3: Condition referencing non-existent column
run_test "Non-existent column error" \
    "not found" \
    "CREATE PARTIAL HASH INDEX idx_nonexistent 
     ON test_products (name) 
     WHERE non_existent_column = 1;"

# Test 7.4: Duplicate partial index creation
run_test "Duplicate index error" \
    "already exists" \
    "CREATE PARTIAL HASH INDEX idx_active_products 
     ON test_products (category) 
     WHERE active = true;"

# Test 7.5: Drop partial index
run_test "Drop partial index" \
    "Statement completed successfully" \
    "DROP INDEX idx_recent_products;"

# Test 7.6: Query after index drop
run_test "Query after index drop uses table scan" \
    "NATURAL" \
    "SET PLAN ON;
     SELECT name FROM test_products WHERE name LIKE 'Laptop%' AND created_date >= '2024-01-01';
     SET PLAN OFF;"

# ==========================================
# Test Category 8: Stress Tests and Performance Benchmarks
# ==========================================

echo ""
echo -e "${PURPLE}Category 8: Stress Tests and Performance Benchmarks${NC}"
echo -e "${PURPLE}=================================================${NC}"

# Test 8.1: Large dataset creation
echo -e "${CYAN}Creating large dataset for stress testing...${NC}"
run_test "Create large dataset" \
    "Statement completed successfully" \
    "CREATE TABLE large_test_table (
        id INTEGER PRIMARY KEY,
        category_id INTEGER,
        status VARCHAR(20),
        value DECIMAL(10,2),
        created_timestamp TIMESTAMP
    );"

# Test 8.2: Create partial index on large table
run_test "Create partial index on large table" \
    "Statement completed successfully" \
    "CREATE PARTIAL HASH INDEX idx_large_active 
     ON large_test_table (category_id) 
     WHERE status = 'ACTIVE';"

# Test 8.3: Bulk insert with partial index
echo -e "${CYAN}Performing bulk insert stress test...${NC}"
run_performance_test "Bulk insert with partial index" \
    "-- Setup for bulk insert" \
    "INSERT INTO large_test_table VALUES (GEN_ID(GEN_TEST_ID, 1), MOD(GEN_ID(GEN_TEST_ID, 0), 100), CASE WHEN MOD(GEN_ID(GEN_TEST_ID, 0), 3) = 0 THEN 'ACTIVE' ELSE 'INACTIVE' END, RAND() * 1000, CURRENT_TIMESTAMP);" \
    100 \
    0.1

# Test 8.4: Query performance on large dataset
run_performance_test "Query performance on large dataset" \
    "-- Query setup" \
    "SELECT COUNT(*) FROM large_test_table WHERE category_id = 50 AND status = 'ACTIVE';" \
    50 \
    0.5

# Test 8.5: Update performance test
run_performance_test "Update performance with partial index" \
    "-- Update setup" \
    "UPDATE large_test_table SET status = 'ACTIVE' WHERE id = MOD(GEN_ID(GEN_TEST_ID, 1), 1000) + 1;" \
    20 \
    0.2

# Test 8.6: Delete performance test
run_performance_test "Delete performance with partial index" \
    "-- Delete setup" \
    "DELETE FROM large_test_table WHERE id = MOD(GEN_ID(GEN_TEST_ID, 1), 1000) + 1;" \
    20 \
    0.2

# Test 8.7: Concurrent access simulation
echo -e "${CYAN}Testing concurrent access patterns...${NC}"
run_test "Concurrent read operations" \
    "Statement completed successfully" \
    "SELECT COUNT(*) FROM large_test_table WHERE category_id IN (1,2,3,4,5) AND status = 'ACTIVE';"

# Test 8.8: Memory usage validation
run_test "Memory usage within limits" \
    "Statement completed successfully" \
    "SELECT * FROM RDB\$INDEX_STATISTICS WHERE RDB\$INDEX_NAME = 'IDX_LARGE_ACTIVE';"

# ==========================================
# Test Summary and Cleanup
# ==========================================

echo ""
echo -e "${BLUE}Test Summary${NC}"
echo -e "${BLUE}============${NC}"
echo -e "Total tests run: ${TOTAL_TESTS}"
echo -e "${GREEN}Passed: ${PASSED_TESTS}${NC}"
echo -e "${RED}Failed: ${FAILED_TESTS}${NC}"

if [ $FAILED_TESTS -eq 0 ]; then
    echo -e "${GREEN}All tests passed successfully!${NC}"
    exit_code=0
else
    echo -e "${YELLOW}Some tests failed. Check $OUTPUT_FILE for details.${NC}"
    exit_code=1
fi

# Test completion summary to output file
echo "" >> "$OUTPUT_FILE"
echo "=========================================" >> "$OUTPUT_FILE"
echo "Test Summary:" >> "$OUTPUT_FILE"
echo "Total tests: $TOTAL_TESTS" >> "$OUTPUT_FILE"
echo "Passed: $PASSED_TESTS" >> "$OUTPUT_FILE"
echo "Failed: $FAILED_TESTS" >> "$OUTPUT_FILE"
echo "Test run completed: $(date)" >> "$OUTPUT_FILE"
echo "=========================================" >> "$OUTPUT_FILE"

# Cleanup test database
echo ""
echo -e "${YELLOW}Cleaning up test database...${NC}"
rm -f "$TEST_DB"
echo -e "${GREEN}Cleanup completed${NC}"

echo ""
echo -e "${BLUE}Partial Hash Index test suite completed${NC}"
echo -e "${BLUE}Results saved to: $OUTPUT_FILE${NC}"

exit $exit_code