# ScratchBird

Modernized C/C++ refactor workspace for Firebird-related experiments and tooling.

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build
```

## Layout
- `src/`: library and app sources
- `include/`: public headers
- `tests/`: unit tests (CTest + GoogleTest or Catch2)
- `cmake/`: CMake modules

## Requirements
- CMake 3.20+
- gcc/clang toolchain
