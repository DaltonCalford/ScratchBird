# Build System and Directory Structure

## Directory Layout
```
/workspace/
├── CMakeLists.txt           # Root CMake configuration
├── include/                 # Public headers
│   └── scratchbird/
│       ├── engine.h
│       ├── server.h
│       └── engine/         # Engine subsystem headers
├── src/                     # Implementation files
│   ├── CMakeLists.txt      # Source CMake configuration
│   ├── main.cpp            # Entry point
│   ├── engine/             # Engine implementation
│   ├── audit/              # Audit subsystem
│   ├── telemetry/          # Telemetry subsystem
│   └── trace/              # Tracing subsystem
├── tests/                   # Test suites
│   ├── CMakeLists.txt
│   └── verification_suite/
├── ProjectPlan/            # Phase specifications
│   ├── Phase_XX_NAME.md   # Phase specification
│   └── progress/
│       └── Phase_XX_progress.md  # Progress tracking
└── build/                  # Build output directory
```

## Build Requirements
- CMake 3.20+
- C++17 compiler (GCC 9+ or Clang 10+)
- OpenSSL development libraries
- ZLIB development libraries
- Google Test (for testing)

## Build Commands
```bash
# Configure
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build -j$(nproc)

# Test
ctest --test-dir build --output-on-failure

# Install
cmake --install build --prefix /usr/local
```

## CMake Targets
- `scratchbird` - Main executable
- `scratchbird_engine` - Core engine library
- `test_verification_all` - Complete test suite
- `test_verification_ordered` - Ordered test execution

## Compiler Flags
- `-Wall -Wextra -Werror` - Strict warnings
- `-fsanitize=address,undefined` - Sanitizers (Debug)
- `-O3 -march=native` - Optimization (Release)
- `--coverage` - Code coverage (Debug)