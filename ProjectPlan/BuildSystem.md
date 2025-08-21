# ScratchBird Build System Documentation

## Overview
ScratchBird uses CMake as its build system with CTest integration for comprehensive testing. All build artifacts are contained within the `build/` directory to maintain a clean project structure.

## Directory Structure

```
ScratchBird/
├── build/                  # All compiled executables and build artifacts
│   ├── test_*              # Test executables (built by CMake)
│   ├── libscratchbird_engine.a
│   └── [other build artifacts]
├── tests/                  # All test source files (.cpp)
│   ├── engine_api_tests.cpp
│   ├── executor_tests.cpp
│   ├── test_union_simple.cpp
│   ├── test_create_view.cpp
│   └── [40+ other test files]
├── src/                    # Main source code
│   └── engine/             # Core database engine
├── include/                # Header files
│   └── scratchbird/
├── ProjectPlan/            # Project documentation and planning
│   ├── BuildSystem.md      # This file
│   ├── claude_todo.md      # Implementation status
│   └── [phase planning docs]
└── CMakeLists.txt          # Build configuration
```

## Build System Rules

### ❌ NEVER DO THIS:
- Place compiled executables in project root
- Build tests manually with `g++` commands
- Create executables outside the `build/` directory

### ✅ ALWAYS DO THIS:
- Place test source files in `tests/` directory
- Register new tests in `CMakeLists.txt` with `add_executable()` and `add_test()`
- Build using CMake: `cmake --build build --target <test_name>`
- Run tests using CTest: `cd build && ctest -R <test_name>`

## Test Development Workflow

### 1. Create Test Source File
```bash
# Create in tests/ directory
vim tests/my_new_test.cpp
```

### 2. Register in CMakeLists.txt
```cmake
add_executable(my_new_test tests/my_new_test.cpp)
target_link_libraries(my_new_test scratchbird_engine)
add_test(NAME my_new_test COMMAND my_new_test)
```

### 3. Build with CMake
```bash
cmake --build build --target my_new_test
```

### 4. Run with CTest
```bash
cd build && ctest -R my_new_test --verbose
```

## CMake Configuration

The project uses CMake 3.20+ with the following key settings:
- **C++ Standard**: C++17
- **Build Type**: Configurable (Debug/Release)
- **Test Framework**: CTest integration
- **Engine Library**: `scratchbird_engine` (static library)

### Key CMake Targets:
- `scratchbird_engine` - Core database engine library
- `<test_name>` - Individual test executables (40+ tests)
- `all` - Build all targets

### Test Registration Pattern:
```cmake
add_executable(<test_name> tests/<test_name>.cpp)
target_link_libraries(<test_name> scratchbird_engine)
add_test(NAME <test_name> COMMAND <test_name>)
```

## Build Directory Contents

After building, the `build/` directory contains:
```
build/
├── CMakeCache.txt
├── CMakeFiles/
├── libscratchbird_engine.a
├── engine_api_tests        # Test executables
├── executor_tests
├── test_union_simple
├── test_create_view
├── [40+ other test executables]
└── Testing/               # CTest results
```

## Project Conventions

1. **Source Organization**: All source files organized by purpose (`src/`, `tests/`, `include/`)
2. **Build Isolation**: All build artifacts contained in `build/` directory
3. **CMake Integration**: All executables built through CMake, not manual compilation
4. **Test Framework**: CTest for standardized test execution and reporting
5. **Clean Structure**: No compiled artifacts in source directories

## Troubleshooting

### If executables appear in wrong location:
```bash
# Wrong: executable in project root
./my_test  # ❌ Should not exist

# Correct: executable in build directory
./build/my_test  # ✅ Correct location
```

### If CMake doesn't recognize new test:
```bash
# Reconfigure CMake after adding new test to CMakeLists.txt
cd build && cmake .. && make <test_name>
```

### If test isn't registered with CTest:
- Verify `add_test(NAME <test_name> COMMAND <test_name>)` is in `CMakeLists.txt`
- Reconfigure: `cd build && cmake ..`
- List tests: `cd build && ctest --list-tests`

## Historical Context

This build system supports ScratchBird's comprehensive test suite of 40+ tests covering:
- Heap storage and corruption testing
- Transaction visibility and deadlock detection
- SQL execution and optimization
- Constraint and trigger systems
- Set operations (UNION/INTERSECT/EXCEPT)
- View creation and management

The CMake configuration ensures consistent, reproducible builds across all components of this production-ready database engine.
