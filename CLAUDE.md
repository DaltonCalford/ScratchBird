# Claude Memory File for ScratchBird Project

## 🚨 CRITICAL RECOVERY STATUS 🚨

**BUILD SYSTEM RECOVERY IN PROGRESS**
- **Current Status**: Rolled back to commit 548986b (working state)
- **Issue**: Build was broken by improper heap validator removal in commit 0fff208
- **Recovery Doc**: `ProjectPlan/BUILD_BREAK_RECOVERY.md` - **CHECK THIS FIRST!**
- **Date**: 2025-08-21

## Project Identity
**ScratchBird** is a production-ready SQL database engine that was working correctly until Claude-generated commits broke the build system.

## Directory Structure (CRITICAL - Remember This!)

```
ScratchBird/
├── build/                    # 🔴 ALL compiled executables go here (CMake builds)
├── tests/                    # 🔴 ALL test source files (.cpp) go here
├── src/engine/               # Core database engine source
├── include/scratchbird/      # Header files
├── ProjectPlan/              # 🔴 CHECK THIS FIRST - Project documentation
│   ├── BUILD_BREAK_RECOVERY.md  # 🚨 CRITICAL - Current recovery status
│   ├── BuildSystem.md        # Complete build system rules
│   ├── claude_todo.md        # Implementation status
│   └── [Phase planning docs]
└── CMakeLists.txt            # Build configuration
```

## 🚨 CRITICAL RECOVERY INSTRUCTIONS 🚨

**IF CONTEXT IS COMPACTED - DO THIS FIRST:**

1. **Read `ProjectPlan/BUILD_BREAK_RECOVERY.md`** - Contains full recovery plan
2. **Current working commit**: 548986b (ALTER TABLE implementation)
3. **Build verification**: `cmake --build build --parallel` should work
4. **DO NOT modify heap validator files** - they are working correctly
5. **Follow re-implementation plan** step by step with validation

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
- **Check `ProjectPlan/BUILD_BREAK_RECOVERY.md` for current status**

## What Went Wrong

**Commit 0fff208 broke the system by:**
- Removing `include/scratchbird/engine/heap_validator.h`
- Removing `src/engine/heap_validator.cpp`
- BUT leaving `src/dbcheck.cpp` that depends on heap_validator.h
- BUT leaving CMakeLists.txt targets that try to build dbcheck
- **Result**: Build system completely broken

## Recovery Status (As of August 21, 2025)

**✅ COMPLETED:**
- Root cause analysis of build failure
- Rollback to working commit 548986b
- Build verification - system now compiles correctly
- Recovery plan documented in BUILD_BREAK_RECOVERY.md

**🔄 IN PROGRESS:**
- Re-implementing beneficial Claude changes with proper validation
- Following established project procedures
- Maintaining working build state throughout process

## Key Implementation Files at Working State
- `src/engine/heap_validator.cpp` - ✅ PRESENT and working
- `include/scratchbird/engine/heap_validator.h` - ✅ PRESENT and working
- `src/dbcheck.cpp` - ✅ PRESENT and compiles correctly
- All test executables build successfully

## Memory Refresh Protocol

**⚠️  CRITICAL: If context is compacted, immediately check:**
1. 🔴 **`ProjectPlan/BUILD_BREAK_RECOVERY.md`** - MOST IMPORTANT - current recovery status
2. 🔴 **Check `ProjectPlan/` directory first** - contains all current status
3. 🔴 **Build system**: CMake only, executables in `build/` only
4. 🔴 **Test workflow**: Source in `tests/`, register in `CMakeLists.txt`, build with CMake
5. 🔴 **Current state**: System rolled back, recovery plan documented, re-implementation in progress

## If Ever In Doubt:
1. Read `ProjectPlan/BUILD_BREAK_RECOVERY.md` for current recovery status
2. Read `ProjectPlan/BuildSystem.md` for build rules (if it exists)
3. Verify current commit with `git log --oneline -1`
4. Test build with `cmake --build build --parallel`
5. Never deviate from CMake-based build system
6. Never remove files without removing ALL dependencies

## Git Repository
- **GitHub**: https://github.com/DaltonCalford/ScratchBird.git
- **Remote name**: `github` (not `origin`)
- **Current branch**: Detached HEAD at 548986b (working state)
- **Recovery branch**: Will be created as `proper-claude-implementation`

---

🚨 **REMEMBER**: The system was broken by Claude changes. Recovery is in progress. Always check BUILD_BREAK_RECOVERY.md first to understand current status and next steps.
