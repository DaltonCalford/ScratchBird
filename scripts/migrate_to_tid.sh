#!/bin/bash
# ScratchBird
# Copyright (c) 2025-2026 Dalton Calford
#
# Licensed under the Initial Developer's Public License Version 1.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at:
# https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/

# Script to migrate uint64_t tuple_id to TID struct across the codebase
# Phase 1.5: TID Migration to GPID-based Format
#
# This script performs systematic search-and-replace operations to migrate
# from legacy uint64_t tuple IDs to TID struct throughout the index layer.

set -e

echo "=== Phase 1.5: TID Migration Script ==="
echo "This script will migrate index APIs from uint64_t to TID struct"
echo ""

# Backup files before modification
echo "[1/10] Creating backups..."
mkdir -p backups/phase1.5_$(date +%Y%m%d_%H%M%S)
BACKUP_DIR="backups/phase1.5_$(date +%Y%m%d_%H%M%S)"
cp src/core/btree.cpp "$BACKUP_DIR/"
cp src/core/hash_index.cpp "$BACKUP_DIR/" 2>/dev/null || true
cp src/core/gin_index.cpp "$BACKUP_DIR/" 2>/dev/null || true
cp src/core/bitmap_index.cpp "$BACKUP_DIR/" 2>/dev/null || true
cp src/core/hnsw_index.cpp "$BACKUP_DIR/" 2>/dev/null || true
cp src/core/brin_index.cpp "$BACKUP_DIR/" 2>/dev/null || true

echo "Backups created in $BACKUP_DIR"

# Function to migrate a single implementation file
migrate_impl_file() {
    local file=$1
    echo "[Migrating] $file..."

    # Convert tuple_id parameter to const TID &tid
    sed -i 's/uint64_t tuple_id,/const TID \&tid,/g' "$file"
    sed -i 's/uint64_t tuple_id)/const TID \&tid)/g' "$file"
    sed -i 's/uint64_t new_tuple_id,/const TID \&new_tid,/g' "$file"
    sed -i 's/uint64_t new_tuple_id)/const TID \&new_tid)/g' "$file"

    # Convert vector<uint64_t> *tuple_ids_out to vector<TID> *tids_out
    sed -i 's/std::vector<uint64_t> \*tuple_ids_out/std::vector<TID> \*tids_out/g' "$file"
    sed -i 's/std::vector<uint64_t> \&dead_tids/const std::vector<TID> \&dead_tids/g' "$file"
    sed -i 's/const std::vector<uint64_t> \&dead_tids/const std::vector<TID> \&dead_tids/g' "$file"

    # Convert references from tuple_ids_out to tids_out
    sed -i 's/tuple_ids_out->/tids_out->/g' "$file"
    sed -i 's/(tuple_ids_out)/(tids_out)/g' "$file"

    # Note: Manual fixes still needed for:
    # - Variable name changes (tuple_id -> tid, new_tuple_id -> new_tid)
    # - Conversion calls (convertTIDtoLegacy, convertLegacyTID)
    # - Iterator signatures
}

echo "[2/10] Migrating btree.cpp..."
# B-Tree specific migrations (already partially done)
# Skip btree.cpp as we've been manually updating it

echo "[3/10] Migrating hash_index.cpp..."
if [ -f "src/core/hash_index.cpp" ]; then
    migrate_impl_file "src/core/hash_index.cpp"
fi

echo "[4/10] Migrating gin_index.cpp..."
if [ -f "src/core/gin_index.cpp" ]; then
    migrate_impl_file "src/core/gin_index.cpp"
fi

echo "[5/10] Migrating bitmap_index.cpp..."
if [ -f "src/core/bitmap_index.cpp" ]; then
    migrate_impl_file "src/core/bitmap_index.cpp"
fi

echo "[6/10] Migrating hnsw_index.cpp..."
if [ -f "src/core/hnsw_index.cpp" ]; then
    migrate_impl_file "src/core/hnsw_index.cpp"
fi

echo "[7/10] Migrating brin_index.cpp..."
if [ -f "src/core/brin_index.cpp" ]; then
    migrate_impl_file "src/core/brin_index.cpp"
fi

echo ""
echo "=== Migration Complete ==="
echo ""
echo "IMPORTANT: This script performed automated replacements."
echo "Manual fixes are still required for:"
echo "  1. Variable renames (tuple_id -> tid throughout function bodies)"
echo "  2. TID conversion calls (convertTIDtoLegacy/convertLegacyTID)"
echo "  3. Iterator method signatures"
echo "  4. On-disk storage format updates"
echo "  5. Compilation error fixes"
echo ""
echo "Next steps:"
echo "  1. Review git diff to see all changes"
echo "  2. Manually fix remaining issues"
echo "  3. Attempt compilation: cd build && make"
echo "  4. Fix compilation errors iteratively"
echo ""
echo "Backups available in: $BACKUP_DIR"
