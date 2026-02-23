# XOS-006 Dependency Strategy and Version Contract
Last-Modified: 2026-02-22

## Source Baseline
Dependency declarations reviewed from:
1. `CMakeLists.txt`
2. `src/CMakeLists.txt`

## Core Dependency Contract
### Required
1. CMake `>= 3.20`
2. C++17 compiler
3. Zlib (`find_package(ZLIB REQUIRED)`)
4. nlohmann/json `v3.11.3` (FetchContent in root CMake)

### Security/crypto
1. OpenSSL (`find_package(OpenSSL)`) for secure random and crypto features.
2. `libcrypt` support where available; fallback path is not accepted as security baseline for release packaging.

### Optional feature dependencies
1. LZ4
2. ZSTD
3. GEOS
4. PROJ
5. libxml2

## Windows Dependency Resolution Policy
1. Preferred Windows package manager for native MSVC builds: `vcpkg` manifest mode.
2. Cross-compile (Linux -> Windows MinGW) uses pinned MinGW sysroot packages matching vcpkg versions where feasible.
3. Optional dependencies may be disabled with explicit build flags when unavailable, but required dependencies may not be skipped.

## Version Pin Policy
1. `nlohmann/json` remains pinned to `v3.11.3` until explicit upgrade row is added.
2. Build manifests must capture exact dependency versions and hashes per target profile.
3. Linux and Windows dependency manifests must be committed in-tree for reproducibility.

## ABI and Linkage Policy
1. Use consistent CRT/link profile per Windows target lane.
2. Avoid mixing debug and release CRT variants in the same artifact.
3. Record static vs dynamic linkage decisions per dependency in build manifest evidence.

## Gate Binding
- Gate: `XOS-GATE-01`
- Tracker row: `XOS-006`
