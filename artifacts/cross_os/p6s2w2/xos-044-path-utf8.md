# XOS-044 Path + UTF-8 Normalization
Last-Modified: 2026-02-22

## Implemented
- Runtime config normalization and UTF-8 validation are enforced through:
  - `ServiceController::normalizeConfigPathsAndValidateUtf8(...)`
  - `normalizePathForCurrentPlatform(...)`
  - `validateUtf8Setting(...)`
- Code location:
  - `src/server/service_controller.cpp`

## Validation
- Targeted Linux gate tests (path normalization + UTF-8 rejection):
  - `ServiceControllerListenerBootstrapTest.NormalizeConfigPathsAndValidateUtf8NormalizesKeyRuntimePaths`
  - `ServiceControllerListenerBootstrapTest.NormalizeConfigPathsAndValidateUtf8RejectsInvalidUtf8Fields`
  - Evidence: `artifacts/cross_os/p6s2w2/xos-044-048-ctest.txt`
- Full gate rebuild evidence:
  - Linux configure/build: `artifacts/cross_os/p6s2w2/xos-044-048-linux-configure.txt`
  - Linux configure/build: `artifacts/cross_os/p6s2w2/xos-044-048-linux-build.txt`
  - MinGW configure/build: `artifacts/cross_os/p6s2w2/xos-044-048-mingw-configure.txt`
  - MinGW configure/build: `artifacts/cross_os/p6s2w2/xos-044-048-mingw-build.txt`

