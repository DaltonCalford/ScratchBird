# XOS-016 Auth Bootstrap File Permission Migration
Last-Modified: 2026-02-22

## Migration Summary
Bootstrap token permission validation in auth flow was migrated from direct POSIX `stat` checks to the filesystem permissions adapter.

Updated file:
1. `src/core/auth_provider.cpp`

## Changes
1. Added `file_permissions` adapter include to auth provider.
2. Added singleton adapter accessor for bootstrap checks.
3. Replaced direct `stat`/mode bit checks in `validateBootstrapTokenPermissions()` with:
   - `readMetadata()`
   - regular-file check from adapter metadata
   - `0600` mode check when mode bits are supported
4. Retained Windows behavior through adapter metadata (`mode_supported=false`), avoiding direct platform conditionals in auth code.

## Security Outcome
1. Linux bootstrap token permission policy remains enforced (`0600` required).
2. Permission-policy code path is now centralized through a reusable adapter contract.

## Validation
Focused tests passed:
1. `AuthBootstrapClaimStandaloneTest.*` (1/1)
2. `AuthBootstrapClaimTest.*` (18/18)
3. `FilePermissionsControlTest.*` (2/2)

Evidence:
1. `artifacts/cross_os/p6s1w2/xos-015-016-auth-fs-ctest.txt`
2. `artifacts/cross_os/p6s1w2/xos-015-016-command-log.txt`

## Gate Binding
- Gate: `XOS-GATE-02`
- Tracker row: `XOS-016`
