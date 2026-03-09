#!/usr/bin/env python3
"""
Remove specifications without proper source anchors
"""

import os
import re
from pathlib import Path

def count_source_anchors(spec_path):
    """Count source anchors in a spec file"""
    try:
        with open(spec_path, 'r') as f:
            content = f.read()
        
        # Count source anchors (not test anchors)
        source_pattern = r'Source anchor:\s*`([^`]+)`'
        matches = re.findall(source_pattern, content)
        return len(matches)
    except:
        return 0

def main():
    project_root = Path('/home/dcalford/CliWork/ScratchBird')
    specs_root = project_root / 'docs/documentation/developers_guide/specifications'
    
    print("=" * 70)
    print("REMOVING UNANCHORED SPECIFICATIONS")
    print("=" * 70)
    print()
    
    spec_files = list(specs_root.rglob('*.md'))
    to_remove = []
    
    for spec_path in sorted(spec_files):
        if spec_path.name in ['README.md', 'TEMPLATE.md', 'index.md']:
            continue
            
        count = count_source_anchors(spec_path)
        if count < 2:
            rel_path = spec_path.relative_to(specs_root)
            to_remove.append((spec_path, rel_path, count))
            
    print(f"Found {len(to_remove)} specs with fewer than 2 source anchors:")
    print()
    
    for spec_path, rel_path, count in to_remove:
        print(f"  {rel_path}: {count} source anchors")
        
    print()
    print("=" * 70)
    
    # Remove the files
    for spec_path, rel_path, count in to_remove:
        try:
            spec_path.unlink()
            print(f"Removed: {rel_path}")
        except Exception as e:
            print(f"Error removing {rel_path}: {e}")
            
    # Clean up empty directories
    for dir_path in sorted(specs_root.rglob('*'), reverse=True):
        if dir_path.is_dir():
            try:
                # Check if directory is empty
                if not any(dir_path.iterdir()):
                    dir_path.rmdir()
                    print(f"Removed empty directory: {dir_path.relative_to(specs_root)}")
            except:
                pass
                
    print()
    print(f"Removed {len(to_remove)} unanchored specifications")
    
if __name__ == '__main__':
    main()
