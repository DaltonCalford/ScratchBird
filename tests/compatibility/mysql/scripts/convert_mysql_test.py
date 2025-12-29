#!/usr/bin/env python3
"""
Convert MySQL .test files to plain .sql files

MySQL test format includes:
  - SQL statements
  - Test directives (--source, --echo, --error, etc.)
  - Comments (# or --)
  - Control commands (@, delimiter, etc.)

This converter extracts the core SQL and converts directives to comments.

Sources:
  - https://github.com/mysql/mysql-server
  - https://dev.mysql.com/doc/dev/mysql-server/latest/PAGE_MYSQL_TEST_RUN.html
"""

import re
import sys
from pathlib import Path
from typing import List, Tuple

def convert_mysql_test(test_path: Path, output_dir: Path) -> Tuple[Path, Path]:
    """
    Convert MySQL .test file to .sql file

    Args:
        test_path: Path to MySQL .test file
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
    sql_lines = []
    expected_lines = []
    in_expected_section = False

    # Track if we need expected output file
    has_expected_output = False

    for line in content.split('\n'):
        stripped = line.strip()

        # Skip empty lines (but preserve in SQL for readability)
        if not stripped:
            if not in_expected_section:
                sql_lines.append('')
            continue

        # Handle directives
        if stripped.startswith('--'):
            # --source directives (include other files)
            if stripped.startswith('--source'):
                sql_lines.append(f"-- DIRECTIVE: {stripped}")

            # --echo directives (output messages)
            elif stripped.startswith('--echo'):
                echo_text = stripped[6:].strip()
                sql_lines.append(f"-- ECHO: {echo_text}")
                expected_lines.append(echo_text)
                has_expected_output = True

            # --error directives (expected error codes)
            elif stripped.startswith('--error'):
                sql_lines.append(f"-- EXPECTED ERROR: {stripped[7:].strip()}")

            # --disable_* and --enable_* directives
            elif stripped.startswith('--disable_') or stripped.startswith('--enable_'):
                sql_lines.append(f"-- DIRECTIVE: {stripped}")

            # --replace_* directives (output substitution)
            elif stripped.startswith('--replace_'):
                sql_lines.append(f"-- DIRECTIVE: {stripped}")

            # --sorted_result directive
            elif stripped.startswith('--sorted_result'):
                sql_lines.append(f"-- DIRECTIVE: {stripped}")

            # --skip directive (skip test)
            elif stripped.startswith('--skip'):
                sql_lines.append(f"-- SKIP: {stripped[6:].strip()}")

            # Regular comments (keep as-is)
            else:
                sql_lines.append(line)

        # Handle # comments (MySQL style)
        elif stripped.startswith('#'):
            sql_lines.append(f"-- {stripped[1:].strip()}")

        # Handle mysqltest commands (eval, let, etc.)
        elif stripped.startswith('--'):
            sql_lines.append(f"-- DIRECTIVE: {stripped}")

        # Handle DELIMITER command (mysqltest specific)
        elif stripped.upper().startswith('DELIMITER'):
            delimiter = stripped.split(None, 1)[1] if len(stripped.split()) > 1 else ';'
            sql_lines.append(f"-- DELIMITER CHANGED TO: {delimiter}")

        # Regular SQL statements
        else:
            sql_lines.append(line)

    # Generate SQL file
    sql_path = output_dir / f"{test_name}.sql"
    with open(sql_path, 'w', encoding='utf-8') as f:
        f.write(f"-- MySQL Test: {test_name}\n")
        f.write(f"-- Converted from: {test_path.name}\n")
        f.write(f"-- Original path: {test_path.relative_to(test_path.parents[4])}\n")
        f.write("\n")
        f.write('\n'.join(sql_lines))
        if sql_lines and not sql_lines[-1].endswith('\n'):
            f.write('\n')

    # Check for corresponding .result file
    result_path = test_path.parent.parent / 'r' / f"{test_name}.result"
    expected_path = None

    if result_path.exists():
        try:
            with open(result_path, 'r', encoding='utf-8', errors='replace') as f:
                result_content = f.read()

            expected_dir = output_dir.parent / 'expected'
            expected_dir.mkdir(parents=True, exist_ok=True)
            expected_path = expected_dir / f"{test_name}.expected"

            with open(expected_path, 'w', encoding='utf-8') as f:
                f.write(f"-- MySQL Expected Results: {test_name}\n")
                f.write(f"-- From: {result_path.relative_to(result_path.parents[4])}\n")
                f.write("\n")
                f.write(result_content)

            has_expected_output = True
        except Exception as e:
            print(f"WARNING: Failed to process result file {result_path}: {e}", file=sys.stderr)

    return sql_path, expected_path

def main():
    """Main entry point for command-line usage"""
    if len(sys.argv) < 3:
        print("Usage: convert_mysql_test.py <input.test> <output_dir>")
        print()
        print("Arguments:")
        print("  input.test  - Path to MySQL .test file")
        print("  output_dir  - Output directory for converted SQL files")
        print()
        print("Examples:")
        print("  convert_mysql_test.py select.test converted/main/")
        sys.exit(1)

    test_file = Path(sys.argv[1])
    output_dir = Path(sys.argv[2])

    if not test_file.exists():
        print(f"ERROR: File not found: {test_file}", file=sys.stderr)
        sys.exit(1)

    sql_path, expected_path = convert_mysql_test(test_file, output_dir)

    if sql_path:
        print(f"✓ Converted: {test_file.name} → {sql_path.name}")
        if expected_path:
            print(f"  └─ Expected: {expected_path.name}")
    else:
        print(f"✗ Failed to convert: {test_file.name}", file=sys.stderr)
        sys.exit(1)

if __name__ == '__main__':
    main()
