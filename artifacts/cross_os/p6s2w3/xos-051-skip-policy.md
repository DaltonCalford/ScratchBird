# XOS-051 Linux-only Skip Policy
Last-Modified: 2026-02-22

## Policy
Linux-only tests must carry an explicit skip contract through one of:
- `linux_only` label at registration time, and/or
- deterministic name-pattern skip rule used by cross-OS lanes.

## Active Linux-only Rules
- `UnixSocketTest.*`
  - reason: requires `AF_UNIX` local socket behavior.
  - cross-OS skip tag: `skip_unix_socket_ipc`.
- `TSAN_*`
  - reason: TSAN runtime/toolchain lane is Linux-only in this cycle.

## Enforcement Points
- Test partition generator:
  - `scripts/cross_os/generate_test_partition.py`
- Portable lane executor:
  - `scripts/cross_os/run_portable_lane.sh`
- Test registration update:
  - `tests/CMakeLists.txt`

## Evidence
- Partition output:
  - `artifacts/cross_os/p6s2w3/xos-050-test-partition.csv`
- Partition command log:
  - `artifacts/cross_os/p6s2w3/xos-050-partition-summary.txt`
