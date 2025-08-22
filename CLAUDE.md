# Claude Memory File for ScratchBird Project

## Project Identity

**ScratchBird** is a SQL database engine transitioning from Alpha to Beta development. Phases 1-7 are 99% complete (comprehensive database functionality). **Currently implementing Phase 8: PSQL Runtime Engine** - adding procedural SQL capabilities (EXECUTE BLOCK, stored procedures, functions, control flow). This transforms ScratchBird from a database into a full application development platform.

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
3. **Phase 8 Status**: Sprint 1 75% complete - PSQL executor integrated, basic tests passing
4. **Current Priority**: Complete control flow execution (IF/WHILE/FOR) and proceed to Sprint 2

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
- **Current status**: Phase 8 Sprint 1 in progress - PSQL runtime implementation

## Current Phase 8 Implementation Status

**✅ SPRINT 1 COMPLETED (75%):**
- PSQL execution context with variable scoping
- EXECUTE BLOCK integration in main executor
- Type management system (PsqlTypeManager)
- Basic control flow framework (IF/WHILE)
- PSQL test suite foundation (`psql_basic_tests.cpp`)

**🔄 SPRINT 1 IN PROGRESS:**
- Control flow execution refinement
- Parser integration for complex EXECUTE BLOCK syntax

**📋 SPRINT 2 PLANNED:**
- Stored procedures (CREATE/EXECUTE PROCEDURE)
- User-defined functions with return values
- Exception handling infrastructure

---

🚨 **CURRENT FOCUS**: Complete Phase 8 Sprint 1 control flow execution, then advance to Sprint 2 stored procedures and functions. This will complete the transition from database to application development platform.
