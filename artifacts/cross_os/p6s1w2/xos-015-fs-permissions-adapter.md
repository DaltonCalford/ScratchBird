# XOS-015 Filesystem Permissions Adapter
Last-Modified: 2026-02-22

## Implementation Summary
New filesystem permissions abstraction added:
1. `include/scratchbird/core/file_permissions.h`
2. `src/core/file_permissions.cpp`

Adapter contract:
1. `readMetadata(path, FileMetadata*)`
2. `setMode(path, mode_bits)`

Metadata coverage:
1. `exists`
2. `is_regular`
3. `mode_supported`
4. `mode_bits`

## Platform Behavior
### Linux/POSIX
1. Metadata uses `stat`.
2. Mode writes use `chmod`.
3. Mode bits are reported as POSIX `0777` mask.

### Windows
1. Metadata uses filesystem status.
2. `mode_supported=false` (ACL mode mapping not represented as POSIX bits).
3. `setMode` returns `Status::NOT_SUPPORTED`.

## Validation
Focused tests added and passed:
1. `FilePermissionsControlTest.ReadMetadataForExistingFile`
2. `FilePermissionsControlTest.SetModeAndReadBack` (POSIX only)

Evidence:
1. `artifacts/cross_os/p6s1w2/xos-015-016-auth-fs-ctest.txt`
2. `artifacts/cross_os/p6s1w2/xos-015-016-command-log.txt`

## Gate Binding
- Gate: `XOS-GATE-02`
- Tracker row: `XOS-015`
