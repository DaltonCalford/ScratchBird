# Build System and Directory Structure

## ⚠️ MANDATORY REFERENCE
**BEFORE ANY IMPLEMENTATION**: You MUST read `references/CODING_AND_BUILD_STANDARDS.md`

**AI DEVELOPERS**: Start every session by confirming you have read the coding standards.

## Current Project State

**Phase**: Planning Complete, Ready for Alpha Implementation
**Next Task**: Alpha 1.01.1 - Database file creation

## Directory Layout

```
.
├── CMakeLists.txt           # Root CMake configuration
├── include/                 # Public headers
│   └── scratchbird/
│       └── scratchbird.h    # Main C API header
├── src/                     # Source code
│   ├── CMakeLists.txt
│   ├── main.cpp             # Minimal entry point
│   ├── api/                 # C API implementation (TO CREATE)
│   ├── engine/              # Core engine (TO CREATE)
│   ├── parser/              # SQL parsers (TO CREATE)
│   └── yvalve/              # Y-Valve router (TO CREATE)
├── tests/                   # Test suites
│   ├── CMakeLists.txt
│   ├── api/                 # API tests (TO CREATE)
│   └── engine/              # Engine tests (TO CREATE)
├── references/              # Technical specifications ✅ COMPLETE
│   ├── wire_protocols/      # Protocol specs
│   ├── data_types/          # Type system
│   ├── technical_specifications/  # Core specs
│   └── CODING_AND_BUILD_STANDARDS.md
├── ProjectPlan/             # Project planning ✅ ORGANIZED
│   ├── PROJECT_STATUS.md    # Current status
│   ├── AUTHORITATIVE_IMPLEMENTATION_PLAN.md  # Single source of truth
│   ├── Phase_*.md           # Individual phases
│   ├── progress/            # Progress tracking
│   │   ├── implementation.log
│   │   └── test.log
│   └── archive/             # Old versions
└── docs/                    # Architecture docs
    ├── architecture/        # ADRs
    ├── specifications/      # Feature specs
    └── compatibility/       # Compatibility guides
```

## Build Commands

### Standard Build
```bash
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
make -j$(nproc)
```

### Test Build
```bash
# In build directory
make test
# Or run directly
./tests/scratchbird_tests
```

### All Page Sizes Test
```bash
# Tests MUST verify all page sizes
for size in 8K 16K 32K; do
    ./tests/test_database_creation --page-size=$size
done
```

## CMake Configuration

### Required CMake Version
```cmake
cmake_minimum_required(VERSION 3.20)
```

### C++ Standard
```cmake
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
```

### Compiler Flags
```cmake
# Debug build
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -Wall -Wextra -Wpedantic")

# Release build  
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

# Sanitizers for testing
set(CMAKE_CXX_FLAGS_ASAN "-fsanitize=address -fno-omit-frame-pointer")
```

## Implementation Checklist

### For Each Phase

- [ ] Read CODING_AND_BUILD_STANDARDS.md
- [ ] Read phase specification
- [ ] Implement C API functions
- [ ] Create unit tests
- [ ] Test all 5 page sizes
- [ ] Run sanitizers
- [ ] Update progress log
- [ ] Commit with descriptive message

### Progress Tracking

```bash
# Log implementation progress
echo "2024-01-15 14:00 1.01.1 IMPLEMENTED sb_create_database for 8K pages" >> progress/implementation.log

# Log test progress
echo "2024-01-15 14:30 1.01.1 TESTED sb_create_database 8K PASS" >> progress/test.log
```

## Critical Requirements

1. **Page Sizes**: Alpha MUST support 8K, 16K, 32K (64K/128K in Beta)
2. **Thread Safety**: All API functions must be thread-safe
3. **Error Handling**: Rich error information with chains
4. **Memory Management**: No leaks, use RAII
5. **Testing**: 100% coverage of public API

## First Implementation Tasks

### Alpha 1.01.1 - Database File Creation

1. Create `src/api/core.c` with:
   - `sb_init()`
   - `sb_version()`
   - `sb_shutdown()`

2. Create `src/api/database.c` with:
   - `sb_create_database()`
   - Database file structure initialization

3. Create `tests/api/test_database_creation.c` with:
   - Test all 5 page sizes
   - Verify file structure
   - Check header validity

## Common Pitfalls to Avoid

1. **DON'T** implement without reading specs
2. **DON'T** skip testing any page size
3. **DON'T** use raw pointers without RAII
4. **DON'T** assume single-threaded access
5. **DON'T** ignore compiler warnings
6. **DON'T** commit without tests passing

## Getting Help

1. Check `/references/technical_specifications/` for specs
2. Review `/docs/architecture/` for design decisions
3. Look at `PROJECT_STATUS.md` for current state
4. Check progress logs for what's been done

---

**Remember**: Quality over speed. Each component must be solid before moving on.