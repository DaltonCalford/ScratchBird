#!/usr/bin/env python3
"""
Phase 1.5: Complete TID Migration Script

This script automates the migration of all index implementations and core APIs
from uint64_t tuple_id to TID struct.

Usage: python3 scripts/complete_tid_migration.py
"""

import re
import sys
from pathlib import Path
from typing import List, Tuple

class TIDMigrationTool:
    def __init__(self, repo_root: Path):
        self.repo_root = repo_root
        self.src_dir = repo_root / "src" / "core"
        self.include_dir = repo_root / "include" / "scratchbird" / "core"
        self.changes_made = []

    def migrate_implementation_file(self, filepath: Path) -> int:
        """Migrate a single .cpp implementation file"""
        if not filepath.exists():
            print(f"Skipping {filepath} - file not found")
            return 0

        print(f"Migrating {filepath.name}...")

        with open(filepath, 'r') as f:
            content = f.read()

        original = content
        changes = 0

        # Pattern 1: Method signatures with uint64_t tuple_id
        content, n = re.subn(
            r'(Status\s+\w+Index::(?:insert|remove))\s*\([^)]*uint64_t\s+tuple_id',
            lambda m: m.group(1) + '(' + m.group(0).split('(')[1].replace('uint64_t tuple_id', 'const TID &tid'),
            content
        )
        changes += n

        # Pattern 2: std::vector<uint64_t> *tuple_ids_out -> std::vector<TID> *tids_out
        content, n = re.subn(
            r'std::vector<uint64_t>\s*\*tuple_ids_out',
            'std::vector<TID> *tids_out',
            content
        )
        changes += n

        # Pattern 3: std::vector<uint64_t> return types
        content, n = re.subn(
            r'std::vector<uint64_t>\s+(\w+Index::find)',
            r'std::vector<TID> \1',
            content
        )
        changes += n

        # Pattern 4: removeDeadEntries signature
        content, n = re.subn(
            r'Status\s+(\w+Index::removeDeadEntries)\s*\(\s*const\s+std::vector<uint64_t>\s*&dead_tids',
            r'Status \1(const std::vector<TID> &dead_tids',
            content
        )
        changes += n

        if content != original:
            with open(filepath, 'w') as f:
                f.write(content)
            self.changes_made.append(f"{filepath.name}: {changes} automatic replacements")
            return changes

        return 0

    def add_tid_conversions(self, filepath: Path) -> int:
        """Add TID conversion comments where needed"""
        if not filepath.exists():
            return 0

        with open(filepath, 'r') as f:
            content = f.read()

        original = content
        changes = 0

        # Add conversion helper comment at the top of key methods
        insert_comment = """    // PHASE 1.5: Convert TID to legacy format for storage
    // uint64_t legacy_tid = convertTIDtoLegacy(tid);
    // if (legacy_tid == 0) return Status::NOT_IMPLEMENTED;
"""

        # This is a placeholder - actual conversion code needs manual insertion
        # The script identifies locations but doesn't auto-insert to avoid breaking code

        return changes

    def migrate_all_indexes(self):
        """Migrate all index implementation files"""
        index_files = [
            'hash_index.cpp',
            'gin_index.cpp',
            'bitmap_index.cpp',
            'hnsw_index.cpp',
            'brin_index.cpp',
        ]

        total_changes = 0
        for filename in index_files:
            filepath = self.src_dir / filename
            changes = self.migrate_implementation_file(filepath)
            total_changes += changes

        return total_changes

    def generate_migration_report(self) -> str:
        """Generate a report of all changes made"""
        report = ["=== TID Migration Report ===\n"]
        report.append(f"Total files processed: {len(self.changes_made)}\n")
        report.append("\nChanges by file:\n")
        for change in self.changes_made:
            report.append(f"  - {change}\n")

        report.append("\n=== Manual Steps Required ===\n")
        report.append("""
After running this script, you must manually add:

1. TID conversion calls in each method:
   - insert(): uint64_t legacy = convertTIDtoLegacy(tid);
   - remove(): uint64_t legacy = convertTIDtoLegacy(tid);
   - find(): TID tid = convertLegacyTID(stored_tid); for each result
   - removeDeadEntries(): Build std::set<uint64_t> from vector<TID>

2. Variable name changes:
   - tuple_id -> tid (throughout method bodies)
   - tuple_ids_out -> tids_out (in method bodies)
   - new_tuple_id -> new_tid

3. Compilation fixes:
   - Run: cd build && make 2>&1 | tee errors.log
   - Fix any remaining type mismatches

4. Test updates:
   - Update test files to use TID struct
   - Run: cd build && ctest --output-on-failure

See docs/guides/PHASE_1_5_COMPLETION_GUIDE.md for detailed patterns.
""")

        return "".join(report)


def main():
    # Find repository root
    script_dir = Path(__file__).parent
    repo_root = script_dir.parent

    print("Phase 1.5: TID Migration - Automated Tool")
    print("=" * 50)
    print()

    tool = TIDMigrationTool(repo_root)

    # Migrate all index files
    print("Migrating index implementations...")
    total_changes = tool.migrate_all_indexes()

    # Generate report
    report = tool.generate_migration_report()
    print()
    print(report)

    # Write report to file
    report_file = repo_root / "PHASE_1_5_MIGRATION_REPORT.txt"
    with open(report_file, 'w') as f:
        f.write(report)

    print(f"\nReport saved to: {report_file}")
    print(f"\nTotal automatic replacements: {total_changes}")
    print("\nNOTE: Manual steps still required - see report above")
    print("Refer to docs/guides/PHASE_1_5_COMPLETION_GUIDE.md for detailed instructions")

    return 0


if __name__ == "__main__":
    sys.exit(main())
