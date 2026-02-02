#!/bin/bash
# ScratchBird
# Copyright (c) 2025-2026 Dalton Calford
#
# Licensed under the Initial Developer's Public License Version 1.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at:
# https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/

# MGA Compliance Verification Script
# Verifies ScratchBird codebase is 100% Firebird MGA compliant

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

cd "$PROJECT_ROOT"

echo "=================================================="
echo "  MGA Compliance Verification"
echo "  ScratchBird Database Engine"
echo "=================================================="
echo ""

ERRORS=0
WARNINGS=0

# Check 1: No Snapshot structures
echo "✓ Checking for Snapshot structures..."
SNAPSHOT_STRUCTS=$(grep -r "struct Snapshot" --include="*.h" include/ src/ 2>/dev/null | grep -v "// OLD:" | wc -l)
if [ "$SNAPSHOT_STRUCTS" -gt 0 ]; then
    echo "  ❌ FAIL: Found $SNAPSHOT_STRUCTS Snapshot structure(s)"
    grep -rn "struct Snapshot" --include="*.h" include/ src/ 2>/dev/null | grep -v "// OLD:"
    ERRORS=$((ERRORS + 1))
else
    echo "  ✅ PASS: No Snapshot structures found"
fi
echo ""

# Check 2: No Snapshot* parameters
echo "✓ Checking for Snapshot* parameters..."
SNAPSHOT_PARAMS=$(grep -r "Snapshot\s*\*" --include="*.h" --include="*.cpp" include/ src/ 2>/dev/null | grep -v "// OLD:" | grep -v "^Binary" | wc -l)
if [ "$SNAPSHOT_PARAMS" -gt 0 ]; then
    echo "  ❌ FAIL: Found $SNAPSHOT_PARAMS Snapshot* parameter(s)"
    grep -rn "Snapshot\s*\*" --include="*.h" --include="*.cpp" include/ src/ 2>/dev/null | grep -v "// OLD:" | head -20
    ERRORS=$((ERRORS + 1))
else
    echo "  ✅ PASS: No Snapshot* parameters found"
fi
echo ""

# Check 3: No isSnapshotVisible() calls
echo "✓ Checking for isSnapshotVisible() calls..."
SNAPSHOT_VISIBLE=$(grep -r "isSnapshotVisible" --include="*.cpp" src/ 2>/dev/null | grep -v "// OLD:" | wc -l)
if [ "$SNAPSHOT_VISIBLE" -gt 0 ]; then
    echo "  ❌ FAIL: Found $SNAPSHOT_VISIBLE isSnapshotVisible() call(s)"
    grep -rn "isSnapshotVisible" --include="*.cpp" src/ 2>/dev/null | grep -v "// OLD:" | head -10
    ERRORS=$((ERRORS + 1))
else
    echo "  ✅ PASS: No isSnapshotVisible() calls found"
fi
echo ""

# Check 4: No getSnapshot() calls
echo "✓ Checking for getSnapshot() calls..."
GET_SNAPSHOT=$(grep -r "getSnapshot" --include="*.cpp" src/ 2>/dev/null | grep -v "// OLD:" | grep -v "getSnapshotIsolation" | wc -l)
if [ "$GET_SNAPSHOT" -gt 0 ]; then
    echo "  ❌ FAIL: Found $GET_SNAPSHOT getSnapshot() call(s)"
    grep -rn "getSnapshot" --include="*.cpp" src/ 2>/dev/null | grep -v "// OLD:" | grep -v "getSnapshotIsolation" | head -10
    ERRORS=$((ERRORS + 1))
else
    echo "  ✅ PASS: No getSnapshot() calls found"
fi
echo ""

# Check 5: TIP usage (getTransactionState)
echo "✓ Checking for TIP usage (getTransactionState)..."
TIP_USAGE=$(grep -r "getTransactionState" --include="*.cpp" src/ 2>/dev/null | wc -l)
if [ "$TIP_USAGE" -lt 5 ]; then
    echo "  ⚠️  WARNING: Only $TIP_USAGE getTransactionState() call(s) found"
    echo "  Expected at least 5 (one per major component)"
    WARNINGS=$((WARNINGS + 1))
else
    echo "  ✅ PASS: TIP usage confirmed ($TIP_USAGE occurrences)"
fi
echo ""

# Check 6: isVersionVisible() usage
echo "✓ Checking for isVersionVisible() usage..."
VERSION_VISIBLE=$(grep -r "isVersionVisible" --include="*.cpp" src/ 2>/dev/null | wc -l)
if [ "$VERSION_VISIBLE" -lt 1 ]; then
    echo "  ❌ FAIL: No isVersionVisible() calls found"
    echo "  Expected at least 1 (should replace isSnapshotVisible)"
    ERRORS=$((ERRORS + 1))
else
    echo "  ✅ PASS: isVersionVisible() usage confirmed ($VERSION_VISIBLE occurrences)"
fi
echo ""

# Check 7: No MVCC references in comments (informational)
echo "✓ Checking for MVCC references in new code..."
MVCC_REFS=$(grep -r "MVCC" --include="*.cpp" --include="*.h" src/ include/ 2>/dev/null | grep -v "// OLD:" | grep -v "PostgreSQL MVCC" | grep -v "not MVCC" | wc -l)
if [ "$MVCC_REFS" -gt 20 ]; then
    echo "  ⚠️  INFO: Found $MVCC_REFS references to 'MVCC' in code"
    echo "  Consider updating comments to reference 'Firebird MGA' instead"
    WARNINGS=$((WARNINGS + 1))
else
    echo "  ✅ INFO: Minimal MVCC references found ($MVCC_REFS)"
fi
echo ""

# Summary
echo "=================================================="
echo "  Verification Summary"
echo "=================================================="
echo ""

if [ "$ERRORS" -eq 0 ] && [ "$WARNINGS" -eq 0 ]; then
    echo "✅ Result: 100% FIREBIRD MGA COMPLIANT"
    echo ""
    echo "All checks passed!"
    echo "- No PostgreSQL MVCC contamination detected"
    echo "- TIP-based visibility confirmed"
    echo "- isVersionVisible() usage confirmed"
    echo ""
    exit 0
elif [ "$ERRORS" -eq 0 ]; then
    echo "⚠️  Result: MGA COMPLIANT (with warnings)"
    echo ""
    echo "All critical checks passed, but there are $WARNINGS warning(s)."
    echo "Review warnings above for potential improvements."
    echo ""
    exit 0
else
    echo "❌ Result: NOT MGA COMPLIANT"
    echo ""
    echo "Found $ERRORS error(s) and $WARNINGS warning(s)."
    echo ""
    echo "ERRORS FOUND:"
    echo "- PostgreSQL MVCC patterns still present"
    echo "- Must fix before claiming MGA compliance"
    echo ""
    echo "Refer to:"
    echo "- /docs/planning/MGA_COMPLIANCE_FIX_PLAN.md"
    echo "- /docs/audit/01_MGA_COMPLIANCE_AUDIT.md"
    echo "- /MGA_RULES.md"
    echo ""
    exit 1
fi
