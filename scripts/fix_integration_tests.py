#!/usr/bin/env python3
"""
Automated script to fix common API mismatches in integration tests.

This script addresses systematic API changes:
1. Database::create() - now static with page_size parameter
2. TransactionManager::beginTransaction() - requires proc_id, xid_out
3. TransactionManager::commitTransaction() - requires proc_id
4. TransactionManager::rollbackTransaction() - requires proc_id
5. createTID() → makeTID()
6. Index find() methods - return Status, take results pointer
7. Missing connection_context.h include for IsolationLevel
"""

import re
import sys
import os
from pathlib import Path

def fix_includes(content):
    """Add connection_context.h include if missing and IsolationLevel is used."""
    if 'IsolationLevel' in content and 'connection_context.h' not in content:
        # Find the last #include line
        lines = content.split('\n')
        last_include_idx = -1
        for i, line in enumerate(lines):
            if line.strip().startswith('#include'):
                last_include_idx = i

        if last_include_idx >= 0:
            lines.insert(last_include_idx + 1, '#include "scratchbird/core/connection_context.h"')
            content = '\n'.join(lines)

    return content

def fix_database_create(content):
    """Fix Database::create() calls - change from instance to static method."""
    # Pattern: db_->create(path, &ctx) or db->create(path, &ctx)
    # Replace with: Database::create(path, 8192, &ctx) and move before db_ = make_unique

    # Find the pattern db_->create or db->create
    pattern = r'(\s+)(db_?)\s*->\s*create\s*\((.*?)\)'

    def replace_create(match):
        indent = match.group(1)
        args = match.group(3)
        # Insert page_size parameter (8192) if not present
        if args.count(',') == 1:  # Only path and ctx
            args_parts = args.split(',')
            return f'{indent}Status status = Database::create({args_parts[0]}, 8192, {args_parts[1]})'
        return match.group(0)  # Already has page_size

    content = re.sub(pattern, replace_create, content)

    # Also handle db_ = make_unique ordering
    # Pattern: db_ = make_unique<Database>(); ... status = db_->create
    # Should be: Database::create(...); db_ = make_unique<Database>(); db_->open(...)

    return content

def fix_begin_transaction(content):
    """Fix beginTransaction() calls to use new API."""
    # Old: uint64_t xid = tx_manager_->beginTransaction(...)
    # New: uint64_t xid; status = tx_manager_->beginTransaction(0, xid, &ctx);

    # Pattern 1: Simple assignment
    pattern1 = r'uint64_t\s+(\w+)\s*=\s*tx(?:n)?_manager_?\s*->\s*beginTransaction\s*\([^)]*\);'
    replacement1 = r'uint64_t \1;\n    status = tx_manager_->beginTransaction(0, \1, &ctx);\n    ASSERT_EQ(status, Status::OK);'
    content = re.sub(pattern1, replacement1, content)

    # Pattern 2: Already has status = but wrong signature
    pattern2 = r'status\s*=\s*tx(?:n)?_manager_?\s*->\s*beginTransaction\s*\(\s*(\w+)\s*,\s*(\w+)\s*,\s*&ctx\s*\);'
    # This is already correct if proc_id is first (0) and xid is second
    # Check if it's backwards (xid, proc_id)

    return content

def fix_commit_transaction(content):
    """Fix commitTransaction() calls to include proc_id."""
    # Old: tx_manager_->commitTransaction(xid);
    # New: status = tx_manager_->commitTransaction(0, xid, &ctx); ASSERT_EQ(status, Status::OK);

    pattern = r'tx(?:n)?_manager_?\s*->\s*commitTransaction\s*\(\s*(\w+)\s*\);'
    replacement = r'status = tx_manager_->commitTransaction(0, \1, &ctx);\n    ASSERT_EQ(status, Status::OK);'
    content = re.sub(pattern, replacement, content)

    return content

def fix_rollback_transaction(content):
    """Fix rollbackTransaction() calls to include proc_id."""
    pattern = r'tx(?:n)?_manager_?\s*->\s*rollbackTransaction\s*\(\s*(\w+)\s*\);'
    replacement = r'status = tx_manager_->rollbackTransaction(0, \1, &ctx);\n    ASSERT_EQ(status, Status::OK);'
    content = re.sub(pattern, replacement, content)

    return content

def fix_create_tid(content):
    """Fix createTID() → makeTID()."""
    content = content.replace('createTID(', 'makeTID(')
    return content

def fix_index_find(content):
    """Fix index find() calls that return vector to use Status + pointer."""
    # This is complex and file-specific, so we'll handle it manually for now
    return content

def process_file(filepath):
    """Process a single test file."""
    print(f"Processing {filepath}...")

    try:
        with open(filepath, 'r') as f:
            content = f.read()

        original = content

        # Apply fixes
        content = fix_includes(content)
        content = fix_database_create(content)
        content = fix_begin_transaction(content)
        content = fix_commit_transaction(content)
        content = fix_rollback_transaction(content)
        content = fix_create_tid(content)
        content = fix_index_find(content)

        if content != original:
            with open(filepath, 'w') as f:
                f.write(content)
            print(f"  ✓ Fixed {filepath}")
            return True
        else:
            print(f"  - No changes needed for {filepath}")
            return False

    except Exception as e:
        print(f"  ✗ Error processing {filepath}: {e}")
        return False

def main():
    """Main entry point."""
    test_dir = Path("/home/user/ScratchBird/tests/integration")

    if not test_dir.exists():
        print(f"Error: {test_dir} does not exist")
        return 1

    cpp_files = list(test_dir.glob("*.cpp"))

    if not cpp_files:
        print(f"No .cpp files found in {test_dir}")
        return 1

    print(f"Found {len(cpp_files)} integration test files")
    print("="*60)

    fixed_count = 0
    for filepath in sorted(cpp_files):
        if process_file(filepath):
            fixed_count += 1

    print("="*60)
    print(f"Fixed {fixed_count} / {len(cpp_files)} files")

    return 0

if __name__ == "__main__":
    sys.exit(main())
