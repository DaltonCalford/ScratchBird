#!/bin/bash
# Move verified-complete planning documents to implemented/ subdirectory
# Generated: 2025-10-24

BASE="/home/dcalford/CliWork/ScratchBird/docs/planning"

# Verify implemented/ directory exists
mkdir -p "$BASE/implemented"

# Move Sprint 0 (verified complete)
if [ -f "$BASE/SPRINT0_BUG_FIX_COMPLETE.md" ]; then
    mv "$BASE/SPRINT0_BUG_FIX_COMPLETE.md" "$BASE/implemented/"
    echo "✅ Moved SPRINT0_BUG_FIX_COMPLETE.md"
else
    echo "⚠️  SPRINT0_BUG_FIX_COMPLETE.md already in implemented/ or not found"
fi

# Move Sprint 1 (verified complete)
if [ -f "$BASE/SPRINT1_FOUNDATION_COMPLETE.md" ]; then
    mv "$BASE/SPRINT1_FOUNDATION_COMPLETE.md" "$BASE/implemented/"
    echo "✅ Moved SPRINT1_FOUNDATION_COMPLETE.md"
else
    echo "⚠️  SPRINT1_FOUNDATION_COMPLETE.md already in implemented/ or not found"
fi

# Move Sprint 3 Summary (design complete)
if [ -f "$BASE/SPRINT3_SUMMARY.md" ]; then
    mv "$BASE/SPRINT3_SUMMARY.md" "$BASE/implemented/"
    echo "✅ Moved SPRINT3_SUMMARY.md"
else
    echo "⚠️  SPRINT3_SUMMARY.md already in implemented/ or not found"
fi

# Move Sprint 3 Architecture (document complete)
if [ -f "$BASE/SPRINT3_ONLINE_MIGRATION_ARCHITECTURE.md" ]; then
    mv "$BASE/SPRINT3_ONLINE_MIGRATION_ARCHITECTURE.md" "$BASE/implemented/"
    echo "✅ Moved SPRINT3_ONLINE_MIGRATION_ARCHITECTURE.md"
else
    echo "⚠️  SPRINT3_ONLINE_MIGRATION_ARCHITECTURE.md already in implemented/ or not found"
fi

echo ""
echo "Reorganization complete!"
echo "Moved 4 verified-complete documents to implemented/"
echo "Remaining documents in planning/ need verification before moving"
