# XOS-034 Linux Feature Gates

## Scope
Added explicit CMake feature gates for Linux-only runtime surfaces (systemd hooks and fork-based runtime).

## Added/updated behavior
- New options in `CMakeLists.txt`:
  - `SCRATCHBIRD_ENABLE_SYSTEMD_RUNTIME`
  - `SCRATCHBIRD_ENABLE_FORK_RUNTIME`
- Platform safety rules:
  - On Windows, both options must remain `OFF` (hard configure error if enabled).
- Compile definitions now derived from gate values:
  - `SCRATCHBIRD_FEATURE_SYSTEMD_RUNTIME`
  - `SCRATCHBIRD_FEATURE_POSIX_DAEMON`
- Server link gate in `src/CMakeLists.txt`:
  - `${CMAKE_DL_LIBS}` linked only when `SCRATCHBIRD_ENABLE_SYSTEMD_RUNTIME=ON`.

## Validation
- Gate override configure test:
  - `cmake --preset linux-gcc-debug -DSCRATCHBIRD_ENABLE_SYSTEMD_RUNTIME=OFF -DSCRATCHBIRD_ENABLE_FORK_RUNTIME=ON`
- Output captured in:
  - `artifacts/cross_os/p6s2w1/xos-034-feature-gates-configure.txt`
- Restored default Linux gate state (systemd ON, fork ON):
  - `artifacts/cross_os/p6s2w1/xos-034-feature-gates-restore.txt`
