#!/bin/bash

# 18_migrated_monitoring_admin.sh
# ScratchBird Consolidated Test Suite - Migrated from Firebird
# 
# Category: monitoring_admin
# Individual Tests: 6
# Revolutionary Features: 2496 demonstrations

set -e

# Source centralized test configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/test_config.sh"

# Master test configuration
TEST_SUITE="18_migrated_monitoring_admin"
TEST_CATEGORY="monitoring_admin"
SUITE_LOG="$SB_TEST_RESULTS_DIR/${TEST_SUITE}_suite.log"

echo "=== SCRATCHBIRD MIGRATED TEST SUITE ==="
echo "Suite: $TEST_SUITE"
echo "Category: $TEST_CATEGORY" 
echo "Individual Tests: 6"
echo "Revolutionary Features: 2496"
echo "Date: $(date)"
echo

# Initialize suite log
cat > "$SUITE_LOG" << SUITE_EOF
=================================================================
SCRATCHBIRD MIGRATED TEST SUITE: monitoring_admin
=================================================================
Suite: $TEST_SUITE
Individual Tests: 6
Revolutionary Features Demonstrated: 2496
Execution Date: $(date)

INDIVIDUAL TEST RESULTS:
========================
SUITE_EOF

# Execute all individual tests
suite_passed=0
suite_failed=0
suite_total=0

# Execute: 01_monitoring_admin_functional_monitoring_01
echo "🧪 Executing: 01_monitoring_admin_functional_monitoring_01"
if bash "temp_monitoring_admin/01_monitoring_admin_functional_monitoring_01.sh" >> "$SUITE_LOG" 2>&1; then
    echo "✅ PASSED: 01_monitoring_admin_functional_monitoring_01"
    echo "PASSED: 01_monitoring_admin_functional_monitoring_01" >> "$SUITE_LOG"
    ((suite_passed++))
else
    echo "❌ FAILED: 01_monitoring_admin_functional_monitoring_01"
    echo "FAILED: 01_monitoring_admin_functional_monitoring_01" >> "$SUITE_LOG"
    ((suite_failed++))
fi
((suite_total++))

# Execute: 02_monitoring_admin_functional_monitoring_02
echo "🧪 Executing: 02_monitoring_admin_functional_monitoring_02"
if bash "temp_monitoring_admin/02_monitoring_admin_functional_monitoring_02.sh" >> "$SUITE_LOG" 2>&1; then
    echo "✅ PASSED: 02_monitoring_admin_functional_monitoring_02"
    echo "PASSED: 02_monitoring_admin_functional_monitoring_02" >> "$SUITE_LOG"
    ((suite_passed++))
else
    echo "❌ FAILED: 02_monitoring_admin_functional_monitoring_02"
    echo "FAILED: 02_monitoring_admin_functional_monitoring_02" >> "$SUITE_LOG"
    ((suite_failed++))
fi
((suite_total++))

# Execute: 03_monitoring_admin_functional_monitoring_03
echo "🧪 Executing: 03_monitoring_admin_functional_monitoring_03"
if bash "temp_monitoring_admin/03_monitoring_admin_functional_monitoring_03.sh" >> "$SUITE_LOG" 2>&1; then
    echo "✅ PASSED: 03_monitoring_admin_functional_monitoring_03"
    echo "PASSED: 03_monitoring_admin_functional_monitoring_03" >> "$SUITE_LOG"
    ((suite_passed++))
else
    echo "❌ FAILED: 03_monitoring_admin_functional_monitoring_03"
    echo "FAILED: 03_monitoring_admin_functional_monitoring_03" >> "$SUITE_LOG"
    ((suite_failed++))
fi
((suite_total++))

# Execute: 04_monitoring_admin_functional_monitoring_04
echo "🧪 Executing: 04_monitoring_admin_functional_monitoring_04"
if bash "temp_monitoring_admin/04_monitoring_admin_functional_monitoring_04.sh" >> "$SUITE_LOG" 2>&1; then
    echo "✅ PASSED: 04_monitoring_admin_functional_monitoring_04"
    echo "PASSED: 04_monitoring_admin_functional_monitoring_04" >> "$SUITE_LOG"
    ((suite_passed++))
else
    echo "❌ FAILED: 04_monitoring_admin_functional_monitoring_04"
    echo "FAILED: 04_monitoring_admin_functional_monitoring_04" >> "$SUITE_LOG"
    ((suite_failed++))
fi
((suite_total++))

# Execute: 05_monitoring_admin_functional_shadow_create_01
echo "🧪 Executing: 05_monitoring_admin_functional_shadow_create_01"
if bash "temp_monitoring_admin/05_monitoring_admin_functional_shadow_create_01.sh" >> "$SUITE_LOG" 2>&1; then
    echo "✅ PASSED: 05_monitoring_admin_functional_shadow_create_01"
    echo "PASSED: 05_monitoring_admin_functional_shadow_create_01" >> "$SUITE_LOG"
    ((suite_passed++))
else
    echo "❌ FAILED: 05_monitoring_admin_functional_shadow_create_01"
    echo "FAILED: 05_monitoring_admin_functional_shadow_create_01" >> "$SUITE_LOG"
    ((suite_failed++))
fi
((suite_total++))

# Execute: 06_monitoring_admin_functional_shadow_create_02
echo "🧪 Executing: 06_monitoring_admin_functional_shadow_create_02"
if bash "temp_monitoring_admin/06_monitoring_admin_functional_shadow_create_02.sh" >> "$SUITE_LOG" 2>&1; then
    echo "✅ PASSED: 06_monitoring_admin_functional_shadow_create_02"
    echo "PASSED: 06_monitoring_admin_functional_shadow_create_02" >> "$SUITE_LOG"
    ((suite_passed++))
else
    echo "❌ FAILED: 06_monitoring_admin_functional_shadow_create_02"
    echo "FAILED: 06_monitoring_admin_functional_shadow_create_02" >> "$SUITE_LOG"
    ((suite_failed++))
fi
((suite_total++))


# Suite summary
echo
echo "=== SUITE SUMMARY ==="
echo "Total Tests: $suite_total"
echo "Passed: $suite_passed"
echo "Failed: $suite_failed"
echo "Revolutionary Features: 2496"

# Log suite completion
cat >> "$SUITE_LOG" << SUITE_EOF

=================================================================
SUITE SUMMARY
=================================================================
Total Tests: $suite_total
Passed: $suite_passed  
Failed: $suite_failed
Success Rate: $(( suite_passed * 100 / suite_total ))%
Revolutionary Features Demonstrated: 2496

Category: monitoring_admin
Migration Status: COMPLETE
=================================================================
SUITE_EOF

if [ $suite_failed -eq 0 ]; then
    echo "🎉 Suite completed successfully!"
    log_test_execution "$TEST_SUITE" "PASSED" "All $suite_total tests passed"
    exit 0
else
    echo "⚠️  Suite completed with $suite_failed failures"
    log_test_execution "$TEST_SUITE" "FAILED" "$suite_failed of $suite_total tests failed"
    exit 1
fi
