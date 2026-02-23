# XOS-033 Deterministic Windows Dependency Resolution

## Scope
Integrated deterministic Windows dependency resolution using vcpkg manifest mode with a pinned baseline.

## Added files
- `vcpkg.json`
- `vcpkg-configuration.json`
- `cmake/dependencies/windows_dependency_resolution.cmake`
- `scripts/windows/bootstrap_vcpkg.sh`
- `scripts/windows/bootstrap_vcpkg.ps1`

## Updated files
- `CMakeLists.txt`
- `CMakePresets.json`

## Contract
- Pinned baseline commit: `05442024c3fda64320bd25d2251cc9807b84fb6f`
- Windows configure requires vcpkg toolchain when `SCRATCHBIRD_WINDOWS_REQUIRE_VCPKG=ON`.
- Windows preset `windows-msvc-debug` now resolves through:
  - `$env{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake`
  - `VCPKG_TARGET_TRIPLET=x64-windows`
  - `VCPKG_FEATURE_FLAGS=manifests,registries`

## Verification evidence
- `artifacts/cross_os/p6s2w1/xos-033-presets-list.txt`
- `artifacts/cross_os/p6s2w1/xos-033-vcpkg-manifest.pretty.json`
- `artifacts/cross_os/p6s2w1/xos-033-vcpkg-configuration.pretty.json`

## Notes
- Linux host validation confirms preset/manifest wiring and CMake compatibility.
- Windows package acquisition is performed by bootstrap scripts and validated in dependency parity (`XOS-035`).
