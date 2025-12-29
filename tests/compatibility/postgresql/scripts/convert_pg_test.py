#!/usr/bin/env python3
"""
Convert PostgreSQL regression test files to standardized .sql files

PostgreSQL test format is mostly pure SQL with minimal directives.
Tests are in src/test/regress/sql/*.sql
Expected output in src/test/regress/expected/*.out

This converter adds metadata headers and copies expected output.

Sources:
  - https://github.com/postgres/postgres
  - https://www.postgresql.org/docs/current/regress.html
"""

import sys
from pathlib import Path
from typing import Tuple

def convert_pg_test(test_path: Path, output_dir: Path) -> Tuple[Path, Path]:
    """
    Convert PostgreSQL .sql test to standardized format

    Args:
        test_path: Path to PostgreSQL .sql file
        output_dir: Output directory for converted files

    Returns:
        Tuple of (sql_path, expected_path) or (None, None) on failure
    """
    try:
        with open(test_path, 'r', encoding='utf-8', errors='replace') as f:
            content = f.read()
    except Exception as e:
        print(f"ERROR: Failed to read {test_path}: {e}", file=sys.stderr)
        return None, None

    # Create output directory
    output_dir.mkdir(parents=True, exist_ok=True)

    test_name = test_path.stem

    # Generate SQL file with metadata header
    sql_path = output_dir / f"{test_name}.sql"
    with open(sql_path, 'w', encoding='utf-8') as f:
        f.write(f"-- PostgreSQL Regression Test: {test_name}\n")
        f.write(f"-- Converted from: {test_path.name}\n")
        f.write(f"-- Original path: {test_path.relative_to(test_path.parents[5])}\n")
        f.write("\n")
        f.write(content)
        if content and not content.endswith('\n'):
            f.write('\n')

    # Check for corresponding .out file (expected output)
    expected_source = test_path.parent.parent / 'expected' / f"{test_name}.out"
    expected_path = None

    if expected_source.exists():
        try:
            with open(expected_source, 'r', encoding='utf-8', errors='replace') as f:
                expected_content = f.read()

            expected_dir = output_dir.parent / 'expected'
            expected_dir.mkdir(parents=True, exist_ok=True)
            expected_path = expected_dir / f"{test_name}.expected"

            with open(expected_path, 'w', encoding='utf-8') as f:
                f.write(f"-- PostgreSQL Expected Output: {test_name}\n")
                f.write(f"-- From: {expected_source.relative_to(expected_source.parents[5])}\n")
                f.write("\n")
                f.write(expected_content)

        except Exception as e:
            print(f"WARNING: Failed to process expected output {expected_source}: {e}", file=sys.stderr)

    # Check for variant expected outputs (platform-specific, version-specific)
    # PostgreSQL sometimes has multiple .out files like test_1.out, test_2.out
    variant_pattern = f"{test_name}_*.out"
    variant_files = list(expected_source.parent.glob(variant_pattern))

    for variant_file in variant_files:
        try:
            variant_name = variant_file.stem
            with open(variant_file, 'r', encoding='utf-8', errors='replace') as f:
                variant_content = f.read()

            expected_dir = output_dir.parent / 'expected'
            expected_dir.mkdir(parents=True, exist_ok=True)
            variant_expected_path = expected_dir / f"{variant_name}.expected"

            with open(variant_expected_path, 'w', encoding='utf-8') as f:
                f.write(f"-- PostgreSQL Expected Output (Variant): {variant_name}\n")
                f.write(f"-- From: {variant_file.relative_to(variant_file.parents[5])}\n")
                f.write("\n")
                f.write(variant_content)

            print(f"  ├─ Variant: {variant_expected_path.name}", file=sys.stderr)

        except Exception as e:
            print(f"WARNING: Failed to process variant {variant_file}: {e}", file=sys.stderr)

    return sql_path, expected_path

def main():
    """Main entry point for command-line usage"""
    if len(sys.argv) < 3:
        print("Usage: convert_pg_test.py <input.sql> <output_dir>")
        print()
        print("Arguments:")
        print("  input.sql   - Path to PostgreSQL .sql test file")
        print("  output_dir  - Output directory for converted SQL files")
        print()
        print("Examples:")
        print("  convert_pg_test.py select.sql converted/core/")
        sys.exit(1)

    test_file = Path(sys.argv[1])
    output_dir = Path(sys.argv[2])

    if not test_file.exists():
        print(f"ERROR: File not found: {test_file}", file=sys.stderr)
        sys.exit(1)

    sql_path, expected_path = convert_pg_test(test_file, output_dir)

    if sql_path:
        print(f"✓ Converted: {test_file.name} → {sql_path.name}")
        if expected_path:
            print(f"  └─ Expected: {expected_path.name}")
    else:
        print(f"✗ Failed to convert: {test_file.name}", file=sys.stderr)
        sys.exit(1)

if __name__ == '__main__':
    main()
