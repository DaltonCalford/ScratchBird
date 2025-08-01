#!/bin/bash
#
# ScratchBird v0.6.0 Hierarchical Schema Runtime Test Runner
# 
# This script runs comprehensive tests of the hierarchical schema functionality
# to verify that the implementation works correctly at runtime.
#

set -e  # Exit on any error

# Configuration
SCRATCHBIRD_HOME="/home/dcalford/Documents/claude/GitHubRepo/ScratchBird/gen/Release/scratchbird"
TEST_DB="/tmp/scratchbird_schema_test.fdb"
ISQL_BIN="$SCRATCHBIRD_HOME/bin/sb_isql"
LOG_FILE="/tmp/schema_test_results.log"

echo "=== ScratchBird v0.6.0 Hierarchical Schema Runtime Tests ==="
echo "Test Database: $TEST_DB"
echo "Log File: $LOG_FILE"
echo "ISQL Binary: $ISQL_BIN"
echo

# Check if ScratchBird tools are available
if [ ! -f "$ISQL_BIN" ]; then
    echo "❌ ERROR: ISQL tool not found at $ISQL_BIN"
    echo "Please ensure ScratchBird is built with 'make TARGET=Release sb_isql'"
    exit 1
fi

echo "✅ ScratchBird tools found"

# Set environment
export SCRATCHBIRD="$SCRATCHBIRD_HOME"
export LD_LIBRARY_PATH="$SCRATCHBIRD_HOME/lib:$LD_LIBRARY_PATH"

# Clean up any existing test database
if [ -f "$TEST_DB" ]; then
    echo "🧹 Cleaning up existing test database..."
    rm -f "$TEST_DB"
fi

echo "📊 Starting hierarchical schema runtime tests..."

# Initialize log file
cat > "$LOG_FILE" << 'EOF'
ScratchBird v0.6.0 Hierarchical Schema Runtime Test Results
============================================================

EOF

# Function to run test and capture results
run_test() {
    local test_name="$1"
    local sql_command="$2"
    
    echo "Running: $test_name"
    echo "Test: $test_name" >> "$LOG_FILE"
    echo "SQL: $sql_command" >> "$LOG_FILE"
    
    # Execute SQL and capture output
    if echo "$sql_command" | "$ISQL_BIN" -q "$TEST_DB" >> "$LOG_FILE" 2>&1; then
        echo "  ✅ PASSED" | tee -a "$LOG_FILE"
    else
        echo "  ❌ FAILED" | tee -a "$LOG_FILE"
        return 1
    fi
    
    echo "" >> "$LOG_FILE"
}

# Test execution begins
echo "" | tee -a "$LOG_FILE"

# Test 1: Database Creation
echo "🗄️  Test 1: Database Creation and Bootstrap Verification"
if "$ISQL_BIN" -q << EOF >> "$LOG_FILE" 2>&1
CREATE DATABASE '$TEST_DB';
CONNECT '$TEST_DB';
SELECT 'Database created successfully' FROM RDB\$DATABASE;
QUIT;
EOF
then
    echo "  ✅ Database creation PASSED"
else
    echo "  ❌ Database creation FAILED - Critical error!"
    echo "Server may have segmentation fault issue"
    exit 1
fi

# Test 2: System Schema Verification
echo "🔍 Test 2: SYSTEM Schema Bootstrap Verification"
run_test "SYSTEM schema check" "CONNECT '$TEST_DB'; SELECT RDB\$SCHEMA_NAME, RDB\$SCHEMA_PATH FROM RDB\$SCHEMAS WHERE RDB\$SCHEMA_NAME = 'SYSTEM';"

# Test 3: Basic Schema Creation
echo "📁 Test 3: Basic Schema Creation"
run_test "Create root schema" "CONNECT '$TEST_DB'; CREATE SCHEMA finance;"

# Test 4: Nested Schema Creation  
echo "📂 Test 4: Nested Schema Creation"
run_test "Create nested schema" "CONNECT '$TEST_DB'; CREATE SCHEMA finance.accounting;"

# Test 5: Deep Nested Schema
echo "📁📂📁 Test 5: Deep Nested Schema Creation"
run_test "Create deep nested schema" "CONNECT '$TEST_DB'; CREATE SCHEMA finance.accounting.reports;"

# Test 6: Schema Hierarchy Verification
echo "🔗 Test 6: Schema Hierarchy Verification"
run_test "Verify hierarchy" "CONNECT '$TEST_DB'; SELECT RDB\$SCHEMA_NAME, RDB\$SCHEMA_PATH, RDB\$SCHEMA_LEVEL FROM RDB\$SCHEMAS WHERE RDB\$SCHEMA_PATH LIKE 'finance%' ORDER BY RDB\$SCHEMA_LEVEL;"

# Test 7: Table Creation in Nested Schema
echo "🗃️  Test 7: Table Creation in Nested Schema"
run_test "Create table in nested schema" "CONNECT '$TEST_DB'; CREATE TABLE finance.accounting.reports.test_table (id INTEGER, name VARCHAR(50));"

# Test 8: Data Operations
echo "💾 Test 8: Data Operations in Hierarchical Schema"
run_test "Insert and query data" "CONNECT '$TEST_DB'; INSERT INTO finance.accounting.reports.test_table VALUES (1, 'Test Record'); SELECT * FROM finance.accounting.reports.test_table;"

# Cleanup Tests
echo "🧹 Test 9: Schema Cleanup"
run_test "Drop table" "CONNECT '$TEST_DB'; DROP TABLE finance.accounting.reports.test_table;"
run_test "Drop schemas" "CONNECT '$TEST_DB'; DROP SCHEMA finance.accounting.reports; DROP SCHEMA finance.accounting; DROP SCHEMA finance;"

# Final Results
echo "" | tee -a "$LOG_FILE"
echo "=== TEST SUMMARY ===" | tee -a "$LOG_FILE"

# Count results
total_tests=$(grep -c "✅ PASSED\|❌ FAILED" "$LOG_FILE" || echo "0")
passed_tests=$(grep -c "✅ PASSED" "$LOG_FILE" || echo "0")
failed_tests=$(grep -c "❌ FAILED" "$LOG_FILE" || echo "0")

echo "Total Tests: $total_tests" | tee -a "$LOG_FILE"
echo "Passed: $passed_tests" | tee -a "$LOG_FILE"  
echo "Failed: $failed_tests" | tee -a "$LOG_FILE"

if [ "$failed_tests" -eq 0 ]; then
    echo "🎉 ALL HIERARCHICAL SCHEMA TESTS PASSED!" | tee -a "$LOG_FILE"
    echo "✅ Runtime verification: SUCCESSFUL"
    echo "✅ Hierarchical schema implementation: FUNCTIONAL"
else
    echo "❌ Some tests failed. Check $LOG_FILE for details." | tee -a "$LOG_FILE"
fi

# Cleanup test database
if [ -f "$TEST_DB" ]; then
    rm -f "$TEST_DB"
    echo "🧹 Test database cleaned up"
fi

echo ""
echo "📋 Full test results saved to: $LOG_FILE"
echo "🔍 Review the log file for detailed test output"