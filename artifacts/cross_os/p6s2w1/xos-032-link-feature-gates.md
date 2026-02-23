# XOS-032 Link and Feature Gate Normalization

## Scope
Normalized platform link options/libraries and introduced explicit platform feature compile flags.

## Files changed
- `CMakeLists.txt`
- `src/CMakeLists.txt`

## Link normalization
- `scratchbird_link_group` now applies `-Wl,--no-as-needed` only on non-Windows hosts.
- Network platform libraries are consolidated via `SCRATCHBIRD_PLATFORM_NETWORK_LIBS`.
  - `ws2_32` on Windows
  - empty on POSIX
- Server platform libraries are consolidated via `SCRATCHBIRD_PLATFORM_SERVER_LIBS`.
  - `ws2_32 advapi32` on Windows
  - `${CMAKE_DL_LIBS}` on POSIX

## Feature flags added
Global compile definitions in root CMake now declare:
- `SCRATCHBIRD_PLATFORM_WINDOWS`
- `SCRATCHBIRD_PLATFORM_POSIX`
- `SCRATCHBIRD_FEATURE_SYSTEMD_RUNTIME`
- `SCRATCHBIRD_FEATURE_POSIX_DAEMON`

## Verification
- Full rebuild succeeded: `cmake --build build -j4`.
- Runtime portability subset passed:
  - `PortableRuntimeGuard`
  - `SignalControlTest.*`
  - `ServiceControllerListenerBootstrapTest.*`
