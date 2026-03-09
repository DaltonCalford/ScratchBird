#!/usr/bin/env python3
"""
Documentation Integrity Audit Script

Verifies that all source code anchors in documentation files point to actual
implemented code in the ScratchBird codebase.

Usage:
    python3 audit_documentation.py [--fix] [--report]
    
Options:
    --fix      Attempt to fix broken anchors by removing unverified claims
    --report   Generate a detailed JSON report of all findings
"""

import os
import re
import json
import argparse
from pathlib import Path
from dataclasses import dataclass, field, asdict
from typing import List, Dict, Set, Tuple
from datetime import datetime

@dataclass
class SourceAnchor:
    file_path: str
    line_number: int
    raw_text: str
    verified: bool = False
    
@dataclass
class DocumentationFile:
    path: str
    title: str = ""
    source_anchors: List[SourceAnchor] = field(default_factory=list)
    test_anchors: List[SourceAnchor] = field(default_factory=list)
    unverified_claims: List[str] = field(default_factory=list)

class DocumentationAuditor:
    def __init__(self, project_root: Path):
        self.project_root = project_root
        self.docs_root = project_root / "docs" / "documentation"
        self.src_root = project_root / "src"
        self.tests_root = project_root / "tests"
        
        self.source_files_cache: Set[str] = set()
        self.test_files_cache: Set[str] = set()
        
        self.results: List[DocumentationFile] = []
        self.verified_count = 0
        self.unverified_count = 0
        
    def _build_file_cache(self):
        """Build cache of all source and test files for fast lookup"""
        print("Building source file cache...")
        
        if self.src_root.exists():
            for f in self.src_root.rglob("*"):
                if f.is_file() and f.suffix in {'.cpp', '.h', '.hpp', '.c'}:
                    self.source_files_cache.add(str(f.relative_to(self.project_root)))
                    
        if self.tests_root.exists():
            for f in self.tests_root.rglob("*"):
                if f.is_file() and f.suffix in {'.cpp', '.h', '.hpp', '.c'}:
                    self.test_files_cache.add(str(f.relative_to(self.project_root)))
                    
        print(f"  Found {len(self.source_files_cache)} source files")
        print(f"  Found {len(self.test_files_cache)} test files")
        
    def _extract_anchors(self, content: str) -> Tuple[List[SourceAnchor], List[SourceAnchor], List[str]]:
        """Extract source and test anchors from markdown content"""
        source_anchors = []
        test_anchors = []
        unverified_claims = []
        
        # Pattern for source anchors: src/... or full paths with :line_number
        # Matches patterns like:
        # - `src/parser/parser_v3.cpp:123`
        # - Source: `/home/.../src/parser/parser_v3.cpp:1`
        # - [source](src/...)
        
        source_patterns = [
            # Markdown code blocks with path
            r'`((?:src|source)/[^`]+\.(?:cpp|h|hpp|c)):(\d+)`',
            # Source: prefix
            r'(?:Source|source):\s*`?([^`\n]+/(?:src|source)/[^`\n]+\.(?:cpp|h|hpp|c)):(\d+)`?',
            # Source anchor: prefix (our actual format)
            r'(?:Source anchor|source anchor):\s*(.+?\.(?:cpp|h|hpp|c)):(\d+)',
            # Link patterns
            r'\[([^\]]+)\]\(((?:src|source)/[^)]+\.(?:cpp|h|hpp|c))\)',
        ]
        
        test_patterns = [
            # Test file references
            r'`((?:tests?)/[^`]+\.(?:cpp|h|hpp|c)):(\d+)`',
            # Test: prefix
            r'(?:Test|test):\s*`?([^`\n]+/(?:tests?)/[^`\n]+\.(?:cpp|h|hpp|c)):(\d+)`?',
            # Test anchor: prefix (our actual format)
            r'(?:Test anchor|test anchor):\s*(.+?\.(?:cpp|h|hpp|c)):(\d+)',
        ]
        
        # Look for unverified claims - statements about implementation without anchors
        unverified_patterns = [
            r'(?:The|This) implementation\s+[^.]+\.(?:\s+It\s+[^.]+\.)?',
            r'Implemented in\s+[^.]+\.',
            r'See\s+`?src/[^`]+`?\s+for',
            r'Refer to\s+`?src/[^`]+`?',
        ]
        
        for pattern in source_patterns:
            for match in re.finditer(pattern, content, re.IGNORECASE):
                if len(match.groups()) >= 2:
                    file_path = match.group(1)
                    try:
                        line_num = int(match.group(2))
                    except (ValueError, IndexError):
                        line_num = 1
                    source_anchors.append(SourceAnchor(
                        file_path=file_path,
                        line_number=line_num,
                        raw_text=match.group(0),
                        verified=False
                    ))
                    
        for pattern in test_patterns:
            for match in re.finditer(pattern, content, re.IGNORECASE):
                if len(match.groups()) >= 2:
                    file_path = match.group(1)
                    try:
                        line_num = int(match.group(2))
                    except (ValueError, IndexError):
                        line_num = 1
                    test_anchors.append(SourceAnchor(
                        file_path=file_path,
                        line_number=line_num,
                        raw_text=match.group(0),
                        verified=False
                    ))
                    
        return source_anchors, test_anchors, unverified_claims
        
    def _verify_anchor(self, anchor: SourceAnchor, is_test: bool = False) -> bool:
        """Verify if a source anchor points to an existing file"""
        # Clean up the path - remove any absolute prefix
        file_path = anchor.file_path
        
        # Remove absolute path prefix if present
        if '/ScratchBird/' in file_path:
            file_path = file_path.split('/ScratchBird/')[-1]
            
        # Check if it's in our cache
        cache = self.test_files_cache if is_test else self.source_files_cache
        
        if file_path in cache:
            anchor.verified = True
            return True
            
        # Try alternative path formats
        alt_paths = [
            file_path.lstrip('/'),
            file_path.replace('/home/dcalford/CliWork/ScratchBird/', ''),
        ]
        
        for alt in alt_paths:
            if alt in cache:
                anchor.file_path = alt
                anchor.verified = True
                return True
                
        # Check if file exists on disk
        full_path = self.project_root / file_path.lstrip('/')
        if full_path.exists():
            anchor.verified = True
            return True
            
        return False
        
    def audit_file(self, doc_path: Path) -> DocumentationFile:
        """Audit a single documentation file"""
        result = DocumentationFile(path=str(doc_path.relative_to(self.project_root)))
        
        try:
            content = doc_path.read_text(encoding='utf-8')
        except Exception as e:
            print(f"  Error reading {doc_path}: {e}")
            return result
            
        # Extract title
        title_match = re.search(r'^#\s+(.+)$', content, re.MULTILINE)
        if title_match:
            result.title = title_match.group(1)
            
        # Extract anchors
        source_anchors, test_anchors, unverified = self._extract_anchors(content)
        result.source_anchors = source_anchors
        result.test_anchors = test_anchors
        result.unverified_claims = unverified
        
        # Verify anchors
        for anchor in result.source_anchors:
            if self._verify_anchor(anchor, is_test=False):
                self.verified_count += 1
            else:
                self.unverified_count += 1
                
        for anchor in result.test_anchors:
            if self._verify_anchor(anchor, is_test=True):
                self.verified_count += 1
            else:
                self.unverified_count += 1
                
        return result
        
    def run_audit(self) -> Dict:
        """Run the full documentation audit"""
        print(f"\n{'='*60}")
        print("DOCUMENTATION INTEGRITY AUDIT")
        print(f"{'='*60}")
        print(f"Project root: {self.project_root}")
        print(f"Documentation root: {self.docs_root}")
        print(f"Timestamp: {datetime.now().isoformat()}")
        print()
        
        self._build_file_cache()
        
        if not self.docs_root.exists():
            print(f"ERROR: Documentation directory not found: {self.docs_root}")
            return {}
            
        # Find all markdown files
        md_files = list(self.docs_root.rglob("*.md"))
        print(f"\nFound {len(md_files)} documentation files")
        print()
        
        # Audit each file
        for i, md_file in enumerate(sorted(md_files), 1):
            print(f"[{i}/{len(md_files)}] Auditing: {md_file.relative_to(self.docs_root)}")
            result = self.audit_file(md_file)
            self.results.append(result)
            
            if result.source_anchors or result.test_anchors:
                verified = sum(1 for a in result.source_anchors + result.test_anchors if a.verified)
                total = len(result.source_anchors) + len(result.test_anchors)
                print(f"    Found {len(result.source_anchors)} source, {len(result.test_anchors)} test anchors")
                print(f"    Verified: {verified}/{total}")
                
        return self._generate_report()
        
    def _generate_report(self) -> Dict:
        """Generate audit report"""
        total_files = len(self.results)
        files_with_anchors = sum(1 for r in self.results if r.source_anchors or r.test_anchors)
        files_with_issues = sum(1 for r in self.results 
                              if any(not a.verified for a in r.source_anchors + r.test_anchors))
        
        report = {
            'timestamp': datetime.now().isoformat(),
            'summary': {
                'total_files': total_files,
                'files_with_anchors': files_with_anchors,
                'files_with_issues': files_with_issues,
                'total_source_anchors': sum(len(r.source_anchors) for r in self.results),
                'total_test_anchors': sum(len(r.test_anchors) for r in self.results),
                'verified_anchors': self.verified_count,
                'unverified_anchors': self.unverified_count,
                'verification_rate': round(self.verified_count / max(1, self.verified_count + self.unverified_count) * 100, 2)
            },
            'files': [asdict(r) for r in self.results]
        }
        
        return report
        
    def print_summary(self, report: Dict):
        """Print audit summary"""
        summary = report.get('summary', {})
        
        print(f"\n{'='*60}")
        print("AUDIT SUMMARY")
        print(f"{'='*60}")
        print(f"Total documentation files:      {summary.get('total_files', 0)}")
        print(f"Files with source anchors:      {summary.get('files_with_anchors', 0)}")
        print(f"Files with unverified anchors:  {summary.get('files_with_issues', 0)}")
        print()
        print(f"Total source anchors:           {summary.get('total_source_anchors', 0)}")
        print(f"Total test anchors:             {summary.get('total_test_anchors', 0)}")
        print()
        print(f"Verified anchors:               {summary.get('verified_anchors', 0)}")
        print(f"Unverified anchors:             {summary.get('unverified_anchors', 0)}")
        print(f"Verification rate:              {summary.get('verification_rate', 0)}%")
        print(f"{'='*60}")
        
        # Print files with issues
        if summary.get('files_with_issues', 0) > 0:
            print("\nFILES WITH UNVERIFIED ANCHORS:")
            print("-" * 60)
            for file_result in self.results:
                unverified = [a for a in file_result.source_anchors + file_result.test_anchors if not a.verified]
                if unverified:
                    print(f"\n{file_result.path}")
                    for anchor in unverified:
                        print(f"  ✗ {anchor.file_path}:{anchor.line_number}")

def main():
    parser = argparse.ArgumentParser(description='Audit documentation source anchors')
    parser.add_argument('--fix', action='store_true', help='Fix broken anchors')
    parser.add_argument('--report', type=str, help='Save JSON report to file')
    parser.add_argument('--project-root', type=str, default='/home/dcalford/CliWork/ScratchBird',
                        help='Path to ScratchBird project root')
    
    args = parser.parse_args()
    
    project_root = Path(args.project_root)
    auditor = DocumentationAuditor(project_root)
    report = auditor.run_audit()
    auditor.print_summary(report)
    
    if args.report:
        report_path = Path(args.report)
        report_path.write_text(json.dumps(report, indent=2))
        print(f"\nDetailed report saved to: {report_path}")
        
    return 0 if report.get('summary', {}).get('unverified_anchors', 0) == 0 else 1

if __name__ == '__main__':
    exit(main())
