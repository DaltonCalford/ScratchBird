# Claude Memory File for ScratchBird Project

## Project Identity

**ScratchBird** is a SQL database engine in Alpha development. The system was previously corrupted by Claude-generated commits that improperly removed heap validator files, breaking the build system. The system has been recovered and is now functional, but significant work remains to complete all required features before reaching Beta testing phase.

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

1. **Read `ProjectPlan/2ndTry_Phase_Review.md`** - Contains gap analysis of missing features that need implementation
2. **Build verification**: `cmake --build build --parallel` should work (system is recovered)
3. **DO NOT modify heap validator files** - they are working correctly
4. **Focus on implementing missing features** identified in the gap analysis

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

## Key Implementation Files at Working State

- `src/engine/heap_validator.cpp` - ✅ PRESENT and working
- `include/scratchbird/engine/heap_validator.h` - ✅ PRESENT and working
- `src/dbcheck.cpp` - ✅ PRESENT and compiles correctly
- All test executables build successfully

## Memory Refresh Protocol

**⚠️  CRITICAL: If context is compacted, immediately check:**

1. 🔴 **`ProjectPlan/2ndTry_Phase_Review.md`** - MOST IMPORTANT - identifies missing features to implement
2. 🔴 **Check `ProjectPlan/` directory first** - contains all project documentation and current development focus
3. 🔴 **Build system**: CMake only, executables in `build/` only
4. 🔴 **Test workflow**: Source in `tests/`, register in `CMakeLists.txt`, build with CMake

## If Ever In Doubt:

1. Read `ProjectPlan/2ndTry_Phase_Review.md` for missing features that need implementation
2. Read `ProjectPlan/BuildSystem.md` for complete build system rules
3. Verify current commit with `git log --oneline -1`
4. Test build with `cmake --build build --parallel`
5. Never deviate from CMake-based build system
6. Never remove files without removing ALL dependencies

## Git Repository

- **GitHub**: https://github.com/DaltonCalford/ScratchBird.git
- **Remote name**: `github` (not `origin`)
- **Current branch**: `main` (recovery completed)
- **Current status**: Build system recovered, focus on implementing missing features

---

🚨 **REMEMBER**: System was previously corrupted by improper Claude changes but has been recovered. Current focus: implement missing features identified in gap analysis to progress from Alpha to Beta phase.
