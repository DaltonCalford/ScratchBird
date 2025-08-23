# Claude Memory File for ScratchBird Project

## Project Identity

**ScratchBird** is a SQL database engine that has successfully completed its Alpha to Beta transition. **Phase 8: PSQL Runtime Engine is 100% COMPLETE** - world-class procedural SQL platform implemented (EXECUTE BLOCK, stored procedures, functions, control flow, cursors, exceptions, debugging, development tools, performance optimizations). **Now transitioning to Phase 9: Index Families and Advanced Options** - implementing Hash, Bitmap, GIN, and R-Tree indexes beyond current B-Tree implementation.

## Directory Structure (CRITICAL - Remember This!)

```
ScratchBird/
├── build/                    # 🔴 ALL compiled executables go here (CMake builds)
├── tests/                    # 🔴 ALL test source files (.cpp) go here
├── src/engine/               # Core database engine source
├── include/scratchbird/      # Header files
├── ProjectPlan/              # 🔴 CHECK THIS FIRST - Project documentation
│   ├── 2ndTry_Phase_Review.md  # 📋 Gap analysis - missing features to implement
│   ├── BuildSystem.md        # Complete build system rules
│   ├── claude_todo.md        # Implementation status
│   └── [Phase planning docs]
└── CMakeLists.txt            # Build configuration
```

## 🚨 CRITICAL INSTRUCTIONS 🚨

**IF CONTEXT IS COMPACTED - DO THIS FIRST:**

1. **Read `ProjectPlan/Phase_9_Index_Families_TODO.md`** - Current development focus (Index families)
2. **Build verification**: `cmake --build build --parallel` should work
3. **Phase 8 Status**: 100% COMPLETE - All 5 sprints finished, production-ready PSQL platform
4. **Current Status**: Phase 9 starting - Index families beyond B-Tree (Hash, Bitmap, GIN, R-Tree)

## Build System Rules (NON-NEGOTIABLE!)

### ❌ NEVER:

- Remove files without removing ALL dependencies
- Make changes without build validation
- Place compiled executables in project root
- Use manual `g++` compilation
- Build tests outside CMake system
- Ignore the `ProjectPlan/` directory

### ✅ ALWAYS:

- Validate build after EVERY change
- Place test sources in `tests/` directory
- Register new tests in `CMakeLists.txt`
- Build with `cmake --build build --target <name>`
- Run tests with `cd build && ctest -R <name>`

## Memory Refresh Protocol

**⚠️  CRITICAL: If context is compacted, immediately check:**

1. 🔴 **`ProjectPlan/Phase_9_Index_Families_TODO.md`** - Current Phase 9 index families implementation
2. 🔴 **`ProjectPlan/Phase_8_Implementation_Plan.md`** - Historical - Phase 8 100% complete documentation
3. 🔴 **Build system**: CMake only, executables in `build/` only
4. 🔴 **Test workflow**: Source in `tests/`, register in `CMakeLists.txt`, build with CMake

## If Ever In Doubt:

1. Read `ProjectPlan/Phase_9_Index_Families_TODO.md` for index family features that need implementation
2. Read `ProjectPlan/BuildSystem.md` for complete build system rules
3. Verify current commit with `git log --oneline -1`
4. Test build with `cmake --build build --parallel`
5. Never deviate from CMake-based build system
6. Never remove files without removing ALL dependencies

## Git Repository

- **GitHub**: https://github.com/DaltonCalford/ScratchBird.git
- **Remote name**: `github` (not `origin`)
- **Current branch**: `main` (recovery completed)
- **Current status**: Phase 9 STARTING - Index families beyond B-Tree (Hash, Bitmap, GIN, R-Tree)

## Current Development Status

### ✅ Phase 8 - PSQL Runtime Engine (100% COMPLETE)
**World-class procedural SQL platform achieved:**
- PSQL execution context with variable scoping ✅
- EXECUTE BLOCK integration in main executor ✅
- Stored procedures and functions (CREATE/EXECUTE) ✅
- Exception handling with RAISE and WHEN clauses ✅
- Cursor operations (DECLARE/OPEN/FETCH/CLOSE) ✅
- Security context management (DEFINER/INVOKER) ✅
- PSQL debugging infrastructure with breakpoints ✅
- Package support with visibility control ✅
- Advanced cursor features (scrollable, FOR loops, bulk operations) ✅
- Enhanced package support (bodies, initialization, state management) ✅
- Function overloading and recursion optimization ✅
- Enhanced development tools (definition/reference search, code completion) ✅
- Performance optimizations (dead code elimination, expression optimization) ✅
- **100% test pass rate (42/42 tests)** ✅

### 🎯 Phase 9 - Index Families and Advanced Options (STARTING)
**Advanced index types beyond B-Tree for specialized workloads:**
- **Hash indexes**: Directory/bucket structure for exact-match lookups
- **Bitmap indexes**: Compressed bitmaps for low-cardinality data
- **GIN indexes**: Generalized inverted indexes for text search and arrays
- **R-Tree indexes**: Spatial rectangle queries and geometric operations
- **INCLUDE columns**: Covering indexes for index-only scans
- **Partial indexes**: WHERE clause predicate enforcement

---

🎉 **MILESTONE ACHIEVED**: ScratchBird has evolved from a simple SQL database into a world-class enterprise application development platform with comprehensive procedural programming capabilities.

## 🚨 CRITICAL UPDATE - August 23, 2025 🚨
**100% Test Suite Achievement**: All 42 tests now passing - comprehensive regression protection established.
**Phase 8 Complete**: Full PSQL application development platform implemented.
**Phase 9 Ready**: Index families and advanced options ready to begin.
