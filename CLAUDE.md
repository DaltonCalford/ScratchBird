# Claude Memory File for ScratchBird Project

## 🎯 CRITICAL: Always Check ProjectPlan/ Directory First!

**When working on ScratchBird, ALWAYS check the `ProjectPlan/` directory for the most up-to-date documentation and project context.**

Key files to reference:
- `ProjectPlan/BuildSystem.md` - Complete build system documentation
- `ProjectPlan/claude_todo.md` - Current implementation status and completed features
- `ProjectPlan/Phase_*.md` - Detailed phase planning and requirements

## Project Identity
**ScratchBird** is a production-ready SQL database engine with 100% completion of Phase 1-7 goals.

## Directory Structure (CRITICAL - Remember This!)

```
ScratchBird/
├── build/                    # 🔴 ALL compiled executables go here (CMake builds)
├── tests/                    # 🔴 ALL test source files (.cpp) go here
├── src/engine/               # Core database engine source
├── include/scratchbird/      # Header files
├── ProjectPlan/              # 🔴 CHECK THIS FIRST - Project documentation
│   ├── BuildSystem.md        # Complete build system rules
│   ├── claude_todo.md        # Implementation status
│   └── [Phase planning docs]
└── CMakeLists.txt            # Build configuration
```

## Build System Rules (NON-NEGOTIABLE!)

### ❌ NEVER:
- Place compiled executables in project root
- Use manual `g++` compilation
- Build tests outside CMake system
- Ignore the `ProjectPlan/` directory

### ✅ ALWAYS:
- Place test sources in `tests/` directory
- Register new tests in `CMakeLists.txt`
- Build with `cmake --build build --target <name>`
- Run tests with `cd build && ctest -R <name>`
- **Check `ProjectPlan/BuildSystem.md` for detailed workflow**

## Project Status (As of January 2025)
🎉 **100% COMPLETE** - All Phase 1-7 features implemented:

**Completed in Latest Work:**
- ✅ UNION/INTERSECT/EXCEPT set operations (full implementation)
- ✅ CREATE VIEW support (catalog integration)
- ✅ FK SET DEFAULT (verified working correctly)

**Production Features:**
- SERIALIZABLE isolation with MVCC
- Complete constraint system (PK, UNIQUE, FK, CHECK)
- Advanced trigger engine with WHEN clauses
- Hash joins and window functions
- Statistics and cost-based optimization
- ALTER TABLE operations
- Comprehensive WAL and recovery system

## Test Suite
- **40+ test files** in `tests/` directory
- **All executables** built by CMake into `build/` directory
- **CTest integration** for standardized test execution
- **Comprehensive coverage** of all database subsystems

## Key Implementation Files
- `src/engine/executor.cpp` - SQL execution engine (set ops, views, FK handling)
- `src/engine/parser.cpp` - SQL parsing (SELECT routing, DDL detection)
- `include/scratchbird/engine/ast.h` - AST definitions (SelectQuery, DdlView)
- `src/engine/catalog_manager.cpp` - Schema and view management

## Memory Refresh Protocol

**When context gets compacted, remember:**
1. 🔴 **Check `ProjectPlan/` directory first** - contains all current status
2. 🔴 **Build system**: CMake only, executables in `build/` only
3. 🔴 **Test workflow**: Source in `tests/`, register in `CMakeLists.txt`, build with CMake
4. 🔴 **Project status**: 100% complete, production-ready database engine
5. 🔴 **Recent work**: Set operations, CREATE VIEW, FK SET DEFAULT all implemented

## If Ever In Doubt:
1. Read `ProjectPlan/BuildSystem.md` for build rules
2. Read `ProjectPlan/claude_todo.md` for current implementation status
3. Follow established patterns from existing 40+ test files
4. Never deviate from CMake-based build system

## Git Repository
- **GitHub**: https://github.com/DaltonCalford/ScratchBird.git
- **Remote name**: `github` (not `origin`)
- **Main branch**: `main`

---

🚨 **REMEMBER**: This is a complete, production-ready database engine. Any new work should maintain the high quality standards and established architectural patterns. Always verify changes against the comprehensive test suite!
