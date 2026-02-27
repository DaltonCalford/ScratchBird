# ScratchBird Build Guide

## 1) Configure

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
```

## 2) Build

```bash
cmake --build build -j"$(nproc)"
```

## 3) Optional helper

```bash
./sb_build --config release
```

## Notes

- Default C++ standard is C++17.
- Build type defaults to `Release` if not set.
- Compiler/parser build is enabled by default (`SCRATCHBIRD_WITH_COMPILER=ON`).

## Build ScratchBird-driver (for compatibility gates)

```bash
cd ../ScratchBird-driver
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
```
