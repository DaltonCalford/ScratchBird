#!/usr/bin/env python3
"""
Hallucination Cleanup Script

Rigorously reviews all specifications and removes/corrects hallucinated content.
Rules:
1. Any spec referencing WAL/XLog must be removed (ScratchBird uses MGA, not WAL)
2. Any spec with line numbers that don't exist in source files must be fixed/removed
3. Any spec describing features not in code must be removed
4. All source anchors must point to actual implementation
"""

import os
import re
import json
from pathlib import Path
from dataclasses import dataclass
from typing import List, Tuple

@dataclass
class SpecFile:
    path: Path
    has_wal_references: bool = False
    has_hallucinated_structures: bool = False
    broken_anchors: List[str] = None
    
    def __post_init__(self):
        if self.broken_anchors is None:
            self.broken_anchors = []

class HallucinationCleaner:
    def __init__(self, project_root: Path):
        self.project_root = project_root
        self.specs_root = project_root / "docs/documentation/developers_guide/specifications"
        self.src_root = project_root / "src"
        
        # Directories that don't exist (hallucinated structure)
        self.nonexistent_dirs = [
            "storage",  # No src/storage/ directory
        ]
        
        # Prohibited concepts
        self.prohibited_patterns = [
            (r'\bWAL\b', "Write-Ahead Logging - ScratchBird uses MGA"),
            (r'\bxlog\b', "XLog - PostgreSQL concept, not used"),
            (r'\bpg_xlog\b', "pg_xlog - PostgreSQL directory"),
            (r'\bclog\b', "CLog - PostgreSQL concept"),
            (r'\bsubtransaction\b', "Check if actually implemented"),
            (r'\btwo.phase.commit\b', "Check if prepared transactions exist"),
        ]
        
        self.specs_to_remove = []
        self.specs_to_fix = []
        
    def check_source_anchor(self, file_path: str, line_num: int) -> bool:
        """Check if a source anchor points to a real file with that line number"""
        # Clean up path
        if '/ScratchBird/' in file_path:
            file_path = file_path.split('/ScratchBird/')[-1]
        file_path = file_path.lstrip('/')
        
        full_path = self.project_root / file_path
        if not full_path.exists():
            return False
            
        # Check line number exists
        try:
            with open(full_path, 'r') as f:
                lines = f.readlines()
                return line_num <= len(lines)
        except:
            return False
            
    def analyze_spec(self, spec_path: Path) -> SpecFile:
        """Analyze a specification file for hallucinations"""
        spec = SpecFile(path=spec_path)
        
        try:
            content = spec_path.read_text()
        except:
            return spec
            
        # Check for prohibited concepts
        for pattern, reason in self.prohibited_patterns:
            if re.search(pattern, content, re.IGNORECASE):
                spec.has_wal_references = True
                print(f"  ❌ WAL reference found: {spec_path.name}")
                break
                
        # Check for source anchors
        anchor_pattern = r'`(/home/dcalford/CliWork/ScratchBird/[^`]+):(\d+)`'
        for match in re.finditer(anchor_pattern, content):
            file_path = match.group(1)
            line_num = int(match.group(2))
            if not self.check_source_anchor(file_path, line_num):
                spec.broken_anchors.append(f"{file_path}:{line_num}")
                
        # Check if describing non-existent directories
        relative = spec_path.relative_to(self.specs_root)
        first_part = str(relative).split('/')[0]
        if first_part in self.nonexistent_dirs:
            spec.has_hallucinated_structures = True
            print(f"  ❌ Hallucinated directory: {spec_path.name} (in {first_part}/)")
            
        return spec
        
    def run_cleanup(self):
        """Run the full cleanup process"""
        print("=" * 70)
        print("HALLUCINATION CLEANUP - ScratchBird Specifications")
        print("=" * 70)
        print(f"Project root: {self.project_root}")
        print(f"Specs root: {self.specs_root}")
        print()
        
        if not self.specs_root.exists():
            print("ERROR: Specifications directory not found")
            return
            
        # Find all spec files
        spec_files = list(self.specs_root.rglob("*.md"))
        print(f"Found {len(spec_files)} specification files to analyze")
        print()
        
        # Analyze each spec
        print("Analyzing specifications...")
        print("-" * 70)
        
        for spec_path in sorted(spec_files):
            if spec_path.name in ['README.md', 'TEMPLATE.md', 'index.md']:
                continue
                
            spec = self.analyze_spec(spec_path)
            
            if spec.has_wal_references or spec.has_hallucinated_structures:
                self.specs_to_remove.append(spec)
            elif spec.broken_anchors:
                self.specs_to_fix.append(spec)
                
        print()
        print("=" * 70)
        print("CLEANUP SUMMARY")
        print("=" * 70)
        
        print(f"\n🔴 Specs to REMOVE (hallucinated): {len(self.specs_to_remove)}")
        for spec in self.specs_to_remove:
            reason = []
            if spec.has_wal_references:
                reason.append("WAL references")
            if spec.has_hallucinated_structures:
                reason.append("non-existent directory")
            print(f"  - {spec.path.relative_to(self.specs_root)} ({', '.join(reason)})")
            
        print(f"\n🟡 Specs to FIX (broken anchors): {len(self.specs_to_fix)}")
        for spec in self.specs_to_fix:
            print(f"  - {spec.path.relative_to(self.specs_root)}")
            for anchor in spec.broken_anchors[:5]:  # Show first 5
                print(f"      Broken: {anchor}")
                
        return self.specs_to_remove, self.specs_to_fix
        
    def execute_removal(self, dry_run=True):
        """Execute the removal of hallucinated specs"""
        print()
        print("=" * 70)
        if dry_run:
            print("DRY RUN - No files will be removed")
        else:
            print("EXECUTING REMOVAL")
        print("=" * 70)
        
        for spec in self.specs_to_remove:
            rel_path = spec.path.relative_to(self.project_root)
            print(f"\nRemoving: {rel_path}")
            
            if not dry_run:
                try:
                    spec.path.unlink()
                    print("  ✅ Removed")
                except Exception as e:
                    print(f"  ❌ Error: {e}")
                    
def main():
    import argparse
    parser = argparse.ArgumentParser(description='Clean up hallucinated specifications')
    parser.add_argument('--execute', action='store_true', help='Actually remove files (default: dry run)')
    parser.add_argument('--project-root', type=str, default='/home/dcalford/CliWork/ScratchBird')
    args = parser.parse_args()
    
    cleaner = HallucinationCleaner(Path(args.project_root))
    to_remove, to_fix = cleaner.run_cleanup()
    
    if to_remove or to_fix:
        cleaner.execute_removal(dry_run=not args.execute)
        
        print()
        if not args.execute:
            print("This was a DRY RUN. Use --execute to actually remove files.")
    else:
        print("\n✅ No hallucinated content found!")
        
if __name__ == '__main__':
    main()
