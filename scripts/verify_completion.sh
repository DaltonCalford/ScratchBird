#!/bin/bash
# Completion Verification Script
# Run BEFORE marking any task complete
# Usage: ./scripts/verify_completion.sh <feature_area>

set -e

FEATURE=$1
if [ -z "$FEATURE" ]; then
    echo "Usage: $0 <feature_area>"
    echo "Example: $0 dependency_tracking"
    exit 1
fi

echo "======================================================================="
echo "Completion Verification for: $FEATURE"
echo "======================================================================="
echo ""

FAILED=0
WARNINGS=0

# 1. TODO Check
echo "1. TODO CHECK"
echo "----------------------------------------------------------------------"
TODOS=$(grep -r "TODO" src/ include/ 2>/dev/null | grep -i "$FEATURE" || true)
TODO_COUNT=$(echo "$TODOS" | grep -c "TODO" || echo "0")

if [ "$TODO_COUNT" -gt 0 ]; then
    echo "❌ FAILED: Found $TODO_COUNT related TODO(s)"
    echo ""
    echo "$TODOS"
    echo ""
    echo "   Action required: Implement or document each TODO before marking complete"
    FAILED=$((FAILED + 1))
else
    echo "✓ PASSED: No related TODOs found"
fi
echo ""

# 2. Test File Check
echo "2. TEST FILE CHECK"
echo "----------------------------------------------------------------------"
TEST_FILE=$(find tests/unit -name "*${FEATURE}*.cpp" 2>/dev/null | head -1 || true)

if [ -z "$TEST_FILE" ]; then
    echo "❌ FAILED: No test file found for '$FEATURE'"
    echo "   Expected: tests/unit/test_${FEATURE}.cpp"
    FAILED=$((FAILED + 1))
else
    echo "✓ Found test file: $TEST_FILE"

    # 2a. Check for restart test
    echo ""
    echo "2a. Restart/Persistence Test Check"
    if grep -q "Restart\|restart\|Persistence\|persistence" "$TEST_FILE"; then
        RESTART_TESTS=$(grep -n "TEST.*Restart\|TEST.*Persistence" "$TEST_FILE" || true)
        if [ -n "$RESTART_TESTS" ]; then
            echo "✓ PASSED: Restart/persistence test(s) found:"
            echo "$RESTART_TESTS"
        else
            echo "⚠️  WARNING: Keywords found but no TEST matching *Restart* or *Persistence*"
            WARNINGS=$((WARNINGS + 1))
        fi
    else
        echo "❌ FAILED: No restart/persistence test found"
        echo "   MANDATORY: Must include test that closes and reopens database"
        FAILED=$((FAILED + 1))
    fi

    # 2b. Check for negative tests
    echo ""
    echo "2b. Negative Test Check"
    if grep -q "EXPECT_EQ.*NOT_FOUND\|EXPECT_EQ.*CONSTRAINT_VIOLATION\|EXPECT_EQ.*INVALID_ARGUMENT" "$TEST_FILE"; then
        NEG_TESTS=$(grep -n "EXPECT_EQ.*NOT_FOUND\|EXPECT_EQ.*CONSTRAINT_VIOLATION\|EXPECT_EQ.*INVALID_ARGUMENT" "$TEST_FILE" | head -3 || true)
        echo "✓ PASSED: Negative test(s) found:"
        echo "$NEG_TESTS"
    else
        echo "❌ FAILED: No negative tests found"
        echo "   MANDATORY: Must test NOT_FOUND, CONSTRAINT_VIOLATION, or INVALID_ARGUMENT"
        FAILED=$((FAILED + 1))
    fi

    # 2c. Check for multi-path tests
    echo ""
    echo "2c. Multi-Path Test Check"
    if grep -q "catalog->" "$TEST_FILE"; then
        echo "✓ PASSED: Direct CatalogManager API calls found"
    else
        echo "⚠️  WARNING: No direct CatalogManager API calls found"
        echo "   Verify feature tested through multiple paths (SQL, API, executor)"
        WARNINGS=$((WARNINGS + 1))
    fi
fi
echo ""

# 3. Catalog Schema Check
echo "3. CATALOG SCHEMA CHECK"
echo "----------------------------------------------------------------------"
if grep -q "sb_${FEATURE}\|sb_.*${FEATURE}" src/core/catalog_manager.cpp; then
    CATALOG_REFS=$(grep -n "sb_${FEATURE}\|sb_.*${FEATURE}" src/core/catalog_manager.cpp | head -5)
    echo "✓ PASSED: Catalog references found:"
    echo "$CATALOG_REFS"
