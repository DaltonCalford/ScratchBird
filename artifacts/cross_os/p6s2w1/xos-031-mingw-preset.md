# XOS-031 MinGW Cross-Compile Preset

## Scope
Added Linux-to-Windows MinGW-w64 cross-compile preset and toolchain file.

## Files changed
- `CMakePresets.json`
- `cmake/toolchains/mingw-w64-x86_64.cmake`

## Preset added
- Configure: `linux-mingw-windows-x64`
- Build: `linux-mingw-windows-x64-build`

## Verification
- `cmake --list-presets` includes `linux-mingw-windows-x64`.
- `cmake --preset linux-mingw-windows-x64` reaches toolchain configuration and fails at dependency discovery with:
  - `Could NOT find ZLIB (missing: ZLIB_LIBRARY ZLIB_INCLUDE_DIR)`

## Result
- Preset wiring is complete.
- Cross-compile dependency parity is pending and tracked for dependency normalization/resolution work (`XOS-033`/`XOS-035`).
