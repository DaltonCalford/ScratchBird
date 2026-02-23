# Cross-OS Linux/Windows Build and Test Flow
Last modified: 2026-02-22

[Back to Installation Index](index.md) | [Back to Documentation Index](../index.md)

---

## Scope

This guide defines the in-tree workflow for Linux x64 and Windows x64 (native and cross-compiled) in `0.1.0`.

## Configure + Build Presets

```bash
# Linux GCC
cmake --preset linux-gcc-debug
cmake --build --preset linux-gcc-debug-build --parallel

# Linux Clang
cmake --preset linux-clang-debug
cmake --build --preset linux-clang-debug-build --parallel

# Linux -> Windows x64 (MinGW)
scripts/cross_os/bootstrap_mingw_zlib.sh
scripts/cross_os/bootstrap_mingw_openssl.sh
cmake --preset linux-mingw-windows-x64
cmake --build --preset linux-mingw-windows-x64-build --parallel
```

## Portable Test Lanes

```bash
# Linux portable lane
scripts/cross_os/run_portable_lane.sh --lane portable --test-preset linux-gcc-debug-test

# Linux-only lane
scripts/cross_os/run_portable_lane.sh --lane linux_only --test-preset linux-gcc-debug-test

# Windows portable lane (run on Windows runner/host)
scripts/cross_os/run_portable_lane.sh --lane windows_portable --test-preset windows-msvc-debug-test
```

Linux-only exclusions currently include `UnixSocketTest.*` and `TSAN_*`.

## CI Workflows

- `.github/workflows/cross-os-gate.yml`
  - Linux GCC + Linux Clang + Windows MSVC + Linux->Windows MinGW
  - Aggregated gate job: `Cross OS Required Checks`
- `.github/workflows/cross-os-nightly.yml`
  - nightly extended suite
  - nightly performance sample lane

## Artifact Locations

- Cross-OS gate evidence:
  - `artifacts/cross_os/`
- Linux->Windows binaries:
  - `build/linux-mingw-windows-x64/src/*.exe`
  - `build/linux-mingw-windows-x64/tools/*.exe`