else
    echo "⚠️  WARNING: No catalog table found for '$FEATURE'"
    echo "   This may be acceptable if feature doesn't require catalog storage"
    WARNINGS=$((WARNINGS + 1))
fi
echo ""

# 4. Implementation Check
echo "4. IMPLEMENTATION CHECK"
echo "----------------------------------------------------------------------"
echo "Checking for implementation in key files..."

# Check catalog_manager.cpp
if grep -q "$FEATURE" src/core/catalog_manager.cpp; then
    echo "✓ Found implementation in catalog_manager.cpp"
else
    echo "⚠️  No implementation found in catalog_manager.cpp"
fi

# Check executor.cpp
if grep -q "$FEATURE" src/sblr/executor.cpp; then
    echo "✓ Found implementation in executor.cpp"
else
    echo "⚠️  No implementation found in executor.cpp"
fi
echo ""

# 5. Build Status Check
echo "5. BUILD STATUS CHECK"
echo "----------------------------------------------------------------------"
if [ -d "build" ]; then
    echo "Build directory exists"
    if [ -f "build/tests/scratchbird_tests" ]; then
        echo "✓ Test binary exists"
        echo ""
        echo "MANUAL ACTION REQUIRED:"
        echo "Run tests and provide output:"
        echo "  ./build/tests/scratchbird_tests --gtest_filter=\"*${FEATURE}*\""
    else
        echo "⚠️  WARNING: Test binary not found"
        echo "   Run: cd build && make"
    fi
else
    echo "⚠️  WARNING: Build directory not found"
    echo "   Run: mkdir build && cd build && cmake .. && make"
fi
echo ""

# Summary
echo "======================================================================="
echo "COMPLETION VERIFICATION SUMMARY"
echo "======================================================================="
echo "Feature: $FEATURE"
echo "Failed checks: $FAILED"
echo "Warnings: $WARNINGS"
echo ""

if [ $FAILED -gt 0 ]; then
    echo "❌ FAILED: Completion verification failed with $FAILED critical error(s)"
    echo ""
    echo "DO NOT MARK TASK COMPLETE"
    echo ""
    echo "Required actions:"
    echo "1. Fix all failed checks above"
    echo "2. Re-run this script"
    echo "3. Provide all required evidence to user"
    echo "4. Get user approval"
    echo ""
    echo "See: /COMPLETION_VERIFICATION_CHECKLIST.md for full requirements"
    exit 1
elif [ $WARNINGS -gt 0 ]; then
    echo "⚠️  PASSED WITH WARNINGS: $WARNINGS warning(s) found"
    echo ""
    echo "MANUAL VERIFICATION REQUIRED:"
    echo "1. Review all warnings above"
    echo "2. Run all tests and capture output"
    echo "3. Run restart test specifically and capture output"
    echo "4. Provide catalog query showing persisted data"
    echo "5. Provide grep output showing no TODOs (or explain each)"
    echo "6. Get USER APPROVAL before marking complete"
    echo ""
    echo "Required evidence (see /COMPLETION_VERIFICATION_CHECKLIST.md):"
    echo "  - File paths to all changes"
    echo "  - Test output (ALL test types passing)"
    echo "  - Restart test output"
    echo "  - Catalog query output"
    echo "  - Negative test output"
    echo "  - TODO verification output"
else
    echo "✓ PASSED: Basic verification completed successfully"
    echo ""
    echo "MANUAL VERIFICATION STILL REQUIRED:"
    echo "1. Run all tests and capture output:"
    echo "     ./build/tests/scratchbird_tests --gtest_filter=\"*${FEATURE}*\""
    echo "2. Run restart test and verify data persists:"
    echo "     ./build/tests/scratchbird_tests --gtest_filter=\"*${FEATURE}*Restart*\""
    echo "3. Provide catalog query showing data is stored"
    echo "4. Provide all evidence per /COMPLETION_VERIFICATION_CHECKLIST.md"
    echo "5. Get USER APPROVAL before marking complete"
fi
echo ""
echo "Next: Review /COMPLETION_VERIFICATION_CHECKLIST.md for full checklist"
echo ""
