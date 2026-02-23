# XOS-030 CMake Presets

## Scope
Implemented baseline configure/build/test presets for:
- Linux GCC
- Linux Clang
- Windows MSVC

## Files changed
- `CMakePresets.json`

## Configure presets
- `linux-gcc-debug` (Ninja, `CC=gcc`, `CXX=g++`)
- `linux-clang-debug` (Ninja, `CC=clang`, `CXX=clang++`)
- `windows-msvc-debug` (Visual Studio 2022, x64, condition-gated on Windows host)

## Build presets
- `linux-gcc-debug-build`
- `linux-clang-debug-build`
- `windows-msvc-debug-build`

## Test presets
- `linux-gcc-debug-test`
- `linux-clang-debug-test`
- `windows-msvc-debug-test`

## Verification
- `cmake --list-presets` lists Linux presets on Linux hosts.
- `cmake --preset linux-gcc-debug` configures successfully.
- `cmake --preset linux-clang-debug` configures successfully.
- Windows preset is host-gated and will appear on Windows hosts.
