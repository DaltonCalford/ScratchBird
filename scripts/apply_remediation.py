#!/usr/bin/env python3
"""
ScratchBird Automatic Remediation Script
Applies fixes for common issues identified by validation.
"""

import os
import sys
import shutil
import re
from pathlib import Path
from typing import List, Dict

class RemediationApplier:
    def __init__(self, root_path: str = "/workspace"):
        self.root = Path(root_path)
        self.fixes_applied = []
        self.fixes_failed = []
        
    def apply_all_fixes(self) -> bool:
        """Apply all automatic fixes."""
        print("=" * 70)
        print("ScratchBird Automatic Remediation")
        print("=" * 70 + "\n")
        
        # Apply fixes in order of importance
        self.fix_missing_directories()
        self.fix_file_references()
        self.create_missing_specs()
        self.mark_deprecated_plans()
        self.create_build_files()
        
        # Report results
        return self.report_results()
        
    def fix_missing_directories(self):
        """Create missing directory structure."""
        print("Creating missing directories...")
        
        required_dirs = [
            "src",
            "include/scratchbird",
            "tests/unit",
            "tests/integration", 
            "tests/benchmark",
            "benchmarks",
            "docs/implementation",
            "scripts",
            ".github/workflows",
            "references/technical_specifications/archived"
        ]
        
        for dir_path in required_dirs:
            full_path = self.root / dir_path
            if not full_path.exists():
                full_path.mkdir(parents=True, exist_ok=True)
                self.fixes_applied.append(f"Created directory: {dir_path}")
                
    def fix_file_references(self):
        """Fix incorrect file path references."""
        print("Fixing file references...")
        
        # Fix PROJECT_STATUS.md references
        status_file = self.root / "ProjectPlan" / "PROJECT_STATUS.md"
        if status_file.exists():
            with open(status_file, 'r') as f:
                content = f.read()
                
            original = content
            
            # Fix common path errors
            replacements = [
                # Move specs to correct location
                (r'/references/technical_specifications/BLR_SPECIFICATION\.md',
                 '/references/technical_specifications/archived/BLR_SPECIFICATION.md'),
                (r'/references/technical_specifications/BLR_ADVANCED_FEATURES\.md',
                 '/references/technical_specifications/archived/BLR_ADVANCED_FEATURES.md'),
                # Update to SBLR
                (r'BLR Specification', 'SBLR Bytecode Specification'),
                (r'BLR_SPECIFICATION\.md', 'SBLR_BYTECODE_SPECIFICATION.md'),
            ]
            
            for pattern, replacement in replacements:
                content = re.sub(pattern, replacement, content)
                
            if content != original:
                with open(status_file, 'w') as f:
                    f.write(content)
                self.fixes_applied.append("Fixed path references in PROJECT_STATUS.md")
                
    def create_missing_specs(self):
        """Create stub files for missing specifications."""
        print("Creating missing specification stubs...")
        
        specs_to_create = {
            "THREAD_SAFETY.md": self._thread_safety_template(),
            "MEMORY_MANAGEMENT.md": self._memory_management_template(),
            "ERROR_HANDLING.md": self._error_handling_template(),
        }
        
        spec_dir = self.root / "references" / "technical_specifications"
        
        for spec_name, content in specs_to_create.items():
            spec_path = spec_dir / spec_name
            
            if not spec_path.exists():
                with open(spec_path, 'w') as f:
                    f.write(content)
                self.fixes_applied.append(f"Created specification: {spec_name}")
                
    def mark_deprecated_plans(self):
        """Mark old planning documents as deprecated."""
        print("Marking deprecated plans...")
        
        deprecated_files = [
            "ProjectPlan/PHASED_IMPLEMENTATION_PLAN.md",
            # Deprecated; kept for reference in archive only
            # "ProjectPlan/COMPLETE_PHASED_IMPLEMENTATION.md",
        ]
        
        for file_path in deprecated_files:
            full_path = self.root / file_path
            
            if full_path.exists():
                with open(full_path, 'r') as f:
                    content = f.read()
                    
                if "DEPRECATED" not in content:
                    # Add deprecation notice at top
                    deprecation = """# ⚠️ DEPRECATED DOCUMENT ⚠️
## This document is OBSOLETE. Use AUTHORITATIVE_IMPLEMENTATION_PLAN.md instead.

---

"""
                    content = deprecation + content
                    
                    with open(full_path, 'w') as f:
                        f.write(content)
                    
                    self.fixes_applied.append(f"Marked as deprecated: {file_path}")
                    
    def create_build_files(self):
        """Create missing build system files."""
        print("Creating build system files...")
        
        # Create root CMakeLists.txt
        root_cmake = self.root / "CMakeLists.txt"
        if not root_cmake.exists():
            with open(root_cmake, 'w') as f:
                f.write(self._root_cmake_template())
            self.fixes_applied.append("Created root CMakeLists.txt")
            
        # Create src/CMakeLists.txt
        src_cmake = self.root / "src" / "CMakeLists.txt"
        if not src_cmake.exists():
            with open(src_cmake, 'w') as f:
                f.write(self._src_cmake_template())
            self.fixes_applied.append("Created src/CMakeLists.txt")
            
        # Create tests/CMakeLists.txt
        tests_cmake = self.root / "tests" / "CMakeLists.txt"
        if not tests_cmake.exists():
            with open(tests_cmake, 'w') as f:
                f.write(self._tests_cmake_template())
            self.fixes_applied.append("Created tests/CMakeLists.txt")
            
        # Create version.h
        version_h = self.root / "include" / "scratchbird" / "version.h"
        if not version_h.exists():
            with open(version_h, 'w') as f:
                f.write(self._version_h_template())
            self.fixes_applied.append("Created include/scratchbird/version.h")
            
    def report_results(self) -> bool:
        """Report remediation results."""
        print("\n" + "=" * 70)
        print("REMEDIATION RESULTS")
        print("=" * 70)
        
        if self.fixes_applied:
            print(f"\n✅ FIXES APPLIED ({len(self.fixes_applied)}):")
            print("-" * 40)
            for fix in self.fixes_applied:
                print(f"  • {fix}")
                
        if self.fixes_failed:
            print(f"\n❌ FIXES FAILED ({len(self.fixes_failed)}):")
            print("-" * 40)
            for failure in self.fixes_failed:
                print(f"  • {failure}")
                
        print("\n" + "=" * 70)
        print("NEXT STEPS")
        print("=" * 70)
        
        if not self.fixes_failed:
            print("✅ All automatic fixes applied successfully!")
            print("\n1. Run validation again: python3 scripts/validate_project.py")
            print("2. Review CRITICAL_REMEDIATION_PLAN.md for manual fixes")
            print("3. Commit changes: git add -A && git commit -m 'Apply automatic fixes'")
            return True
        else:
            print("⚠️  Some fixes could not be applied automatically.")
            print("\nManual intervention required for:")
            for failure in self.fixes_failed:
                print(f"  • {failure}")
            return False
            
    # Template methods for file creation
    
    def _thread_safety_template(self) -> str:
        return """# Thread Safety Specification

## Thread Safety Levels

### Level 1: Immutable
No synchronization needed after initialization.

### Level 2: Thread-Local
No sharing between threads.

### Level 3: Read-Write Lock
Multiple readers, single writer.

### Level 4: Mutex Protected
Exclusive access required.

### Level 5: Lock-Free
Atomic operations only.

## Implementation Guidelines

Every structure must declare its thread safety level in comments.

```c
typedef struct Example {
    // THREAD SAFETY: Level 3 - Read-Write Lock
    pthread_rwlock_t lock;
    int data;
} Example;
```

## TODO: Complete specification
- Add specific component thread safety requirements
- Define locking order to prevent deadlocks
- Add performance considerations
"""

    def _memory_management_template(self) -> str:
        return """# Memory Management Specification

## Ownership Rules

### Rule 1: Creator Owns
Whoever allocates memory is responsible for freeing it.

### Rule 2: Transfer Explicitly
Use naming conventions to indicate ownership transfer.

### Rule 3: Output Parameters
Functions returning via output parameters allocate; caller frees.

### Rule 4: Const Never Transfers
Const parameters never transfer ownership.

## Patterns

```c
// Caller owns returned memory
char* create_string(const char* input);  // Caller must free()

// Function owns parameter
void consume_buffer(char* buffer);  // Function will free()

// Borrowed reference
const char* get_name(void);  // Caller must NOT free()
```

## TODO: Complete specification
- Add memory pool design
- Define allocation failure handling
- Add leak detection strategy
"""

    def _error_handling_template(self) -> str:
        return """# Error Handling Specification

## Error Codes

```c
typedef enum sb_error {
    SB_OK = 0,
    
    // File errors (1000-1999)
    SB_ERR_FILE_NOT_FOUND = 1001,
    SB_ERR_FILE_EXISTS = 1002,
    SB_ERR_IO_ERROR = 1003,
    
    // Page errors (2000-2999)
    SB_ERR_PAGE_CORRUPT = 2001,
    SB_ERR_CHECKSUM_MISMATCH = 2002,
    
    // Transaction errors (3000-3999)
    SB_ERR_DEADLOCK = 3001,
    SB_ERR_LOCK_TIMEOUT = 3002,
    
    // TODO: Add more error codes
} sb_error_t;
```

## Error Handling Patterns

```c
#define RETURN_IF_ERROR(expr) \\
    do { \\
        sb_error_t err = (expr); \\
        if (err != SB_OK) return err; \\
    } while(0)
```

## TODO: Complete specification
- Add error context structure
- Define error propagation rules
- Add recovery strategies
"""

    def _root_cmake_template(self) -> str:
        return """cmake_minimum_required(VERSION 3.20)
project(ScratchBird VERSION 0.1.0 LANGUAGES C CXX)

# C++ Standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)

# Options
option(BUILD_TESTS "Build test suite" ON)
option(BUILD_BENCHMARKS "Build benchmarks" OFF)
option(ENABLE_ASAN "Enable AddressSanitizer" OFF)

# Compiler flags
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Subdirectories
add_subdirectory(src)

if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()
"""

    def _src_cmake_template(self) -> str:
        return """# ScratchBird Core Library

# Collect source files
file(GLOB_RECURSE SOURCES "*.cpp" "*.c")

# Create library
add_library(scratchbird_core ${SOURCES})

# Include directories
target_include_directories(scratchbird_core PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

# Link libraries
target_link_libraries(scratchbird_core PUBLIC
    pthread
    m
)

# Installation
install(TARGETS scratchbird_core
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)
"""

    def _tests_cmake_template(self) -> str:
        return """# Test Suite

# Find or fetch GoogleTest
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
)
FetchContent_MakeAvailable(googletest)

# Test sources
file(GLOB TEST_SOURCES "*.cpp" "unit/*.cpp" "integration/*.cpp")

# Test executable
add_executable(scratchbird_tests ${TEST_SOURCES})

# Link libraries
target_link_libraries(scratchbird_tests
    scratchbird_core
    GTest::gtest
    GTest::gtest_main
)

# Register tests
include(GoogleTest)
gtest_discover_tests(scratchbird_tests)
"""

    def _version_h_template(self) -> str:
        return """#ifndef SCRATCHBIRD_VERSION_H
#define SCRATCHBIRD_VERSION_H

#define SCRATCHBIRD_VERSION_MAJOR 0
#define SCRATCHBIRD_VERSION_MINOR 1
#define SCRATCHBIRD_VERSION_PATCH 0
#define SCRATCHBIRD_VERSION_SUFFIX "alpha.1.01"

#define SCRATCHBIRD_VERSION_STRING "0.1.0-alpha.1.01"

#endif // SCRATCHBIRD_VERSION_H
"""

def main():
    """Main entry point."""
    applier = RemediationApplier()
    success = applier.apply_all_fixes()
    
    sys.exit(0 if success else 1)
    
if __name__ == "__main__":
    main()