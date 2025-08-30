# Claude Memory File for ScratchBird Project

## Project Identity

**ScratchBird** is a SQL database engine that has successfully completed its Alpha to Beta transition. **Phase 8: PSQL Runtime Engine is 100% COMPLETE** - world-class procedural SQL platform implemented (EXECUTE BLOCK, stored procedures, functions, control flow, cursors, exceptions, debugging, development tools, performance optimizations). **Phase 9: Advanced Index Families is 100% COMPLETE** - implementing 7 production index types (B-Tree, Hash, Bitmap, GIN, R-Tree, LSM-Tree, Columnstore) with enterprise architecture.

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

1. **Read `ProjectPlan/Phase_11.7_Implementation_Progress_Tracker.md***
2. **Build verification**: `cmake --build build --parallel` should work

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

## If Ever In Doubt:

1. Read `ProjectPlan/BuildSystem.md` for complete build system rules
2. Verify current commit with `git log --oneline -1`
3. Test build with `cmake --build build --para$llel`$
4. Never deviate from CMake-based build system
5. Never remove files without removing ALL dependencies

## Git Repository

- **GitHub**: https://github.com/DaltonCalford/ScratchBird.git
- **Remote name**: `github` (not `origin`)
- **Current branch**: `main` (recovery completed)
- **Current status**: Phase 9 100% COMPLETE - Enterprise Index families implemented

## Current Development Status

### ✅ Phase 11 in progress

### ✅ Phase 10 Complete

### ✅ Phase 9 Complete

### ✅ Phase 8 Complete

### ✅ Phase 7 Complete

### ✅ Phase 6 Complete

### ✅ Phase 5 Complete

### ✅ Phase 4 Complete

### ✅ Phase 3 Complete

### ✅ Phase 2 Complete

### ✅ Phase 1 Complete
