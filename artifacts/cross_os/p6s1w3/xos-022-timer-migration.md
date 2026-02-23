# XOS-022 Scheduler And Retry Timing Migration

Last-Modified: 2026-02-22

## Scope
Migrated scheduler/retry timing paths in `JobScheduler` to the timer adapter.

## Code Changes
- Updated `include/scratchbird/core/job_scheduler.h`
  - Added `ClockControl` dependency and owned adapter member
- Updated `src/core/job_scheduler.cpp`
  - Scheduler constructors now initialize `clock_control_`
  - `runLoop()` now derives wait deadlines from adapter monotonic time
  - `processDueJobs()` and job timestamp code now use adapter realtime milliseconds
  - `executeJobNow()` poll-loop timeout and pacing now use adapter monotonic/sleep APIs
  - `executeJobRun()` execution timing, pre-execute delay, timeout checks, and latency metrics now use adapter APIs
  - `runExternalCommand(...)` timeout/cancel/kill-grace timing now uses adapter monotonic time via injected pointer

## Validation
- Build: `cmake --build build -j4`
- Tests: `ctest --test-dir build -R 'JobScheduler|ClockControlTest' --output-on-failure`
- Results: `artifacts/cross_os/p6s1w3/xos-021-022-clock-ctest.txt`
- Migration surface: `artifacts/cross_os/p6s1w3/xos-021-022-clock-surface.txt`

## Result
`XOS-022` completed.
