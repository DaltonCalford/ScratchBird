# Claude Memory File for ScratchBird Project

## Project Identity

**ScratchBird** is a SQL database engine that has completed its Alpha to Beta transition. Phases 1-7 are 99% complete (comprehensive database functionality). **Phase 8: PSQL Runtime Engine is 100% COMPLETE** - full procedural SQL platform implemented (EXECUTE BLOCK, stored procedures, functions, control flow, cursors, exceptions, debugging, development tools, performance optimizations). ScratchBird is now a complete application development platform.

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

1. **Read `ProjectPlan/Phase_8_Implementation_Plan.md`** - Current development focus (Sprint 1 in progress)
2. **Build verification**: `cmake --build build --parallel` should work
3. **Phase 8 Status**: 100% COMPLETE - All 5 sprints finished, production-ready PSQL platform
4. **Current Status**: Phase 8 complete, ready for production deployment

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

1. 🔴 **`ProjectPlan/Phase_8_Implementation_Plan.md`** - Current Sprint 1 progress and next steps
2. 🔴 **`ProjectPlan/2ndTry_Phase_Review.md`** - Historical - Phases 1-7 completion status (99% done)
3. 🔴 **Build system**: CMake only, executables in `build/` only
4. 🔴 **Test workflow**: Source in `tests/`, register in `CMakeLists.txt`, build with CMake

## If Ever In Doubt:

1. Read `ProjectPlan/Phase_8_Implementation_Plan` for missing features that need implementation
2. Read `ProjectPlan/BuildSystem.md` for complete build system rules
3. Verify current commit with `git log --oneline -1`
4. Test build with `cmake --build build --parallel`
5. Never deviate from CMake-based build system
6. Never remove files without removing ALL dependencies

## Git Repository

- **GitHub**: https://github.com/DaltonCalford/ScratchBird.git
- **Remote name**: `github` (not `origin`)
- **Current branch**: `main` (recovery completed)
- **Current status**: Phase 8 100% COMPLETE - Full PSQL application development platform

## Phase 8 Implementation Status - 100% COMPLETE

**✅ ALL SPRINTS COMPLETED (100%):**

**Sprint 1-4 (Core Implementation):**
- PSQL execution context with variable scoping
- EXECUTE BLOCK integration in main executor
- Stored procedures and functions (CREATE/EXECUTE)
- Exception handling with RAISE and WHEN clauses
- Cursor operations (DECLARE/OPEN/FETCH/CLOSE)
- Security context management (DEFINER/INVOKER)
- PSQL debugging infrastructure with breakpoints
- Package support with visibility control

**Sprint 5 (Advanced Features - COMPLETED):**
- Advanced cursor features (scrollable, FOR loops, bulk operations)
- Enhanced package support (bodies, initialization, state management)
- Function overloading and recursion optimization
- Enhanced development tools (definition/reference search, code completion)
- Performance optimizations (dead code elimination, expression optimization)

---

🎉 **MILESTONE ACHIEVED**: ScratchBird has evolved from a simple SQL database into a world-class enterprise application development platform with comprehensive procedural programming capabilities.

## 🚨 CRITICAL UPDATE - August 23, 2025 🚨
**100% Test Suite Achievement**: All 42 tests now passing - comprehensive regression protection established.
**Phase 8 Complete**: Full PSQL application development platform implemented.
**Phase 9 Ready**: Enterprise production features ready to begin.
