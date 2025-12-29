#!/usr/bin/env python3
"""
Convert Firebird .fbt test files to plain .sql files

.fbt format is a Python dictionary with:
  - Metadata: id, tracker_id, title, description
  - versions: array of test cases for different Firebird versions
    Each version contains:
      - firebird_version, platform, test_type
      - Optional: init_script (DDL setup)
      - test_script (actual SQL to execute)
      - expected_stdout and/or expected_stderr
      - Optional: substitutions (output normalization patterns)

Sources:
  - https://github.com/FirebirdSQL/firebird-qa
  - https://github.com/FirebirdSQL/fbt-repository
"""

import ast
import sys
import json
from pathlib import Path
from typing import Dict, List, Any

def parse_fbt_file(fbt_path: Path) -> Dict[str, Any]:
    """
    Parse .fbt file using Python's ast.literal_eval

    Args:
        fbt_path: Path to .fbt file

    Returns:
        Dictionary containing test data
    """
    try:
        with open(fbt_path, 'r', encoding='utf-8') as f:
            content = f.read()

        # .fbt files are Python dictionaries
        test_data = ast.literal_eval(content)
        return test_data

    except SyntaxError as e:
        print(f"ERROR: Failed to parse {fbt_path}: {e}", file=sys.stderr)
        return None
    except Exception as e:
        print(f"ERROR: Failed to read {fbt_path}: {e}", file=sys.stderr)
        return None

def convert_fbt_to_sql(fbt_path: Path, output_dir: Path, version_filter: str = None) -> List[Path]:
    """
    Convert .fbt file to .sql file(s)

    Args:
        fbt_path: Path to .fbt file
        output_dir: Output directory for converted files
        version_filter: Optional Firebird version to filter (e.g., "3.0")

    Returns:
        List of generated SQL file paths
    """
    test_data = parse_fbt_file(fbt_path)
    if not test_data:
        return []

    # Create output directory
    output_dir.mkdir(parents=True, exist_ok=True)

    # Extract metadata
    test_id = test_data.get('id', 'unknown')
    tracker_id = test_data.get('tracker_id', '')
    title = test_data.get('title', 'Untitled')
    description = test_data.get('description', '')

    # Get versions
    versions = test_data.get('versions', [])
    if not versions:
        print(f"WARNING: No versions found in {fbt_path}", file=sys.stderr)
        return []

    generated_files = []

    for idx, version in enumerate(versions):
        fb_version = version.get('firebird_version', 'unknown')

        # Skip if version filter specified and doesn't match
        if version_filter and fb_version != version_filter:
            continue

        platform = version.get('platform', 'All')
        test_type = version.get('test_type', 'ISQL')

        # Get SQL scripts
        init_script = version.get('init_script', '').strip()
        test_script = version.get('test_script', '').strip()

        if not test_script:
            continue

        # Get expected outputs
        expected_stdout = version.get('expected_stdout', '').strip()
        expected_stderr = version.get('expected_stderr', '').strip()
        substitutions = version.get('substitutions', [])

        # Generate filename
        base_name = fbt_path.stem
        if len(versions) > 1:
            # Multiple versions - include version in filename
            sql_filename = f"{base_name}_v{fb_version.replace('.', '_')}.sql"
            expected_filename = f"{base_name}_v{fb_version.replace('.', '_')}.expected"
        else:
            sql_filename = f"{base_name}.sql"
            expected_filename = f"{base_name}.expected"

        sql_path = output_dir / sql_filename

        # Write SQL file
        with open(sql_path, 'w', encoding='utf-8') as f:
            # Header with metadata
            f.write(f"-- Test ID: {test_id}\n")
            if tracker_id:
                f.write(f"-- Tracker: {tracker_id}\n")
            f.write(f"-- Title: {title}\n")
            if description:
                f.write(f"-- Description: {description}\n")
            f.write(f"-- Firebird Version: {fb_version}\n")
            f.write(f"-- Platform: {platform}\n")
            f.write(f"-- Test Type: {test_type}\n")
            f.write(f"-- Converted from: {fbt_path.name}\n")
            f.write("\n")

            # Init script (if present)
            if init_script:
                f.write("-- === INIT SCRIPT ===\n")
                f.write(init_script)
                if not init_script.endswith('\n'):
                    f.write('\n')
                f.write("\n")

            # Test script
            f.write("-- === TEST SCRIPT ===\n")
            f.write(test_script)
            if not test_script.endswith('\n'):
                f.write('\n')

        generated_files.append(sql_path)

        # Write expected output file (if there's any expected output)
        if expected_stdout or expected_stderr or substitutions:
            expected_dir = output_dir.parent / 'expected'
            expected_dir.mkdir(parents=True, exist_ok=True)
            expected_path = expected_dir / expected_filename

            with open(expected_path, 'w', encoding='utf-8') as f:
                if expected_stdout:
                    f.write("=== EXPECTED STDOUT ===\n")
                    f.write(expected_stdout)
                    if not expected_stdout.endswith('\n'):
                        f.write('\n')
                    f.write("\n")

                if expected_stderr:
                    f.write("=== EXPECTED STDERR ===\n")
                    f.write(expected_stderr)
                    if not expected_stderr.endswith('\n'):
                        f.write('\n')
                    f.write("\n")

                if substitutions:
                    f.write("=== OUTPUT SUBSTITUTIONS ===\n")
                    for sub in substitutions:
                        f.write(f"Pattern: {sub[0]}\n")
                        f.write(f"Replace: {sub[1]}\n")
                        f.write("\n")

    return generated_files

def main():
    """Main entry point for command-line usage"""
    if len(sys.argv) < 3:
        print("Usage: convert_fbt_to_sql.py <input.fbt> <output_dir> [firebird_version]")
        print()
        print("Arguments:")
        print("  input.fbt        - Path to Firebird .fbt test file")
        print("  output_dir       - Output directory for converted SQL files")
        print("  firebird_version - Optional: Filter to specific Firebird version (e.g., '3.0')")
        print()
        print("Examples:")
        print("  convert_fbt_to_sql.py test.fbt converted/")
        print("  convert_fbt_to_sql.py test.fbt converted/ 3.0")
        sys.exit(1)

    fbt_file = Path(sys.argv[1])
    output_dir = Path(sys.argv[2])
    version_filter = sys.argv[3] if len(sys.argv) > 3 else None

    if not fbt_file.exists():
        print(f"ERROR: File not found: {fbt_file}", file=sys.stderr)
        sys.exit(1)

    generated_files = convert_fbt_to_sql(fbt_file, output_dir, version_filter)

    if generated_files:
        for sql_file in generated_files:
            print(f"✓ Converted: {fbt_file.name} → {sql_file.name}")
    else:
        print(f"✗ Failed to convert: {fbt_file.name}", file=sys.stderr)
        sys.exit(1)

if __name__ == '__main__':
    main()
