#!/bin/bash
# ScratchBird
# Copyright (c) 2025-2026 Dalton Calford
#
# Licensed under the Initial Developer's Public License Version 1.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at:
# https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/

# Foundation Verification Script
# Run BEFORE starting any implementation work
# Usage: ./scripts/verify_foundation.sh <feature_area>

set -e

FEATURE=$1
if [ -z "$FEATURE" ]; then
    echo "Usage: $0 <feature_area>"
    echo "Example: $0 dependency_tracking"
    exit 1
fi

echo "======================================================================="
echo "Foundation Verification for: $FEATURE"
echo "======================================================================="
echo ""

WARNINGS=0
ERRORS=0

# 1. Catalog Infrastructure Check
echo "1. CATALOG INFRASTRUCTURE CHECK"
echo "----------------------------------------------------------------------"
echo "Searching for catalog tables related to '$FEATURE'..."
CATALOG_TABLES=$(grep -n "sb_" src/core/catalog_manager.cpp | grep -i "$FEATURE" | grep -E "CREATE TABLE|createTable" || true)
if [ -z "$CATALOG_TABLES" ]; then
    echo "⚠️  WARNING: No catalog tables found for '$FEATURE'"
    echo "   Action required: Verify catalog infrastructure exists or add to task list"
    WARNINGS=$((WARNINGS + 1))
else
    echo "✓ Found catalog table references:"
    echo "$CATALOG_TABLES" | head -5
fi
echo ""

# 2. Specification Check
echo "2. SPECIFICATION CHECK"
echo "----------------------------------------------------------------------"
echo "Searching for specifications mentioning '$FEATURE'..."
SPECS=$(find docs/specifications -name "*.md" 2>/dev/null | xargs grep -l -i "$FEATURE" 2>/dev/null || true)
if [ -z "$SPECS" ]; then
    echo "⚠️  WARNING: No specifications found mentioning '$FEATURE'"
    echo "   Action required: Identify relevant specifications and read them"
    WARNINGS=$((WARNINGS + 1))
else
    echo "✓ Found relevant specifications:"
    echo "$SPECS"
fi
echo ""

# 3. Integration Points Check
echo "3. INTEGRATION POINTS CHECK"
echo "----------------------------------------------------------------------"
echo "Checking for security/audit/resolver integration points..."

# Check for security context
SECURITY_REFS=$(grep -n "SecurityContext\|security_context" src/core/catalog_manager.cpp 2>/dev/null | head -3 || true)
if [ -n "$SECURITY_REFS" ]; then
    echo "✓ Security context integration found"
else
    echo "ℹ️  No security context integration (may not be required for $FEATURE)"
fi

# Check for audit logging
AUDIT_REFS=$(grep -n "AuditLog\|audit_logger" src/core/catalog_manager.cpp 2>/dev/null | head -3 || true)
if [ -n "$AUDIT_REFS" ]; then
    echo "✓ Audit logging integration found"
else
    echo "ℹ️  No audit logging integration (may not be required for $FEATURE)"
fi

# Check for UUID resolver
UUID_REFS=$(grep -n "UUID.*resolver\|resolveObject" src/core/catalog_manager.cpp 2>/dev/null | head -3 || true)
if [ -n "$UUID_REFS" ]; then
    echo "✓ UUID resolver integration found"
else
    echo "ℹ️  No UUID resolver integration (may not be required for $FEATURE)"
fi
echo ""

# 4. Existing Tests Check
echo "4. EXISTING TESTS CHECK"
echo "----------------------------------------------------------------------"
echo "Checking for existing test files..."
TEST_FILES=$(find tests/unit -name "*${FEATURE}*.cpp" 2>/dev/null || true)
if [ -z "$TEST_FILES" ]; then
    echo "ℹ️  No existing test file for '$FEATURE' (will need to create)"
else
    echo "✓ Found existing test file(s):"
    echo "$TEST_FILES"
fi
echo ""

# 5. TODO Scan
echo "5. TODO SCAN"
echo "----------------------------------------------------------------------"
echo "Checking for existing TODOs related to '$FEATURE'..."
TODOS=$(grep -r "TODO" src/ include/ 2>/dev/null | grep -i "$FEATURE" || true)
if [ -n "$TODOS" ]; then
    TODO_COUNT=$(echo "$TODOS" | wc -l)
    echo "⚠️  WARNING: Found $TODO_COUNT related TODO(s)"
    echo "   Review these TODOs before starting implementation:"
    echo "$TODOS" | head -10
    WARNINGS=$((WARNINGS + 1))
else
    echo "✓ No related TODOs found"
fi
echo ""

# 6. Dependency Check (audit findings)
echo "6. DEPENDENCY CHECK (Audit Findings)"
echo "----------------------------------------------------------------------"
echo "Checking if '$FEATURE' is mentioned in audit findings..."
if [ -f "docs/findings/engine_gap_report.md" ]; then
    GAP_REFS=$(grep -i "$FEATURE" docs/findings/engine_gap_report.md || true)
    if [ -n "$GAP_REFS" ]; then
        echo "⚠️  WARNING: Feature mentioned in gap report"
        echo "   Review gap report before implementing:"
        echo "$GAP_REFS" | head -5
        WARNINGS=$((WARNINGS + 1))
    else
        echo "✓ No mentions in gap report"
    fi
else
    echo "ℹ️  No gap report found"
fi
echo ""

# Summary
echo "======================================================================="
echo "FOUNDATION VERIFICATION SUMMARY"
echo "======================================================================="
echo "Feature: $FEATURE"
echo "Warnings: $WARNINGS"
echo "Errors: $ERRORS"
echo ""

if [ $ERRORS -gt 0 ]; then
    echo "❌ FAILED: Foundation verification failed with $ERRORS error(s)"
    echo "   DO NOT proceed with implementation"
    exit 1
elif [ $WARNINGS -gt 0 ]; then
    echo "⚠️  WARNINGS: Foundation verification completed with $WARNINGS warning(s)"
    echo ""
    echo "MANUAL VERIFICATION REQUIRED:"
    echo "1. Review all warnings above"
    echo "2. Verify catalog infrastructure exists or add to task list"
    echo "3. Read all relevant specifications"
    echo "4. Identify integration points (security, audit, resolver)"
    echo "5. Get USER APPROVAL on approach before proceeding"
    echo ""
    echo "See: /IMPLEMENTATION_STANDARDS.md for full requirements"
else
    echo "✓ PASSED: Foundation verification completed successfully"
    echo ""
    echo "NEXT STEPS:"
    echo "1. Read all relevant specifications"
    echo "2. Create test plan (see /IMPLEMENTATION_STANDARDS.md)"
    echo "3. Get USER APPROVAL on approach"
    echo "4. Proceed with implementation"
fi
echo ""
