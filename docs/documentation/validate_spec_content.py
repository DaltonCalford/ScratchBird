#!/usr/bin/env python3
"""
Rigorous Specification Content Validation

Validates that specs actually describe implemented code structures.
"""

import os
import re
from pathlib import Path

def check_struct_in_file(file_path, struct_name):
    """Check if a struct/class is actually defined in the file"""
    try:
        with open(file_path, 'r') as f:
            content = f.read()
            # Look for struct/class definition
            pattern = rf'\b(struct|class)\s+{re.escape(struct_name)}\b'
            return bool(re.search(pattern, content))
    except:
        return False

def check_function_in_file(file_path, func_name):
    """Check if a function is defined in the file"""
    try:
        with open(file_path, 'r') as f:
            content = f.read()
            # Look for function definition
            pattern = rf'\b{re.escape(func_name)}\s*\('
            return bool(re.search(pattern, content))
    except:
        return False

def validate_spec(spec_path, project_root):
    """Validate a specification file against actual code"""
    issues = []
    
    try:
        with open(spec_path, 'r') as f:
            content = f.read()
    except:
        return ["Cannot read file"]
        
    # Extract all struct/class names mentioned in code blocks
    struct_pattern = r'`?(\w+)`?\s*\{'  # e.g., "struct Foo {" or "Foo {"
    for match in re.finditer(struct_pattern, content):
        struct_name = match.group(1)
        # Skip common non-struct words
        if struct_name in ['if', 'while', 'for', 'switch', 'return', 'Status', 'int', 'void', 'bool', 'auto']:
            continue
            
    # Extract source anchors
    anchor_pattern = r'`(/home/dcalford/CliWork/ScratchBird/([^`]+)):(\d+)`'
    for match in re.finditer(anchor_pattern, content):
        full_path = match.group(1)
        rel_path = match.group(2)
        line_num = int(match.group(3))
        
        # Verify file exists
        abs_path = project_root / rel_path
        if not abs_path.exists():
            issues.append(f"File does not exist: {rel_path}")
            continue
            
        # Verify line number is valid
        try:
            with open(abs_path, 'r') as f:
                lines = f.readlines()
                if line_num > len(lines):
                    issues.append(f"Line {line_num} does not exist in {rel_path} (file has {len(lines)} lines)")
        except Exception as e:
            issues.append(f"Error reading {rel_path}: {e}")
            
    return issues

def main():
    project_root = Path('/home/dcalford/CliWork/ScratchBird')
    specs_root = project_root / 'docs/documentation/developers_guide/specifications'
    
    print("=" * 70)
    print("RIGOROUS SPEC CONTENT VALIDATION")
    print("=" * 70)
    print()
    
    spec_files = list(specs_root.rglob('*.md'))
    total = len(spec_files)
    valid = 0
    with_issues = 0
    
    for spec_path in sorted(spec_files):
        if spec_path.name in ['README.md', 'TEMPLATE.md', 'index.md']:
            continue
            
        rel_path = spec_path.relative_to(specs_root)
        issues = validate_spec(spec_path, project_root)
        
        if issues:
            with_issues += 1
            print(f"❌ {rel_path}")
            for issue in issues[:5]:  # Show first 5 issues
                print(f"   - {issue}")
        else:
            valid += 1
            
    print()
    print("=" * 70)
    print(f"Total specs: {total - 3}")  # Exclude README, TEMPLATE, index
    print(f"Valid: {valid}")
    print(f"With issues: {with_issues}")
    print("=" * 70)
    
if __name__ == '__main__':
    main()
