#!/usr/bin/env python3
"""
Generate test manifests for all converted compatibility tests

Creates JSON manifests containing:
  - Test inventory (all tests with metadata)
  - Category organization
  - Statistics
"""

import json
import sys
from pathlib import Path
from typing import Dict, List
from datetime import datetime

def generate_manifest(converted_dir: Path, database_name: str) -> Dict:
    """
    Generate test manifest for a database

    Args:
        converted_dir: Directory containing converted SQL tests
        database_name: Database name (firebird, mysql, postgresql)

    Returns:
        Dictionary containing manifest data
    """
    manifest = {
        "database": database_name,
        "generated": datetime.now().isoformat(),
        "version": "1.0",
        "categories": {},
        "tests": [],
        "statistics": {
            "total_tests": 0,
            "total_categories": 0
        }
    }

    if not converted_dir.exists():
        print(f"WARNING: Directory not found: {converted_dir}", file=sys.stderr)
        return manifest

    # Find all SQL test files
    sql_files = sorted(converted_dir.glob("**/*.sql"))

    # Exclude expected output directory
    sql_files = [f for f in sql_files if '/expected/' not in str(f)]

    for sql_file in sql_files:
        # Determine category from path
        rel_path = sql_file.relative_to(converted_dir)
        category = str(rel_path.parent) if rel_path.parent != Path('.') else "main"

        # Extract test metadata from file header
        test_name = sql_file.stem
        test_metadata = extract_metadata(sql_file)

        # Add to tests list
        test_entry = {
            "name": test_name,
            "file": str(rel_path),
            "category": category,
            "path": str(sql_file),
            **test_metadata
        }
        manifest["tests"].append(test_entry)

        # Update category statistics
        if category not in manifest["categories"]:
            manifest["categories"][category] = {
                "path": str(rel_path.parent),
                "test_count": 0,
                "tests": []
            }

        manifest["categories"][category]["test_count"] += 1
        manifest["categories"][category]["tests"].append(test_name)

    # Update statistics
    manifest["statistics"]["total_tests"] = len(manifest["tests"])
    manifest["statistics"]["total_categories"] = len(manifest["categories"])

    return manifest

def extract_metadata(sql_file: Path) -> Dict:
    """
    Extract metadata from SQL file header comments

    Args:
        sql_file: Path to SQL file

    Returns:
        Dictionary of metadata
    """
    metadata = {}

    try:
        with open(sql_file, 'r', encoding='utf-8') as f:
            # Read first 50 lines (header comments)
            for _ in range(50):
                line = f.readline()
                if not line:
                    break

                line = line.strip()

                # Stop at first non-comment line
                if not line.startswith('--'):
                    break

                # Parse metadata comments
                if line.startswith('-- Test ID:'):
                    metadata['test_id'] = line.split(':', 1)[1].strip()
                elif line.startswith('-- Tracker:'):
                    metadata['tracker'] = line.split(':', 1)[1].strip()
                elif line.startswith('-- Title:'):
                    metadata['title'] = line.split(':', 1)[1].strip()
                elif line.startswith('-- Description:'):
                    metadata['description'] = line.split(':', 1)[1].strip()
                elif line.startswith('-- Firebird Version:'):
                    metadata['version'] = line.split(':', 1)[1].strip()
                elif line.startswith('-- Platform:'):
                    metadata['platform'] = line.split(':', 1)[1].strip()
                elif line.startswith('-- Test Type:'):
                    metadata['test_type'] = line.split(':', 1)[1].strip()
                elif line.startswith('-- Converted from:'):
                    metadata['original_file'] = line.split(':', 1)[1].strip()

    except Exception as e:
        print(f"WARNING: Failed to extract metadata from {sql_file}: {e}", file=sys.stderr)

    return metadata

def main():
    """Generate manifests for all databases"""
    compat_dir = Path("/home/dcalford/CliWork/ScratchBird/tests/compatibility")

    databases = [
        ("firebird", compat_dir / "firebird" / "converted"),
        ("mysql", compat_dir / "mysql" / "converted"),
        ("postgresql", compat_dir / "postgresql" / "converted")
    ]

    print("")
    print("═══════════════════════════════════════════════════════════")
    print("  Generating Test Manifests")
    print("═══════════════════════════════════════════════════════════")
    print("")

    for db_name, converted_dir in databases:
        print(f"Processing {db_name}...")

        manifest = generate_manifest(converted_dir, db_name)

        # Save manifest
        config_dir = compat_dir / db_name / "config"
        config_dir.mkdir(parents=True, exist_ok=True)
        manifest_path = config_dir / "test_manifest.json"

        with open(manifest_path, 'w', encoding='utf-8') as f:
            json.dump(manifest, f, indent=2)

        print(f"  ✓ {manifest['statistics']['total_tests']} tests in {manifest['statistics']['total_categories']} categories")
        print(f"  📄 Manifest saved to: {manifest_path}")
        print("")

    # Generate combined summary
    print("═══════════════════════════════════════════════════════════")
    print("  Summary")
    print("═══════════════════════════════════════════════════════════")
    print("")

    total_tests = 0
    for db_name, converted_dir in databases:
        manifest_path = compat_dir / db_name / "config" / "test_manifest.json"
        with open(manifest_path, 'r') as f:
            manifest = json.load(f)
            print(f"{db_name.capitalize():12} {manifest['statistics']['total_tests']:6} tests in {manifest['statistics']['total_categories']:3} categories")
            total_tests += manifest['statistics']['total_tests']

    print(f"{'Total':12} {total_tests:6} tests")
    print("")
    print("✓ All manifests generated successfully!")

if __name__ == '__main__':
    main()
