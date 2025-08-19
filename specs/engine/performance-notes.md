### Engine performance and hardening notes

- Configurable I/O and allocation behavior via `SB_CONFIG`:
  - `direct_io`, `prealloc_mb`, `fsync_policy`, `checksum_policy`.
  - `prefetch_on_alloc` and `prefetch_horizon_pages` to hint kernel on sequential allocations.
- Header I/O fsync is now signal-safe (EINTR retry loop).
- Microbench: `parser_bench_smoke` prints create timings across page sizes.
- Stress scenarios to run manually:
  - Create-drop loops with large `prealloc_mb` to validate allocator and file growth.
  - Concurrent open/read and forced signals during fsync.
- Future: write combining for sequential full-page writes (gather writes into a single aligned vector I/O), per-fsync grouping when `fsync_policy=group`.
