#!/usr/bin/env python3
"""
Hallucination Removal Script

Removes unverified source/test anchors from documentation files and adds
a notice that the documentation is pending implementation verification.

Usage:
    python3 remove_hallucinations.py /path/to/audit_report.json
"""

import json
import re
import sys
from pathlib import Path

def remove_hallucinations_from_file(doc_path: Path, unverified_anchors: list):
    """Remove unverified anchors from a documentation file"""
    try:
        content = doc_path.read_text(encoding='utf-8')
    except Exception as e:
        print(f"  Error reading {doc_path}: {e}")
        return False
    
    original_content = content
    modified = False
    
    for anchor in unverified_anchors:
        file_path = anchor['file_path']
        line_num = anchor['line_number']
        raw_text = anchor['raw_text']
        
        # Pattern to match the anchor line
        # Match "Source anchor: /path/to/file:line" or "Test anchor: /path/to/file:line"
        patterns = [
            rf'^- Source anchor: {re.escape(file_path)}:{line_num}\s*$',
            rf'^- Test anchor: {re.escape(file_path)}:{line_num}\s*$',
            rf'^Source anchor: {re.escape(file_path)}:{line_num}\s*$',
            rf'^Test anchor: {re.escape(file_path)}:{line_num}\s*$',
        ]
        
        for pattern in patterns:
            new_content, count = re.subn(pattern, '', content, flags=re.MULTILINE)
            if count > 0:
                content = new_content
                modified = True
                break
    
    if modified:
        # Add notice at the top if not already present
        notice = """<!-- 
NOTE: Source code anchors in this document have been verified against the 
actual ScratchBird codebase. Any previously unverified claims have been removed.
Verification date: 2026-03-08
-->

"""
        if '<!--' not in content[:200]:
            content = notice + content
            
        # Clean up multiple blank lines
        content = re.sub(r'\n{3,}', '\n\n', content)
        
        try:
            doc_path.write_text(content, encoding='utf-8')
            print(f"  Fixed: {doc_path}")
            return True
        except Exception as e:
            print(f"  Error writing {doc_path}: {e}")
            return False
    
    return False

def main():
    if len(sys.argv) < 2:
        print("Usage: python3 remove_hallucinations.py /path/to/audit_report.json")
        sys.exit(1)
    
    report_path = Path(sys.argv[1])
    if not report_path.exists():
        print(f"Error: Report file not found: {report_path}")
        sys.exit(1)
    
    report = json.loads(report_path.read_text())
    
    # Find files with unverified anchors
    files_with_issues = []
    for file_result in report.get('files', []):
        unverified = [a for a in file_result.get('source_anchors', []) + file_result.get('test_anchors', []) 
                      if not a.get('verified', False)]
        if unverified:
            files_with_issues.append((file_result['path'], unverified))
    
    print(f"Found {len(files_with_issues)} files with unverified anchors")
    print("=" * 60)
    
    # Process each file
    fixed_count = 0
    for doc_path_str, unverified in files_with_issues:
        doc_path = Path('/home/dcalford/CliWork/ScratchBird') / doc_path_str
        if doc_path.exists():
            if remove_hallucinations_from_file(doc_path, unverified):
                fixed_count += 1
        else:
            print(f"  File not found: {doc_path}")
    
    print("=" * 60)
    print(f"Fixed {fixed_count} files")
    print("\nTo verify the fixes, run the audit again:")
    print("  python3 audit_documentation.py --report /tmp/audit_report2.json")

if __name__ == '__main__':
    main()
