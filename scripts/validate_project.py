#!/usr/bin/env python3
# ScratchBird
# Copyright (c) 2025-2026 Dalton Calford
#
# Licensed under the Initial Developer's Public License Version 1.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at:
# https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/

"""
ScratchBird Project Validator
Validates project consistency and identifies issues blocking AI agent implementation.
"""

import os
import sys
import re
import json
from pathlib import Path
from typing import List, Dict, Tuple

class ProjectValidator:
    def __init__(self, root_path: str = "/workspace"):
        self.root = Path(root_path)
        self.errors: List[str] = []
        self.warnings: List[str] = []
        self.info: List[str] = []
        
    def validate_all(self) -> bool:
        """Run all validation checks."""
        print("=" * 70)
        print("ScratchBird Project Validation Report")
        print("=" * 70 + "\n")
        
        # Run all checks
        self.check_planning_consistency()
        self.check_file_references()
        self.check_technical_specifications()
        self.check_build_system()
        self.check_code_consistency()
        self.check_test_coverage()
        
        # Report results
        return self.report_results()
        
    def check_planning_consistency(self):
        """Check for conflicting planning documents."""
        print("Checking planning consistency...")
        
        # Check for authoritative plan
        auth_plan = self.root / "AUTHORITATIVE_IMPLEMENTATION_PLAN.md"
        if not auth_plan.exists():
            self.errors.append("Missing AUTHORITATIVE_IMPLEMENTATION_PLAN.md")
        else:
            self.info.append("✓ Authoritative plan exists")
            
        # Check for deprecated plans still being referenced
        deprecated_plans = [
            "PHASED_IMPLEMENTATION_PLAN.md",
            # Keep awareness but do not require presence
            # "COMPLETE_PHASED_IMPLEMENTATION.md"
        ]
        
        for plan in deprecated_plans:
            plan_path = self.root / "ProjectPlan" / plan
            if plan_path.exists():
                # Check if marked as deprecated
                with open(plan_path) as f:
                    content = f.read()
                    if "DEPRECATED" not in content and "OBSOLETE" not in content:
                        self.warnings.append(f"Plan not marked deprecated: {plan}")
                        
    def check_file_references(self):
        """Verify all documented file paths exist."""
        print("Checking file references...")
        
        # Common incorrect reference patterns
        patterns = [
            (r'/references/technical_specifications/(\w+\.md)', 'technical_specifications'),
            (r'/workspace/references/(\w+\.md)', 'references'),
        ]
        
        # Check PROJECT_STATUS.md
        status_file = self.root / "ProjectPlan" / "PROJECT_STATUS.md"
        if status_file.exists():
            with open(status_file) as f:
                content = f.read()
                
                # Extract file references
                for pattern, expected_dir in patterns:
                    matches = re.findall(pattern, content)
                    for match in matches:
                        # Build correct absolute path for each expected_dir mapping
                        if expected_dir == 'references':
                            # Path like /workspace/references/<file>
                            full_path = self.root / 'references' / match
                        else:
                            # Path like /workspace/references/technical_specifications/<file>
                            full_path = self.root / 'references' / expected_dir / match
                        if not full_path.exists():
                            # Try alternative locations
                            alt_path = self.root / "references" / "technical_specifications" / match
                            if not alt_path.exists():
                                self.errors.append(f"Referenced file not found: {match}")
                            else:
                                self.warnings.append(f"File in wrong location: {match}")
                                
    def check_technical_specifications(self):
        """Check completeness of technical specifications."""
        print("Checking technical specifications...")
        
        required_specs = {
            "ON_DISK_FORMAT.md": ["PageHeader", "checksum", "UUID", "endian"],
            "THREAD_SAFETY.md": ["pthread", "atomic", "lock", "concurrent"],
            "MEMORY_MANAGEMENT.md": ["malloc", "free", "ownership", "lifetime"],
            "ERROR_HANDLING.md": ["error_code", "SB_OK", "SB_ERR", "recovery"],
            "SBLR_BYTECODE_SPECIFICATION.md": ["opcode", "stack", "instruction"],
        }
        
        spec_dir = self.root / "references" / "technical_specifications"
        
        for spec_name, required_terms in required_specs.items():
            spec_path = spec_dir / spec_name
            
            if not spec_path.exists():
                self.errors.append(f"Missing critical specification: {spec_name}")
            else:
                with open(spec_path) as f:
                    content = f.read().lower()
                    
                    # Check for required terms
                    missing_terms = [term for term in required_terms 
                                    if term.lower() not in content]
                    
                    if missing_terms:
                        self.warnings.append(
                            f"{spec_name} missing terms: {', '.join(missing_terms)}"
                        )
                        
                    # Check for TODOs
                    if "todo" in content or "tbd" in content or "fixme" in content:
                        self.warnings.append(f"{spec_name} contains incomplete sections")
                        
    def check_build_system(self):
        """Verify build system completeness."""
        print("Checking build system...")
        
        required_files = [
            ("CMakeLists.txt", ["project", "cmake_minimum_required", "add_subdirectory"]),
            ("src/CMakeLists.txt", ["add_library", "target_"]),
            ("tests/CMakeLists.txt", ["add_executable", "add_test", "gtest"]),
            ("include/scratchbird/version.h", ["VERSION_MAJOR", "VERSION_MINOR"]),
        ]
        
        for file_path, required_content in required_files:
            full_path = self.root / file_path
            
            if not full_path.exists():
                self.errors.append(f"Missing build file: {file_path}")
            else:
                with open(full_path) as f:
                    content = f.read().lower()
                    
                    missing = [req for req in required_content 
                             if req.lower() not in content]
                    
                    if missing:
                        self.warnings.append(
                            f"{file_path} missing: {', '.join(missing)}"
                        )
                        
    def check_code_consistency(self):
        """Check code follows specifications."""
        print("Checking code consistency...")
        
        # Check for Alpha vs Beta features
        src_dir = self.root / "src"
        if src_dir.exists():
            for code_file in src_dir.rglob("*.cpp"):
                with open(code_file) as f:
                    content = f.read()
                    
                    # Check for Beta page sizes in Alpha code
                    if "65536" in content or "131072" in content:
                        self.errors.append(
                            f"Beta page size in Alpha code: {code_file.name}"
                        )
                        
                    # Check for UUID v4 (should be v7)
                    if re.search(r'uuid[_\s]*v4', content, re.IGNORECASE):
                        self.errors.append(
                            f"UUID v4 found (must use v7): {code_file.name}"
                        )
                        
                    # Check for proper error handling
                    if "return -1" in content or "return NULL" in content:
                        if "SB_ERR" not in content:
                            self.warnings.append(
                                f"Non-standard error return: {code_file.name}"
                            )
                            
    def check_test_coverage(self):
        """Check test coverage and structure."""
        print("Checking test coverage...")
        
        test_dir = self.root / "tests"
        if not test_dir.exists():
            self.errors.append("No tests directory")
            return
            
        # Count test files
        test_files = list(test_dir.rglob("test_*.cpp"))
        if len(test_files) < 5:
            self.warnings.append(f"Only {len(test_files)} test files found")
            
        # Check for test categories
        required_test_categories = ["unit", "integration", "benchmark"]
        for category in required_test_categories:
            category_dir = test_dir / category
            if not category_dir.exists():
                self.warnings.append(f"Missing test category: {category}/")
                
    def report_results(self) -> bool:
        """Print validation results and return success status."""
        print("\n" + "=" * 70)
        print("VALIDATION RESULTS")
        print("=" * 70)
        
        # Report errors
        if self.errors:
            print(f"\n❌ CRITICAL ERRORS ({len(self.errors)}):")
            print("-" * 40)
            for i, error in enumerate(self.errors, 1):
                print(f"  {i}. {error}")
                
        # Report warnings
        if self.warnings:
            print(f"\n⚠️  WARNINGS ({len(self.warnings)}):")
            print("-" * 40)
            for i, warning in enumerate(self.warnings, 1):
                print(f"  {i}. {warning}")
                
        # Report info
        if self.info:
            print(f"\n✅ PASSED CHECKS ({len(self.info)}):")
            print("-" * 40)
            for item in self.info:
                print(f"  {item}")
                
        # Summary
        print("\n" + "=" * 70)
        print("SUMMARY")
        print("=" * 70)
        
        total_issues = len(self.errors) + len(self.warnings)
        
        if not self.errors and not self.warnings:
            print("✅ All validation checks passed!")
            print("The project is ready for AI agent implementation.")
            return True
        elif not self.errors:
            print(f"⚠️  Project has {len(self.warnings)} warnings but no critical errors.")
            print("AI agents can proceed but should address warnings.")
            return True
        else:
            print(f"❌ Project has {len(self.errors)} CRITICAL errors that block implementation.")
            print(f"⚠️  Additionally, there are {len(self.warnings)} warnings.")
            print("\nACTION REQUIRED:")
            print("1. Fix all critical errors before proceeding")
            print("2. Run 'python3 scripts/apply_remediation.py' to auto-fix issues")
            print("3. See CRITICAL_REMEDIATION_PLAN.md for detailed fixes")
            return False
            
        print("\n" + "=" * 70)
        
def main():
    """Main entry point."""
    validator = ProjectValidator()
    success = validator.validate_all()
    
    # Exit with appropriate code
    sys.exit(0 if success else 1)
    
if __name__ == "__main__":
    main()